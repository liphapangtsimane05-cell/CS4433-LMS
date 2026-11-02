#include "../include/db.hpp"
#include "../include/model.hpp"
#include "../include/security.hpp"
#include "../include/service.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
int checks=0;
void ok(bool yes,const std::string& why){++checks;if(!yes)throw std::runtime_error("FAILED: "+why);}
template<class F>void rejects(F action,const std::string& why){bool threw=false;try{action();}catch(const std::exception&){threw=true;}ok(threw,why);}
long count(lms::Db& db,const std::string& sql){lms::Statement q(db,sql);ok(q.row(),"SQL count returned row");return static_cast<long>(q.integer(0));}
void testWorkflows(){
 lms::Db db(":memory:");db.loadSchema(std::string(LMS_SOURCE_DIR)+"/sql/schema.sql");
 std::string clock="2026-11-01T12:00:00Z";
 lms::LmsService app(db,std::make_unique<lms::DailyTenPercentPenalty>(),[&clock](){return clock;});
 ok(!app.anyUsers(),"database initially empty");
 app.bootstrapAdmin("admin","System Admin","Admin-Pass-2026!");
 rejects([&]{app.bootstrapAdmin("admin2","Admin 2","Admin-Pass-2026!");},"bootstrap cannot create second initial account");
 auto admin=app.login("admin","Admin-Pass-2026!");
 ok(admin.role==lms::Role::ADMIN,"admin login");
 rejects([&]{app.login("admin","wrong-password");},"wrong password rejected");
 auto dep=app.createDepartment(admin,"CS","Computer Science");auto dep2=app.createDepartment(admin,"MATH","Mathematics");
 auto lecturerId=app.createUser(admin,"teacher","Teacher One",lms::Role::LECTURER,dep,"Lecturer-2026!");
 auto studentId=app.createUser(admin,"pupil","Student One",lms::Role::STUDENT,dep,"Student-2026!");
 auto hodId=app.createUser(admin,"hod","HoD One",lms::Role::HOD,dep,"Hod-Password!");
 app.createUser(admin,"hod2","HoD Two",lms::Role::HOD,dep2,"Hod-Password!");
 auto lecturer=app.login("teacher","Lecturer-2026!");auto student=app.login("pupil","Student-2026!");auto hod=app.login("hod","Hod-Password!");auto hod2=app.login("hod2","Hod-Password!");
 ok(hod.userId==hodId,"correct HoD identity");
 ok(student.userId==studentId,"correct student identity");
 auto course=app.createCourse(admin,dep,"CS101","Intro to CS");
 rejects([&]{app.createAssignment(student,course,"Wrong","x","2026-11-03T00:00:00Z",100);},"role-based access");
 rejects([&]{app.createAssignment(lecturer,course,"Wrong","x","2026-11-03T00:00:00Z",100);},"course teaching assignment required");
 app.assignLecturer(admin,lecturerId,course);app.enrolStudent(admin,studentId,course);
 auto assignment=app.createAssignment(lecturer,course,"Essay","Write a paper","2026-11-03T00:00:00Z",100);
 ok(app.list(student,"assignments").size()==2,"student sees assigned course assignment");
 auto submission1=app.submit(student,assignment,"First answer");auto submission2=app.submit(student,assignment,"Corrected answer");
 ok(submission2>submission1,"resubmission saved as new row");
 ok(count(db,"SELECT COUNT(*) FROM submissions")==2,"all attempts retained");
 rejects([&]{app.mark(lecturer,submission1,80,"Good","Initial");},"only latest attempt is markable");
 auto mark=app.mark(lecturer,submission2,85,"Clear work","Initial assessment");
 ok(mark>0,"mark created");
 ok(app.list(student,"results").size()==1,"pending marks hidden from student");
 ok(count(db,"SELECT COUNT(*) FROM mark_audit")==1,"initial assessment is audited");
 rejects([&]{app.review(hod2,mark,true,"" );},"other department HoD denied");
 app.review(hod,mark,false,"Correct citation format");
 rejects([&]{app.release(hod,mark);},"cannot release returned mark");
 app.mark(lecturer,submission2,90,"Updated feedback","After moderation correction");
 ok(count(db,"SELECT COUNT(*) FROM mark_audit")==2,"mark change audited");
 ok(app.list(admin,"audit").size()==3,"admin can inspect every mark change");
 ok(app.list(hod,"audit").size()==3,"HoD can inspect mark changes in their department");
 ok(app.list(hod2,"audit").size()==1,"HoD cannot inspect other department mark changes");
 rejects([&]{app.list(student,"audit");},"students cannot inspect confidential mark audit");
 app.review(hod,mark,true,"Reviewed");
 ok(app.list(student,"results").size()==1,"approved but unreleased hidden");
 app.release(hod,mark);
 ok(app.list(student,"results").size()==2,"released mark becomes visible");
 ok(count(db,"SELECT COUNT(*) FROM mark_releases")==1,"one release logged");
 rejects([&]{app.mark(lecturer,submission2,95,"No","Attempt to modify released mark");},"released mark is immutable");
 ok(std::abs(lms::DailyTenPercentPenalty{}.calculate(75,100,2)-55)<0.0001,"10 percent max times two days");
 ok(lms::DailyTenPercentPenalty{}.calculate(99,100,4)==0,"after three days zero");
 // An individual extension changes the deadline for its one eligible student.
 auto extra=app.createAssignment(lecturer,course,"Second essay","Write more","2026-11-01T16:00:00Z",100);
 auto ext=app.grantExtension(lecturer,extra,studentId,"2026-11-04T00:00:00Z","Medical reason");
 ok(ext>0,"extension saved");
 clock="2026-11-03T00:00:00Z";
 auto extendedSub=app.submit(student,extra,"After original due, within extension");
 auto extendedMark=app.mark(lecturer,extendedSub,80,"Within extension","Initial assessment");
 lms::Statement ex(db,"SELECT final_mark,penalty_mark FROM marks WHERE mark_id=?");ex.bind(1,extendedMark);ok(ex.row()&&ex.decimal(0)==80&&ex.decimal(1)==0,"extension prevents late penalty");
 // A true late submission is accepted but penalised during marking.
 clock="2026-11-01T00:00:00Z";
 auto lateAssignment=app.createAssignment(lecturer,course,"Late example","Work","2026-11-01T01:00:00Z",100);
 clock="2026-11-02T02:00:00Z"; // 25 hours late: ceil=>two late days
 auto lateSub=app.submit(student,lateAssignment,"Late example answer");auto lateMark=app.mark(lecturer,lateSub,70,"Work is late","Initial assessment");
 lms::Statement lateRow(db,"SELECT final_mark,penalty_mark FROM marks WHERE mark_id=?");lateRow.bind(1,lateMark);
 ok(lateRow.row()&&std::abs(lateRow.decimal(0)-50)<0.0001,"25 hours late incurs 2-day penalty");
 clock="2026-11-01T00:00:00Z";
 auto over=app.createAssignment(lecturer,course,"Overdue","Work","2026-11-01T01:00:00Z",100);
 clock="2026-11-05T02:00:00Z";
 auto overdueSub=app.submit(student,over,"Very late answer");auto overdueMark=app.mark(lecturer,overdueSub,99,"Too late","Initial assessment");
 lms::Statement overdueRow(db,"SELECT final_mark FROM marks WHERE mark_id=?");overdueRow.bind(1,overdueMark);
 ok(overdueRow.row()&&overdueRow.decimal(0)==0,"over three days gives zero");
 rejects([&]{app.submit(student,assignment,"Illegal late resubmission");},"late resubmission blocked");
 rejects([&]{app.grantExtension(lecturer,assignment,studentId,"2026-10-01T00:00:00Z","Backdate");},"extension must extend original deadline");
 rejects([&]{lms::isoUtc("2026-02-30T00:00:00Z");},"invalid date rejected");
 rejects([&]{app.list(student,"users");},"cannot list admin user records as student");
 app.deactivateUser(admin,studentId);
 rejects([&]{app.login("pupil","Student-2026!");},"inactive account denied");
 // Password hashes are salted and not stored as plaintext.
 lms::Statement hashed(db,"SELECT password_hash FROM users WHERE user_id=?");hashed.bind(1,studentId);
 ok(hashed.row()&&hashed.text(0).find("Student-2026!")==std::string::npos,"plaintext password not stored");
}
void testPersistence(){
 namespace fs=std::filesystem;auto path=fs::temp_directory_path()/"cs4433_week4_persistence_test.db";std::error_code ignored;fs::remove(path,ignored);
 {
  lms::Db db(path.string());db.loadSchema(std::string(LMS_SOURCE_DIR)+"/sql/schema.sql");lms::LmsService app(db);
  app.bootstrapAdmin("persist","Persistence Admin","Persist-Secret-2026!");
 }
 {
  lms::Db db(path.string());db.loadSchema(std::string(LMS_SOURCE_DIR)+"/sql/schema.sql");lms::LmsService app(db);
  ok(app.login("persist","Persist-Secret-2026!").role==lms::Role::ADMIN,"data persists across process/database restart");
 }
 fs::remove(path,ignored);
}
}
int main(){try{testWorkflows();testPersistence();std::cout<<"PASS: "<<checks<<" LMS integration and security assertions\n";return 0;}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
