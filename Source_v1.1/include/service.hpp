#pragma once
#include "db.hpp"
#include "model.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace lms {
std::string utcNow();
std::string isoUtc(const std::string& value); // strict ISO UTC validation
class LmsService {
 Db& db_;
 std::unique_ptr<ILatePenaltyPolicy> policy_;
 std::function<std::string()> clock_;
 void require(const Session& s,Role role)const;
 void ensureActive(const Session& s)const;
 void ensureLecturerCourse(const Session& s,Id course)const;
 void ensureStudentEnrolled(const Session& s,Id course)const;
 void ensureHodDepartment(const Session& s,Id mark)const;
public:
 explicit LmsService(Db& db,std::unique_ptr<ILatePenaltyPolicy> policy=std::make_unique<DailyTenPercentPenalty>(),std::function<std::string()> clock=utcNow);
 bool anyUsers();
 void bootstrapAdmin(const std::string& username,const std::string& fullName,const std::string& password);
 Session login(const std::string& username,const std::string& password);
 Id createDepartment(const Session&,const std::string& code,const std::string& name);
 void updateDepartment(const Session&,Id id,const std::string& name);
 Id createUser(const Session&,const std::string& username,const std::string& fullname,Role role,Id departmentId,const std::string& password);
 void updateUser(const Session&,Id target,const std::string& fullname,Id departmentId);
 void deactivateUser(const Session&,Id target);
 void resetPassword(const Session&,Id target,const std::string& password);
 Id createCourse(const Session&,Id departmentId,const std::string& code,const std::string& title);
 void updateCourse(const Session&,Id course,const std::string& title,bool active);
 void assignLecturer(const Session&,Id lecturerId,Id courseId);
 void enrolStudent(const Session&,Id studentId,Id courseId);
 Id createAssignment(const Session&,Id courseId,const std::string& title,const std::string& description,const std::string& deadline,double maximumMark);
 Id grantExtension(const Session&,Id assignmentId,Id studentId,const std::string& revisedDeadline,const std::string& reason);
 Id grantClassExtension(const Session&,Id assignmentId,const std::string& revisedDeadline,const std::string& reason);
 Id submit(const Session&,Id assignmentId,const std::string& text,const std::string& localFilePath="");
 Id mark(const Session&,Id submissionId,double rawMark,const std::string& feedback,const std::string& reason="Initial assessment");
 void review(const Session&,Id markId,bool approve,const std::string& reason);
 void release(const Session&,Id markId);
 std::vector<std::vector<std::string>> list(const Session&,const std::string& view); // whitelisted view identifiers only
 std::string now()const{return clock_();}
};
}
