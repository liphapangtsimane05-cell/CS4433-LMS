#include "../include/service.hpp"
#include "../include/security.hpp"
#include "../include/repository.hpp"
#include <openssl/rand.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace lms {
namespace {
void check(bool ok,const std::string& msg){if(!ok)throw std::invalid_argument(msg);}
void nonblank(const std::string& v,const std::string& label){check(!v.empty()&&v.find_first_not_of(" \t\r\n")!=std::string::npos,label+" is required");}
void done(Statement& s){if(s.row())throw DbError("Unexpected result from write statement");}
std::string randHex(){unsigned char b[12];if(RAND_bytes(b,sizeof(b))!=1)throw std::runtime_error("Random generator failed");static const char* digits="0123456789abcdef";std::string result;result.reserve(24);for(auto n:b){result.push_back(digits[n>>4]);result.push_back(digits[n&15]);}return result;}
std::string cleanFileName(const std::string& name){std::string clean;for(char c:name)if(std::isalnum(static_cast<unsigned char>(c))||c=='_'||c=='-'||c=='.')clean.push_back(c);if(clean.empty())clean="attachment";return clean.substr(0,72);}
std::vector<std::vector<std::string>> gather(Db& db,const std::string& sql,const std::vector<Id>& args,const std::vector<std::string>& headers){
 Statement s(db,sql);for(size_t i=0;i<args.size();++i)s.bind(static_cast<int>(i+1),args[i]);
 std::vector<std::vector<std::string>> rows{headers};
 while(s.row()){
  std::vector<std::string> vals;vals.reserve(headers.size());
  // all list selections explicitly CAST to TEXT / coalesce for predictable output
  for(size_t i=0;i<headers.size();++i)vals.push_back(s.isNull(static_cast<int>(i))?"(none)":s.text(static_cast<int>(i)));
  rows.push_back(std::move(vals));
 }
 return rows;
}
}
std::string utcNow(){
 auto now=std::chrono::system_clock::now();std::time_t t=std::chrono::system_clock::to_time_t(now);
 std::tm tm{};
#ifdef _WIN32
 gmtime_s(&tm,&t);
#else
 gmtime_r(&t,&tm);
#endif
 std::ostringstream oss;oss<<std::put_time(&tm,"%Y-%m-%dT%H:%M:%SZ");return oss.str();
}
std::string isoUtc(const std::string& value){
 static const std::regex pattern(R"(^([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2})Z$)");
 std::smatch m;check(std::regex_match(value,m,pattern),"Date must be UTC, format YYYY-MM-DDTHH:MM:SSZ");
 int y=std::stoi(m[1]),mo=std::stoi(m[2]),d=std::stoi(m[3]),h=std::stoi(m[4]),mi=std::stoi(m[5]),s=std::stoi(m[6]);
 int days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};bool leap=(y%4==0&& (y%100!=0||y%400==0));
 check(y>=2000&&y<=2200&&mo>=1&&mo<=12,"Year/month out of range");if(leap)days[2]=29;
 check(d>=1&&d<=days[mo]&&h<24&&mi<60&&s<60,"Invalid calendar date/time");return value;
}
Role parseRole(const std::string& value){
 if(value=="ADMIN")return Role::ADMIN;
 if(value=="LECTURER")return Role::LECTURER;
 if(value=="HOD")return Role::HOD;
 if(value=="STUDENT")return Role::STUDENT;
 throw std::invalid_argument("Role must be ADMIN, LECTURER, HOD or STUDENT");
}
std::string roleName(Role role){switch(role){case Role::ADMIN:return "ADMIN";case Role::LECTURER:return "LECTURER";case Role::HOD:return "HOD";case Role::STUDENT:return "STUDENT";}throw std::invalid_argument("Unknown role");}
bool Administrator::can(const std::string& a)const{return a=="admin";}
bool Lecturer::can(const std::string& a)const{return a=="teach";}
bool HeadOfDepartment::can(const std::string& a)const{return a=="moderate";}
bool Student::can(const std::string& a)const{return a=="study";}
std::unique_ptr<User> makeUser(Id id,const std::string& name,Role role){
 switch(role){case Role::ADMIN:return std::make_unique<Administrator>(id,name);case Role::LECTURER:return std::make_unique<Lecturer>(id,name);case Role::HOD:return std::make_unique<HeadOfDepartment>(id,name);case Role::STUDENT:return std::make_unique<Student>(id,name);}
 throw std::invalid_argument("Invalid user role");
}
double DailyTenPercentPenalty::calculate(double raw,double maximum,int days)const{
 check(std::isfinite(raw)&&std::isfinite(maximum)&&raw>=0&&maximum>0&&raw<=maximum&&days>=0,"Invalid mark or late days");
 if(days>3)return 0;
 return std::max(0.,raw-0.10*maximum*static_cast<double>(days));
}
LmsService::LmsService(Db& db,std::unique_ptr<ILatePenaltyPolicy> p,std::function<std::string()> clk):db_(db),policy_(std::move(p)),clock_(std::move(clk)){
 check(static_cast<bool>(policy_)&&static_cast<bool>(clock_),"Policy and clock are required");
}
bool LmsService::anyUsers(){Statement s(db_,"SELECT count(*) FROM users");return s.row()&&s.integer(0)>0;}
void LmsService::bootstrapAdmin(const std::string& username,const std::string& fullname,const std::string& password){
 nonblank(username,"Username");nonblank(fullname,"Full name");
 Transaction tx(db_);check(!anyUsers(),"Administrator bootstrap is disabled after first user");
 Statement s(db_,"INSERT INTO users(username,password_hash,full_name,role,department_id) VALUES(?,?,?,'ADMIN',NULL)");
 s.bind(1,username);s.bind(2,security::hashPassword(password));s.bind(3,fullname);done(s);tx.commit();
}
Session LmsService::login(const std::string& username,const std::string& password){
 Statement s(db_,"SELECT user_id,password_hash,role,coalesce(department_id,0),active FROM users WHERE username=? COLLATE NOCASE");s.bind(1,username);
 if(!s.row())throw std::runtime_error("Invalid username or password");
 const Id id=s.integer(0),dept=s.integer(3);const std::string stored=s.text(1),role=s.text(2);const bool active=s.integer(4)==1;
 if(!active||!security::verifyPassword(password,stored))throw std::runtime_error("Invalid username or password");
 return Session{id,parseRole(role),dept,username};
}
void LmsService::ensureActive(const Session& s)const{
 Statement st(db_,"SELECT role,coalesce(department_id,0),active FROM users WHERE user_id=?");st.bind(1,s.userId);
 check(st.row()&&st.text(0)==roleName(s.role)&&st.integer(1)==s.departmentId&&st.integer(2)==1,"Session invalid or account deactivated; sign in again");
}
void LmsService::require(const Session& s,Role role)const{ensureActive(s);auto actor=makeUser(s.userId,s.username,s.role);
 const std::string action=role==Role::ADMIN?"admin":role==Role::LECTURER?"teach":role==Role::HOD?"moderate":"study";
 check(actor->can(action)&&actor->role()==role,"Access denied: incorrect role");}
