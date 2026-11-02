#include "../include/service.hpp"
#include "../include/security.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
using lms::Id;using lms::Session;using lms::LmsService;
std::string prompt(const std::string& label){std::cout<<label<<": "<<std::flush;std::string result;if(!std::getline(std::cin,result))throw std::runtime_error("Input ended");return result;}
Id number(const std::string& label){const std::string text=prompt(label);std::size_t used=0;try{auto n=std::stoll(text,&used);if(used!=text.size()||n<0)throw std::invalid_argument("invalid");return n;}catch(const std::exception&){throw std::invalid_argument("Enter a nonnegative whole number for "+label);}}
double decimal(const std::string& label){const std::string text=prompt(label);std::size_t used=0;try{double d=std::stod(text,&used);if(used!=text.size())throw std::invalid_argument("invalid");return d;}catch(const std::exception&){throw std::invalid_argument("Enter a valid decimal for "+label);}}
void show(const std::vector<std::vector<std::string>>& rows){
 if(rows.size()<2){std::cout<<"(No records)\n";return;}
 for(size_t i=0;i<rows.size();++i){for(size_t j=0;j<rows[i].size();++j){if(j)std::cout<<" | ";std::cout<<rows[i][j];}std::cout<<'\n';if(i==0)std::cout<<"--------------------------------------------------------------\n";}
}
void adminMenu(LmsService& a,const Session& s){
 std::cout<<"\nADMIN | 1 Departments  2 Users  3 Courses  4 Assign lecturer  5 Enrol student\n"
             "        6 Update user  7 Deactivate  8 Reset password  9 Update course\n"
             "       10 View users  11 View courses  12 View departments  13 Update department  14 View enrolments  15 Mark audit  0 Logout\n";
 Id choice=number("Action");if(choice==0)throw 0;
 switch(choice){
 case 1:{auto code=prompt("Code");auto name=prompt("Name");auto id=a.createDepartment(s,code,name);std::cout<<"Department ID: "<<id<<'\n';break;}
 case 2:{auto user=prompt("Username");auto name=prompt("Full name");auto role=lms::parseRole(prompt("Role [ADMIN/LECTURER/HOD/STUDENT]"));Id dept=role==lms::Role::ADMIN?0:number("Department ID");auto pwd=lms::security::readSecret("Initial password (10+ chars): ");auto id=a.createUser(s,user,name,role,dept,pwd);std::cout<<"User ID: "<<id<<'\n';break;}
 case 3:{Id dept=number("Department ID");auto code=prompt("Course code");auto name=prompt("Course title");std::cout<<"Course ID: "<<a.createCourse(s,dept,code,name)<<'\n';break;}
 case 4:{auto lecturer=number("Lecturer ID");auto course=number("Course ID");a.assignLecturer(s,lecturer,course);std::cout<<"Lecturer assigned\n";break;}
 case 5:{auto student=number("Student ID");auto course=number("Course ID");a.enrolStudent(s,student,course);std::cout<<"Student enrolled\n";break;}
 case 6:{auto id=number("User ID");auto name=prompt("New full name");auto dept=number("Department ID (0 if admin)");a.updateUser(s,id,name,dept);std::cout<<"User updated\n";break;}
 case 7:a.deactivateUser(s,number("User ID"));std::cout<<"User deactivated\n";break;
 case 8:{auto id=number("User ID");auto pass=lms::security::readSecret("New password (10+ chars): ");a.resetPassword(s,id,pass);std::cout<<"Password changed\n";break;}
 case 9:{auto id=number("Course ID");auto title=prompt("New title");auto active=number("Active [1/0]");if(active>1)throw std::invalid_argument("Active must be 0 or 1");a.updateCourse(s,id,title,active==1);std::cout<<"Course updated\n";break;}
 case 10:show(a.list(s,"users"));break;
 case 11:show(a.list(s,"courses"));break;
 case 12:show(a.list(s,"departments"));break;
 case 13:{auto id=number("Department ID");auto name=prompt("New department name");a.updateDepartment(s,id,name);std::cout<<"Department updated\n";break;}
 case 14:show(a.list(s,"enrolments"));break;
 case 15:show(a.list(s,"audit"));break;
 default:std::cout<<"Unknown action\n";break;
 }
}
void lecturerMenu(LmsService& a,const Session& s){
 std::cout<<"\nLECTURER | 1 My courses  2 Create assignment  3 Grant extension  4 View submissions\n"
             "           5 Mark / revise mark  6 My assignments  7 Mark statuses  8 Extend whole class  0 Logout\n";
 Id choice=number("Action");if(choice==0)throw 0;
 switch(choice){
 case 1:show(a.list(s,"courses"));break;
 case 2:{auto course=number("Course ID");auto title=prompt("Assignment title");auto description=prompt("Description");auto due=prompt("Deadline [YYYY-MM-DDTHH:MM:SSZ] UTC");auto max=decimal("Maximum mark");std::cout<<"Assignment ID: "<<a.createAssignment(s,course,title,description,due,max)<<'\n';break;}
 case 3:{auto assignment=number("Assignment ID");auto student=number("Student ID");auto due=prompt("Revised deadline [YYYY-MM-DDTHH:MM:SSZ] UTC");auto reason=prompt("Reason");std::cout<<"Extension ID: "<<a.grantExtension(s,assignment,student,due,reason)<<'\n';break;}
 case 4:show(a.list(s,"submissions"));break;
 case 5:{auto submission=number("Submission ID");auto mark=decimal("Raw mark");auto feedback=prompt("Written feedback");auto reason=prompt("Audit reason [e.g. Initial assessment or correction]");std::cout<<"Mark ID: "<<a.mark(s,submission,mark,feedback,reason)<<" (awaiting HoD moderation)\n";break;}
 case 6:show(a.list(s,"assignments"));break;
 case 7:show(a.list(s,"marks"));break;
 case 8:{auto assignment=number("Assignment ID");auto due=prompt("Class deadline [YYYY-MM-DDTHH:MM:SSZ] UTC");auto reason=prompt("Reason");a.grantClassExtension(s,assignment,due,reason);std::cout<<"Class deadline extension saved\n";break;}
 default:std::cout<<"Unknown action\n";break;
 }
}
void studentMenu(LmsService& a,const Session& s){
 std::cout<<"\nSTUDENT | 1 My courses  2 View assignments  3 Submit/resubmit  4 My attempts  5 Released marks  0 Logout\n";
 Id choice=number("Action");if(choice==0)throw 0;
 switch(choice){
 case 1:show(a.list(s,"courses"));break;
 case 2:show(a.list(s,"assignments"));break;
 case 3:{auto assignment=number("Assignment ID");auto content=prompt("Submission text (blank if attaching file)");auto file=prompt("Optional file path (blank if text only)");std::cout<<"Submission ID: "<<a.submit(s,assignment,content,file)<<" (all attempts retained)\n";break;}
 case 4:show(a.list(s,"submissions"));break;
 case 5:show(a.list(s,"results"));break;
 default:std::cout<<"Unknown action\n";break;
 }
}
void hodMenu(LmsService& a,const Session& s){
 std::cout<<"\nHEAD OF DEPARTMENT | 1 Review queue  2 Approve  3 Return for correction\n"
             "                     4 Release approved  5 Decision history  6 Mark audit  0 Logout\n";
 Id choice=number("Action");if(choice==0)throw 0;
 switch(choice){
 case 1:show(a.list(s,"queue"));break;
 case 2:{auto mark=number("Mark ID");auto reason=prompt("Approval note (may be blank)");a.review(s,mark,true,reason);std::cout<<"Approved; not yet visible to student until released\n";break;}
 case 3:{auto mark=number("Mark ID");auto reason=prompt("Correction reason");a.review(s,mark,false,reason);std::cout<<"Returned for correction\n";break;}
 case 4:{auto mark=number("Mark ID");a.release(s,mark);std::cout<<"Released; student may now see result\n";break;}
 case 5:show(a.list(s,"decisions"));break;
 case 6:show(a.list(s,"audit"));break;
 default:std::cout<<"Unknown action\n";break;
 }
}
}
int main(int argc,char* argv[]){
 try{
  std::string dbPath="data/lms.db",schema="sql/schema.sql";bool init=false;
  for(int i=1;i<argc;++i){std::string a=argv[i];
   if(a=="--db"&&i+1<argc)dbPath=argv[++i];
   else if(a=="--schema"&&i+1<argc)schema=argv[++i];
   else if(a=="--init-admin")init=true;
   else if(a=="--help"){std::cout<<"Usage: lms [--db data/lms.db] [--schema sql/schema.sql] [--init-admin]\nSQLite C++17 Moderated LMS. Run from project root or build folder.\n";return 0;}
   else throw std::invalid_argument("Unknown or incomplete option: "+a);
  }
  auto parent=std::filesystem::path(dbPath).parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
  lms::Db db(dbPath);db.loadSchema(schema);LmsService app(db);
  if(init){
   if(app.anyUsers())throw std::runtime_error("Bootstrap refused: accounts already exist");
   auto login=prompt("First admin username");auto name=prompt("Full name");auto password=lms::security::readSecret("New admin password (10+ chars): ");
   app.bootstrapAdmin(login,name,password);std::cout<<"First administrator created successfully.\n";return 0;
  }
  if(!app.anyUsers()){std::cout<<"No users exist. Run: lms --init-admin\n";return 1;}
  std::cout<<"SCHOOL LEARNING MANAGEMENT SYSTEM (offline SQLite)\n";
  for(;;){
   try{
    std::cout<<"\nLogin as:\n"
             "  1. Administrator\n"
             "  2. Lecturer\n"
             "  3. Head of Department (HoD)\n"
             "  4. Student\n"
             "  0. Exit\n";
    auto choice=number("Action");
    if(choice==0)break;
    if(choice<1||choice>4){std::cout<<"Invalid selection. Choose 0 through 4.\n";continue;}
    const lms::Role selectedRole = choice==1 ? lms::Role::ADMIN :
                                   choice==2 ? lms::Role::LECTURER :
                                   choice==3 ? lms::Role::HOD : lms::Role::STUDENT;
    auto login=prompt("Username");auto password=lms::security::readSecret("Password: ");
    Session s=app.login(login,password);
    if(s.role!=selectedRole){
     std::cout<<"Login rejected: this account is not registered as "
              <<lms::roleName(selectedRole)<<". Please select your account's role.\n";
     continue;
    }
    std::cout<<"Signed in as "<<s.username<<" ("<<lms::roleName(s.role)<<")\n";
    for(;;){try{
     switch(s.role){case lms::Role::ADMIN:adminMenu(app,s);break;case lms::Role::LECTURER:lecturerMenu(app,s);break;case lms::Role::STUDENT:studentMenu(app,s);break;case lms::Role::HOD:hodMenu(app,s);break;}
    }catch(int){std::cout<<"Logged out\n";break;}catch(const std::exception& e){if(!std::cin)return 0;std::cerr<<"Operation rejected: "<<e.what()<<'\n';}}
   }catch(const std::exception& e){if(!std::cin)break;std::cerr<<"Error: "<<e.what()<<'\n';}
  }
  std::cout<<"Goodbye\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Startup error: "<<e.what()<<'\n';return 1;}
}
