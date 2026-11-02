#pragma once
#include "db.hpp"
#include "model.hpp"
namespace lms {
struct SubmissionContext{
 Id courseId=0;
 double maximumMark=0;
 int lateDays=0;
 Id assignmentId=0;
 Id studentId=0;
 Id attemptNumber=0;
};
class SQLiteSubmissionRepository final:public ISubmissionRepository {
 Db& db_;
public:
 explicit SQLiteSubmissionRepository(Db& db):db_(db){}
 bool exists(Id submissionId)override;
 SubmissionContext context(Id submissionId);
 Id latestAttempt(Id assignmentId,Id studentId);
 bool isLatest(Id submissionId);
};
}
