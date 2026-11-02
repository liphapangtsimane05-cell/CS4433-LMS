#include "../include/repository.hpp"
#include <stdexcept>
namespace lms {
bool SQLiteSubmissionRepository::exists(Id id){Statement s(db_,"SELECT 1 FROM submissions WHERE submission_id=?");s.bind(1,id);return s.row();}
SubmissionContext SQLiteSubmissionRepository::context(Id id){
 Statement s(db_,"SELECT a.course_id,a.maximum_mark,sub.late_days,sub.assignment_id,sub.student_id,sub.attempt_number FROM submissions sub JOIN assignments a ON a.assignment_id=sub.assignment_id WHERE sub.submission_id=?");s.bind(1,id);
 if(!s.row())throw std::invalid_argument("Submission not found");
 return SubmissionContext{s.integer(0),s.decimal(1),static_cast<int>(s.integer(2)),s.integer(3),s.integer(4),s.integer(5)};
}
Id SQLiteSubmissionRepository::latestAttempt(Id assignment,Id student){Statement s(db_,"SELECT coalesce(MAX(attempt_number),0) FROM submissions WHERE assignment_id=? AND student_id=?");s.bind(1,assignment);s.bind(2,student);return s.row()?s.integer(0):0;}
bool SQLiteSubmissionRepository::isLatest(Id id){auto ctx=context(id);return ctx.attemptNumber==latestAttempt(ctx.assignmentId,ctx.studentId);}
}
