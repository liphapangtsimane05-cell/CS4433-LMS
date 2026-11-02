#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace lms {
using Id=std::int64_t;
enum class Role { ADMIN, LECTURER, HOD, STUDENT };
Role parseRole(const std::string& value);
std::string roleName(Role role);
class User {
protected:
 Id id_;
 std::string username_;
public:
 User(Id id,std::string name):id_(id),username_(std::move(name)){}
 virtual ~User()=default;
 virtual Role role()const=0;
 Id id()const{return id_;}
 const std::string& username()const{return username_;}
 virtual bool can(const std::string& action)const=0;
};
class Administrator final:public User {public:using User::User;Role role()const override{return Role::ADMIN;}bool can(const std::string& action)const override;};
class Lecturer final:public User {public:using User::User;Role role()const override{return Role::LECTURER;}bool can(const std::string& action)const override;};
class HeadOfDepartment final:public User {public:using User::User;Role role()const override{return Role::HOD;}bool can(const std::string& action)const override;};
class Student final:public User {public:using User::User;Role role()const override{return Role::STUDENT;}bool can(const std::string& action)const override;};
std::unique_ptr<User> makeUser(Id id,const std::string& username,Role role);
struct Session {Id userId=0;Role role=Role::STUDENT;Id departmentId=0;std::string username;};
class ILatePenaltyPolicy {
public:
 virtual ~ILatePenaltyPolicy()=default;
 virtual double calculate(double raw,double maximum,int lateDays)const=0;
};
class DailyTenPercentPenalty final:public ILatePenaltyPolicy{
public:double calculate(double raw,double maximum,int lateDays)const override;
};
class ISubmissionRepository {public: virtual ~ISubmissionRepository()=default; virtual bool exists(Id submissionId)=0;};
}
