-- v1.0 -> v1.1: take a backup of lms.db before executing. Run once only.
BEGIN IMMEDIATE;
-- v1.1 CR1: one current class-wide extension per assignment; changes are logged separately.
CREATE TABLE IF NOT EXISTS class_extensions (
 assignment_id INTEGER PRIMARY KEY REFERENCES assignments(assignment_id) ON DELETE RESTRICT,
 granted_by INTEGER NOT NULL REFERENCES users(user_id) ON DELETE RESTRICT,
 revised_due_at TEXT NOT NULL,
 reason TEXT NOT NULL CHECK (length(trim(reason))>0),
 granted_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE TABLE IF NOT EXISTS class_extension_audit (
 audit_id INTEGER PRIMARY KEY,
 assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id),
 granted_by INTEGER NOT NULL REFERENCES users(user_id),
 previous_due_at TEXT,
 revised_due_at TEXT NOT NULL,
 reason TEXT NOT NULL,
 changed_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE TRIGGER IF NOT EXISTS trg_class_extension_authorize BEFORE INSERT ON class_extensions
BEGIN
 SELECT RAISE(ABORT,'Class extension requires assigned lecturer') WHERE NOT EXISTS
 (SELECT 1 FROM assignments a JOIN lecturer_courses lc ON lc.course_id=a.course_id JOIN users u ON u.user_id=lc.lecturer_id WHERE a.assignment_id=NEW.assignment_id AND lc.lecturer_id=NEW.granted_by AND u.role='LECTURER' AND u.active=1);
 SELECT RAISE(ABORT,'Class deadline must be later than original') WHERE julianday(NEW.revised_due_at)<= (SELECT julianday(due_at) FROM assignments WHERE assignment_id=NEW.assignment_id);
END;
CREATE TRIGGER IF NOT EXISTS trg_class_extension_audit_insert AFTER INSERT ON class_extensions
BEGIN
 INSERT INTO class_extension_audit(assignment_id,granted_by,previous_due_at,revised_due_at,reason) VALUES(NEW.assignment_id,NEW.granted_by,NULL,NEW.revised_due_at,NEW.reason);
END;
CREATE TRIGGER IF NOT EXISTS trg_class_extension_audit_update AFTER UPDATE ON class_extensions
BEGIN
 INSERT INTO class_extension_audit(assignment_id,granted_by,previous_due_at,revised_due_at,reason) VALUES(NEW.assignment_id,NEW.granted_by,OLD.revised_due_at,NEW.revised_due_at,NEW.reason);
END;
DROP TRIGGER IF EXISTS trg_submission_valid;
CREATE TRIGGER IF NOT EXISTS trg_submission_valid BEFORE INSERT ON submissions
BEGIN
    SELECT RAISE(ABORT,'Student not enrolled in assignment course')
    WHERE NOT EXISTS (SELECT 1 FROM assignments a JOIN enrolments en ON en.course_id=a.course_id
                      WHERE a.assignment_id=NEW.assignment_id AND en.student_id=NEW.student_id);
    SELECT RAISE(ABORT,'Applicable deadline does not match effective assignment deadline')
    WHERE julianday(NEW.applicable_due_at) IS NULL OR
      abs(julianday(NEW.applicable_due_at) - julianday(MAX(
        COALESCE((SELECT revised_due_at FROM individual_extensions WHERE assignment_id=NEW.assignment_id AND student_id=NEW.student_id),(SELECT due_at FROM assignments WHERE assignment_id=NEW.assignment_id)),
        COALESCE((SELECT revised_due_at FROM class_extensions WHERE assignment_id=NEW.assignment_id),(SELECT due_at FROM assignments WHERE assignment_id=NEW.assignment_id))))) > 0.000001;
    SELECT RAISE(ABORT,'Attempt number must be sequential')
    WHERE NEW.attempt_number <> 1 + (SELECT count(*) FROM submissions
                                  WHERE assignment_id=NEW.assignment_id AND student_id=NEW.student_id);
    SELECT RAISE(ABORT,'Resubmissions must be before the applicable deadline')
    WHERE NEW.attempt_number > 1 AND julianday(NEW.submitted_at) > julianday(NEW.applicable_due_at);
END;
PRAGMA user_version=11;
COMMIT;