void LmsService::ensureLecturerCourse(const Session& s,Id course)const{
 require(s,Role::LECTURER);
 Statement st(db_,"SELECT count(*) FROM lecturer_courses WHERE lecturer_id=? AND course_id=?");st.bind(1,s.userId);st.bind(2,course);
 check(st.row()&&st.integer(0)>0,"Lecturer not assigned to this course");
}
void LmsService::ensureStudentEnrolled(const Session& s,Id course)const{
 require(s,Role::STUDENT);Statement st(db_,"SELECT count(*) FROM enrolments WHERE student_id=? AND course_id=?");st.bind(1,s.userId);st.bind(2,course);
 check(st.row()&&st.integer(0)>0,"Student not enrolled in this course");
}
void LmsService::ensureHodDepartment(const Session& s,Id mark)const{
 require(s,Role::HOD);
 Statement st(db_,"SELECT c.department_id FROM marks m JOIN submissions sub ON sub.submission_id=m.submission_id JOIN assignments a ON a.assignment_id=sub.assignment_id JOIN courses c ON c.course_id=a.course_id WHERE m.mark_id=?");st.bind(1,mark);
 check(st.row()&&st.integer(0)==s.departmentId,"HoD cannot moderate outside own department");
}
Id LmsService::createDepartment(const Session& s,const std::string& code,const std::string& name){
 require(s,Role::ADMIN);nonblank(code,"Department code");nonblank(name,"Department name");
 Statement st(db_,"INSERT INTO departments(department_code,department_name) VALUES(?,?)");st.bind(1,code);st.bind(2,name);done(st);return db_.lastId();
}
void LmsService::updateDepartment(const Session& s,Id id,const std::string& name){
 require(s,Role::ADMIN);nonblank(name,"Department name");
 Statement st(db_,"UPDATE departments SET department_name=? WHERE department_id=?");st.bind(1,name);st.bind(2,id);done(st);
 check(db_.changes()>0,"Department not found");
}
Id LmsService::createUser(const Session& s,const std::string& username,const std::string& full,Role role,Id dept,const std::string& password){
 require(s,Role::ADMIN);nonblank(username,"Username");nonblank(full,"Full name");
 check(role==Role::ADMIN||dept>0,"Non-admin accounts require a department");
 Statement st(db_,"INSERT INTO users(username,password_hash,full_name,role,department_id) VALUES(?,?,?,?,?)");
 st.bind(1,username);st.bind(2,security::hashPassword(password));st.bind(3,full);st.bind(4,roleName(role));if(dept>0)st.bind(5,dept);else st.bindNull(5);done(st);return db_.lastId();
}
void LmsService::updateUser(const Session& s,Id target,const std::string& full,Id dept){
 require(s,Role::ADMIN);nonblank(full,"Full name");
 Statement checkRole(db_,"SELECT role FROM users WHERE user_id=?");checkRole.bind(1,target);check(checkRole.row(),"User not found");check(checkRole.text(0)=="ADMIN"||dept>0,"Non-admin users must have a department");
 Statement st(db_,"UPDATE users SET full_name=?,department_id=? WHERE user_id=?");st.bind(1,full);if(dept>0)st.bind(2,dept);else st.bindNull(2);st.bind(3,target);done(st);
 check(db_.changes()>0,"User not found or unchanged");
}
void LmsService::deactivateUser(const Session& s,Id target){
 require(s,Role::ADMIN);check(target!=s.userId,"Cannot deactivate your own active admin session");
 Statement st(db_,"UPDATE users SET active=0 WHERE user_id=? AND active=1");st.bind(1,target);done(st);check(db_.changes()>0,"User not found or already inactive");
}
void LmsService::resetPassword(const Session& s,Id target,const std::string& password){
 require(s,Role::ADMIN);Statement st(db_,"UPDATE users SET password_hash=? WHERE user_id=?");st.bind(1,security::hashPassword(password));st.bind(2,target);done(st);check(db_.changes()>0,"User not found");
}
Id LmsService::createCourse(const Session& s,Id dept,const std::string& code,const std::string& title){
 require(s,Role::ADMIN);nonblank(code,"Course code");nonblank(title,"Course title");
 Statement st(db_,"INSERT INTO courses(department_id,course_code,course_title) VALUES(?,?,?)");st.bind(1,dept);st.bind(2,code);st.bind(3,title);done(st);return db_.lastId();
}
void LmsService::updateCourse(const Session& s,Id course,const std::string& title,bool active){
 require(s,Role::ADMIN);nonblank(title,"Course title");
 Statement st(db_,"UPDATE courses SET course_title=?,active=? WHERE course_id=?");st.bind(1,title);st.bind(2,static_cast<Id>(active));st.bind(3,course);done(st);check(db_.changes()>0,"Course not found or unchanged");
}
void LmsService::assignLecturer(const Session& s,Id lecturer,Id course){
 require(s,Role::ADMIN);
 Statement st(db_,"INSERT INTO lecturer_courses(lecturer_id,course_id) VALUES(?,?)");st.bind(1,lecturer);st.bind(2,course);done(st);
}
void LmsService::enrolStudent(const Session& s,Id student,Id course){
 require(s,Role::ADMIN);
 Statement st(db_,"INSERT INTO enrolments(student_id,course_id) VALUES(?,?)");st.bind(1,student);st.bind(2,course);done(st);
}
Id LmsService::createAssignment(const Session& s,Id course,const std::string& title,const std::string& description,const std::string& deadline,double maximum){
 ensureLecturerCourse(s,course);nonblank(title,"Assignment title");check(std::isfinite(maximum)&&maximum>0&&maximum<=100000,"Invalid maximum mark");isoUtc(deadline);
 Statement active(db_,"SELECT active FROM courses WHERE course_id=?");active.bind(1,course);check(active.row()&&active.integer(0)==1,"Course inactive or missing");
 Statement st(db_,"INSERT INTO assignments(course_id,created_by,title,description,due_at,maximum_mark) VALUES(?,?,?,?,?,?)");st.bind(1,course);st.bind(2,s.userId);st.bind(3,title);st.bind(4,description);st.bind(5,deadline);st.bind(6,maximum);done(st);return db_.lastId();
}
Id LmsService::grantExtension(const Session& s,Id assignment,Id student,const std::string& deadline,const std::string& reason){
 isoUtc(deadline);nonblank(reason,"Extension reason");
 Statement course(db_,"SELECT course_id FROM assignments WHERE assignment_id=?");course.bind(1,assignment);check(course.row(),"Assignment not found");ensureLecturerCourse(s,course.integer(0));
 Statement st(db_,"INSERT INTO individual_extensions(assignment_id,student_id,granted_by,revised_due_at,reason) VALUES(?,?,?,?,?)");st.bind(1,assignment);st.bind(2,student);st.bind(3,s.userId);st.bind(4,deadline);st.bind(5,reason);done(st);return db_.lastId();
}
Id LmsService::grantClassExtension(const Session& s,Id assignment,const std::string& deadline,const std::string& reason){
 isoUtc(deadline);nonblank(reason,"Class extension reason");
 Statement course(db_,"SELECT course_id,due_at FROM assignments WHERE assignment_id=?");course.bind(1,assignment);
 check(course.row(),"Assignment not found");Id courseId=course.integer(0);const auto original=course.text(1);
 ensureLecturerCourse(s,courseId);
 check(deadline>original,"Class extension must be later than original deadline");
 Statement existing(db_,"SELECT revised_due_at FROM class_extensions WHERE assignment_id=?");existing.bind(1,assignment);
 if(existing.row())check(deadline>=existing.text(0),"Cannot shorten a published class extension");
 Statement st(db_,"INSERT INTO class_extensions(assignment_id,granted_by,revised_due_at,reason) VALUES(?,?,?,?) ON CONFLICT(assignment_id) DO UPDATE SET granted_by=excluded.granted_by,revised_due_at=excluded.revised_due_at,reason=excluded.reason,granted_at=strftime('%Y-%m-%dT%H:%M:%fZ','now')");
 st.bind(1,assignment);st.bind(2,s.userId);st.bind(3,deadline);st.bind(4,reason);done(st);
 return assignment;
}
Id LmsService::submit(const Session& s,Id assignment,const std::string& content,const std::string& inputPath){
 require(s,Role::STUDENT);
 check(!content.empty()||!inputPath.empty(),"Enter submission text or supply a local file");
 const std::string date=isoUtc(clock_());
 Statement ctx(db_,"SELECT a.course_id,a.due_at,a.published,c.active,a.maximum_mark FROM assignments a JOIN courses c ON c.course_id=a.course_id WHERE assignment_id=?");ctx.bind(1,assignment);
 check(ctx.row(),"Assignment not found");Id course=ctx.integer(0);std::string deadline=ctx.text(1);check(ctx.integer(2)==1&&ctx.integer(3)==1,"Assignment unavailable");
 ensureStudentEnrolled(s,course);
 Statement ext(db_,"SELECT MAX(revised_due_at) FROM (SELECT revised_due_at FROM individual_extensions WHERE assignment_id=? AND student_id=? UNION ALL SELECT revised_due_at FROM class_extensions WHERE assignment_id=?)");ext.bind(1,assignment);ext.bind(2,s.userId);ext.bind(3,assignment);if(ext.row()&&!ext.isNull(0)&&ext.text(0)>deadline)deadline=ext.text(0);
 Statement count(db_,"SELECT count(*) FROM submissions WHERE assignment_id=? AND student_id=?");count.bind(1,assignment);count.bind(2,s.userId);check(count.row(),"Cannot determine attempt count");int attempts=static_cast<int>(count.integer(0));
 Statement delta(db_,"SELECT (julianday(?) - julianday(?))*86400.0");delta.bind(1,date);delta.bind(2,deadline);check(delta.row()&&!delta.isNull(0),"Invalid submission timestamps");double seconds=delta.decimal(0);
 if(attempts>0)check(seconds<=0,"Resubmissions allowed only before effective deadline");
 const int days=seconds<=0?0:static_cast<int>(std::ceil((seconds-0.0001)/86400.0));
 const std::string status=days>3?"OVER_THREE_DAYS":(days>0?"LATE":"RECEIVED");
 std::string savedPath;
 if(!inputPath.empty()){
  namespace fs=std::filesystem;
  const fs::path source(inputPath);
  check(fs::is_regular_file(source),"Attached file does not exist or is not a regular file");
  const auto size=fs::file_size(source);check(size>0&&size<=10*1024*1024,"Attachment must be 1 byte to 10 MiB");
  const fs::path destDir="data/submissions";fs::create_directories(destDir);
  const fs::path target=destDir/(randHex()+"_"+cleanFileName(source.filename().string()));
  check(fs::copy_file(source,target,fs::copy_options::none),"Could not copy attachment to managed storage");
  savedPath=target.generic_string();
 }
 try{
  Transaction tx(db_);
  Statement st(db_,"INSERT INTO submissions(assignment_id,student_id,attempt_number,submission_text,file_path,submitted_at,applicable_due_at,late_days,submission_status) VALUES(?,?,?,?,?,?,?,?,?)");
  st.bind(1,assignment);st.bind(2,s.userId);st.bind(3,static_cast<Id>(attempts+1));st.bind(4,content);
  if(savedPath.empty())st.bindNull(5);else st.bind(5,savedPath);
  st.bind(6,date);st.bind(7,deadline);st.bind(8,static_cast<Id>(days));st.bind(9,status);done(st);
  Id id=db_.lastId();tx.commit();return id;
 }catch(...){if(!savedPath.empty()){std::error_code ignored;std::filesystem::remove(savedPath,ignored);}throw;}
}
Id LmsService::mark(const Session& s,Id submission,double raw,const std::string& feedback,const std::string& reason){
 require(s,Role::LECTURER);nonblank(feedback,"Feedback");nonblank(reason,"Audit reason");check(std::isfinite(raw)&&raw>=0,"Raw mark must be nonnegative");
 SQLiteSubmissionRepository repo(db_);
 const SubmissionContext ctx=repo.context(submission);
 ensureLecturerCourse(s,ctx.courseId);check(raw<=ctx.maximumMark,"Raw mark exceeds assignment maximum");
 check(repo.isLatest(submission),"Only the latest attempt may be marked");
 const double max=ctx.maximumMark;const int late=ctx.lateDays;
 const double final=policy_->calculate(raw,max,late);const double penalty=late>3?raw:std::max(0.,raw-final);
 const std::string now=isoUtc(clock_());
 Transaction tx(db_);
 Statement old(db_,"SELECT mark_id,raw_mark,final_mark,status FROM marks WHERE submission_id=?");old.bind(1,submission);
 Id markId=0;bool existed=old.row();double oldRaw=0,oldFinal=0;
 if(existed){markId=old.integer(0);oldRaw=old.decimal(1);oldFinal=old.decimal(2);const std::string status=old.text(3);check(status=="PENDING_MODERATION"||status=="CORRECTION_REQUIRED","Cannot edit approved or released mark");
  Statement edit(db_,"UPDATE marks SET raw_mark=?,penalty_mark=?,final_mark=?,feedback=?,marked_by=?,status='PENDING_MODERATION',updated_at=? WHERE mark_id=?");
  edit.bind(1,raw);edit.bind(2,penalty);edit.bind(3,final);edit.bind(4,feedback);edit.bind(5,s.userId);edit.bind(6,now);edit.bind(7,markId);done(edit);
 }else{
  Statement insert(db_,"INSERT INTO marks(submission_id,marked_by,raw_mark,penalty_mark,final_mark,feedback,status,marked_at,updated_at) VALUES(?,?,?,?,?,?,'PENDING_MODERATION',?,?)");
  insert.bind(1,submission);insert.bind(2,s.userId);insert.bind(3,raw);insert.bind(4,penalty);insert.bind(5,final);insert.bind(6,feedback);insert.bind(7,now);insert.bind(8,now);done(insert);markId=db_.lastId();
 }
 Statement audit(db_,"INSERT INTO mark_audit(mark_id,changed_by,old_raw_mark,new_raw_mark,old_final_mark,new_final_mark,reason,changed_at) VALUES(?,?,?,?,?,?,?,?)");
 audit.bind(1,markId);audit.bind(2,s.userId);if(existed)audit.bind(3,oldRaw);else audit.bindNull(3);audit.bind(4,raw);if(existed)audit.bind(5,oldFinal);else audit.bindNull(5);audit.bind(6,final);audit.bind(7,reason);audit.bind(8,now);done(audit);
 tx.commit();return markId;
}
void LmsService::review(const Session& s,Id mark,bool approve,const std::string& reason){
 ensureHodDepartment(s,mark);if(!approve)nonblank(reason,"Correction reason");
 Statement candidate(db_,"SELECT submission_id FROM marks WHERE mark_id=?");candidate.bind(1,mark);check(candidate.row(),"Mark not found");
 SQLiteSubmissionRepository repo(db_);check(repo.isLatest(candidate.integer(0)),"Cannot moderate a superseded submission attempt");
 Transaction tx(db_);
 Statement state(db_,"SELECT status FROM marks WHERE mark_id=?");state.bind(1,mark);check(state.row()&&(state.text(0)=="PENDING_MODERATION"||state.text(0)=="CORRECTION_REQUIRED"),"Mark not awaiting moderation");
 Statement decision(db_,"INSERT INTO moderation_decisions(mark_id,hod_id,decision,reason,decided_at) VALUES(?,?,?,?,?)");decision.bind(1,mark);decision.bind(2,s.userId);decision.bind(3,approve?"APPROVE":"RETURN_FOR_CORRECTION");decision.bind(4,reason);decision.bind(5,isoUtc(clock_()));done(decision);
 Statement update(db_,"UPDATE marks SET status=?,updated_at=? WHERE mark_id=?");update.bind(1,approve?"APPROVED":"CORRECTION_REQUIRED");update.bind(2,isoUtc(clock_()));update.bind(3,mark);done(update);tx.commit();
}
void LmsService::release(const Session& s,Id mark){
 ensureHodDepartment(s,mark);
 Statement candidate(db_,"SELECT submission_id FROM marks WHERE mark_id=?");candidate.bind(1,mark);check(candidate.row(),"Mark not found");
 SQLiteSubmissionRepository repo(db_);check(repo.isLatest(candidate.integer(0)),"Cannot release a superseded submission attempt");
 Transaction tx(db_);
 Statement latest(db_,"SELECT decision FROM moderation_decisions WHERE mark_id=? ORDER BY decision_id DESC LIMIT 1");latest.bind(1,mark);
 check(latest.row()&&latest.text(0)=="APPROVE","Latest HoD decision must approve release");
 Statement state(db_,"SELECT status FROM marks WHERE mark_id=?");state.bind(1,mark);check(state.row()&&state.text(0)=="APPROVED","Mark not approved or already released");
 Statement st(db_,"INSERT INTO mark_releases(mark_id,released_by,released_at) VALUES(?,?,?)");st.bind(1,mark);st.bind(2,s.userId);st.bind(3,isoUtc(clock_()));done(st);tx.commit();
}
std::vector<std::vector<std::string>> LmsService::list(const Session& s,const std::string& view){
 ensureActive(s);
 using V=std::vector<std::string>;
 if(s.role==Role::ADMIN){
  if(view=="audit")return gather(db_,"SELECT CAST(a.audit_id AS TEXT),CAST(a.mark_id AS TEXT),u.username,coalesce(CAST(a.old_raw_mark AS TEXT),'(initial)'),CAST(a.new_raw_mark AS TEXT),coalesce(CAST(a.old_final_mark AS TEXT),'(initial)'),CAST(a.new_final_mark AS TEXT),a.reason,a.changed_at FROM mark_audit a JOIN users u ON u.user_id=a.changed_by ORDER BY a.audit_id DESC",{},V{"Audit ID","Mark ID","Changed by","Old raw","New raw","Old final","New final","Reason","UTC"});
  if(view=="users")return gather(db_,"SELECT CAST(user_id AS TEXT),username,full_name,role,coalesce(CAST(department_id AS TEXT),'—'),CAST(active AS TEXT) FROM users ORDER BY user_id",{},V{"ID","Username","Name","Role","Dept","Active"});
  if(view=="departments")return gather(db_,"SELECT CAST(department_id AS TEXT),department_code,department_name FROM departments ORDER BY department_code",{},V{"ID","Code","Department"});
  if(view=="courses")return gather(db_,"SELECT CAST(course_id AS TEXT),course_code,course_title,CAST(department_id AS TEXT),CAST(active AS TEXT) FROM courses ORDER BY course_id",{},V{"ID","Code","Title","Dept","Active"});
  if(view=="enrolments")return gather(db_,"SELECT CAST(student_id AS TEXT),CAST(course_id AS TEXT) FROM enrolments ORDER BY student_id",{},V{"Student ID","Course ID"});
 }
 if(s.role==Role::LECTURER){
  if(view=="courses")return gather(db_,"SELECT CAST(c.course_id AS TEXT),c.course_code,c.course_title FROM courses c JOIN lecturer_courses lc ON lc.course_id=c.course_id WHERE lc.lecturer_id=? ORDER BY c.course_code",{s.userId},V{"ID","Course","Title"});
  if(view=="assignments")return gather(db_,"SELECT CAST(a.assignment_id AS TEXT),c.course_code,a.title,a.due_at,CAST(a.maximum_mark AS TEXT) FROM assignments a JOIN courses c ON c.course_id=a.course_id JOIN lecturer_courses lc ON lc.course_id=c.course_id WHERE lc.lecturer_id=? ORDER BY a.assignment_id",{s.userId},V{"ID","Course","Title","Deadline UTC","Max"});
  if(view=="submissions")return gather(db_,"SELECT CAST(s.submission_id AS TEXT),CAST(s.assignment_id AS TEXT),CAST(s.student_id AS TEXT),CAST(s.attempt_number AS TEXT),s.submission_status,CAST(s.late_days AS TEXT),s.submission_text,coalesce(s.file_path,'—') FROM submissions s JOIN assignments a ON a.assignment_id=s.assignment_id JOIN lecturer_courses lc ON lc.course_id=a.course_id WHERE lc.lecturer_id=? ORDER BY s.submission_id",{s.userId},V{"ID","Assignment","Student","Attempt","Status","Late days","Text","File"});
  if(view=="marks")return gather(db_,"SELECT CAST(m.mark_id AS TEXT),CAST(m.submission_id AS TEXT),CAST(m.final_mark AS TEXT),m.status FROM marks m JOIN submissions s ON s.submission_id=m.submission_id JOIN assignments a ON a.assignment_id=s.assignment_id JOIN lecturer_courses lc ON lc.course_id=a.course_id WHERE lc.lecturer_id=? ORDER BY m.mark_id",{s.userId},V{"Mark ID","Submission","Final","Status"});
 }
 if(s.role==Role::STUDENT){
  if(view=="courses")return gather(db_,"SELECT CAST(c.course_id AS TEXT),c.course_code,c.course_title FROM courses c JOIN enrolments e ON e.course_id=c.course_id WHERE e.student_id=? ORDER BY c.course_code",{s.userId},V{"ID","Code","Title"});
  if(view=="assignments")return gather(db_,"SELECT CAST(a.assignment_id AS TEXT),c.course_code,a.title,MAX(a.due_at,coalesce(x.revised_due_at,a.due_at),coalesce(cx.revised_due_at,a.due_at)),CAST(a.maximum_mark AS TEXT) FROM assignments a JOIN courses c ON c.course_id=a.course_id JOIN enrolments e ON e.course_id=c.course_id LEFT JOIN individual_extensions x ON x.assignment_id=a.assignment_id AND x.student_id=e.student_id LEFT JOIN class_extensions cx ON cx.assignment_id=a.assignment_id WHERE e.student_id=? AND a.published=1 AND c.active=1 ORDER BY a.due_at",{s.userId},V{"ID","Course","Title","Effective deadline UTC","Max"});
  if(view=="submissions")return gather(db_,"SELECT CAST(submission_id AS TEXT),CAST(assignment_id AS TEXT),CAST(attempt_number AS TEXT),submitted_at,submission_status FROM submissions WHERE student_id=? ORDER BY submission_id",{s.userId},V{"ID","Assignment","Attempt","Submitted UTC","Status"});
  if(view=="results")return gather(db_,"SELECT CAST(assignment_id AS TEXT),CAST(submission_id AS TEXT),CAST(final_mark AS TEXT),feedback,released_at FROM student_released_results WHERE student_id=? ORDER BY assignment_id",{s.userId},V{"Assignment","Submission","Final mark","Feedback","Released UTC"});
 }
 if(s.role==Role::HOD){
  if(view=="audit")return gather(db_,"SELECT CAST(a.audit_id AS TEXT),CAST(a.mark_id AS TEXT),u.username,coalesce(CAST(a.old_raw_mark AS TEXT),'(initial)'),CAST(a.new_raw_mark AS TEXT),coalesce(CAST(a.old_final_mark AS TEXT),'(initial)'),CAST(a.new_final_mark AS TEXT),a.reason,a.changed_at FROM mark_audit a JOIN users u ON u.user_id=a.changed_by JOIN marks m ON m.mark_id=a.mark_id JOIN submissions sub ON sub.submission_id=m.submission_id JOIN assignments ass ON ass.assignment_id=sub.assignment_id JOIN courses c ON c.course_id=ass.course_id WHERE c.department_id=? ORDER BY a.audit_id DESC",{s.departmentId},V{"Audit ID","Mark ID","Changed by","Old raw","New raw","Old final","New final","Reason","UTC"});
  if(view=="queue")return gather(db_,"SELECT CAST(m.mark_id AS TEXT),CAST(s.submission_id AS TEXT),c.course_code,CAST(s.student_id AS TEXT),CAST(m.final_mark AS TEXT),m.status,m.feedback FROM marks m JOIN submissions s ON s.submission_id=m.submission_id JOIN assignments a ON a.assignment_id=s.assignment_id JOIN courses c ON c.course_id=a.course_id WHERE c.department_id=? ORDER BY m.mark_id",{s.departmentId},V{"Mark ID","Submission","Course","Student","Final","Status","Feedback"});
  if(view=="decisions")return gather(db_,"SELECT CAST(d.decision_id AS TEXT),CAST(d.mark_id AS TEXT),d.decision,d.reason,d.decided_at FROM moderation_decisions d JOIN marks m ON m.mark_id=d.mark_id JOIN submissions s ON s.submission_id=m.submission_id JOIN assignments a ON a.assignment_id=s.assignment_id JOIN courses c ON c.course_id=a.course_id WHERE c.department_id=? ORDER BY d.decision_id",{s.departmentId},V{"ID","Mark","Decision","Reason","UTC"});
 }
 throw std::invalid_argument("Unknown or forbidden view: "+view);
}
}
