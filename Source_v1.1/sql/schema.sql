-- CS4433 Moderated Learning Management System (LMS)
-- SQLite 3 schema | Initial release v1.0 | C++17 / SQLite offline application
-- Apply PRAGMA foreign_keys = ON on EVERY new application connection.
PRAGMA foreign_keys = ON;
BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS departments (
    department_id INTEGER PRIMARY KEY,
    department_code TEXT NOT NULL UNIQUE,
    department_name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY,
    username TEXT NOT NULL UNIQUE COLLATE NOCASE,
    password_hash TEXT NOT NULL,
    full_name TEXT NOT NULL,
    role TEXT NOT NULL CHECK (role IN ('ADMIN','LECTURER','HOD','STUDENT')),
    department_id INTEGER,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    FOREIGN KEY (department_id) REFERENCES departments(department_id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS courses (
    course_id INTEGER PRIMARY KEY,
    department_id INTEGER NOT NULL,
    course_code TEXT NOT NULL UNIQUE,
    course_title TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    FOREIGN KEY (department_id) REFERENCES departments(department_id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS lecturer_courses (
    lecturer_id INTEGER NOT NULL,
    course_id INTEGER NOT NULL,
    assigned_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    PRIMARY KEY (lecturer_id, course_id),
    FOREIGN KEY (lecturer_id) REFERENCES users(user_id) ON DELETE RESTRICT,
    FOREIGN KEY (course_id) REFERENCES courses(course_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS enrolments (
    student_id INTEGER NOT NULL,
    course_id INTEGER NOT NULL,
    enrolled_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    PRIMARY KEY (student_id, course_id),
    FOREIGN KEY (student_id) REFERENCES users(user_id) ON DELETE RESTRICT,
    FOREIGN KEY (course_id) REFERENCES courses(course_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS assignments (
    assignment_id INTEGER PRIMARY KEY,
    course_id INTEGER NOT NULL,
    created_by INTEGER NOT NULL,
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    due_at TEXT NOT NULL,  -- ISO-8601 UTC: YYYY-MM-DDTHH:MM:SSZ
    maximum_mark REAL NOT NULL CHECK (maximum_mark > 0),
    published INTEGER NOT NULL DEFAULT 1 CHECK (published IN (0,1)),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    FOREIGN KEY (course_id) REFERENCES courses(course_id) ON DELETE RESTRICT,
    FOREIGN KEY (created_by) REFERENCES users(user_id) ON DELETE RESTRICT
);

-- Individual extensions remain available in v1.1 alongside whole-class extensions.
CREATE TABLE IF NOT EXISTS individual_extensions (
    extension_id INTEGER PRIMARY KEY,
    assignment_id INTEGER NOT NULL,
    student_id INTEGER NOT NULL,
    granted_by INTEGER NOT NULL,
    revised_due_at TEXT NOT NULL,
    reason TEXT NOT NULL CHECK (length(trim(reason)) > 0),
    granted_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    UNIQUE (assignment_id, student_id),
    FOREIGN KEY (assignment_id) REFERENCES assignments(assignment_id) ON DELETE RESTRICT,
    FOREIGN KEY (student_id) REFERENCES users(user_id) ON DELETE RESTRICT,
    FOREIGN KEY (granted_by) REFERENCES users(user_id) ON DELETE RESTRICT
);

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
-- Keep every attempt. A resubmission creates a new row, never overwrites an earlier one.
CREATE TABLE IF NOT EXISTS submissions (
    submission_id INTEGER PRIMARY KEY,
    assignment_id INTEGER NOT NULL,
    student_id INTEGER NOT NULL,
    attempt_number INTEGER NOT NULL CHECK (attempt_number >= 1),
    submission_text TEXT NOT NULL DEFAULT '',
    file_path TEXT, -- local file path if file-based submissions are supported
    submitted_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    applicable_due_at TEXT NOT NULL,
    late_days INTEGER NOT NULL DEFAULT 0 CHECK (late_days >= 0),
    submission_status TEXT NOT NULL DEFAULT 'RECEIVED'
        CHECK (submission_status IN ('RECEIVED','LATE','OVER_THREE_DAYS')),
    UNIQUE (assignment_id, student_id, attempt_number),
    FOREIGN KEY (assignment_id) REFERENCES assignments(assignment_id) ON DELETE RESTRICT,
    FOREIGN KEY (student_id) REFERENCES users(user_id) ON DELETE RESTRICT,
    CHECK (length(trim(submission_text)) > 0 OR (file_path IS NOT NULL AND length(trim(file_path)) > 0))
);

-- A mark is associated with a specific submission attempt.
CREATE TABLE IF NOT EXISTS marks (
    mark_id INTEGER PRIMARY KEY,
    submission_id INTEGER NOT NULL UNIQUE,
    marked_by INTEGER NOT NULL,
    raw_mark REAL NOT NULL CHECK (raw_mark >= 0),
    penalty_mark REAL NOT NULL DEFAULT 0 CHECK (penalty_mark >= 0),
    final_mark REAL NOT NULL CHECK (final_mark >= 0),
    feedback TEXT NOT NULL CHECK (length(trim(feedback)) > 0),
    status TEXT NOT NULL DEFAULT 'PENDING_MODERATION'
        CHECK (status IN ('PENDING_MODERATION','CORRECTION_REQUIRED','APPROVED','RELEASED')),
    marked_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    FOREIGN KEY (submission_id) REFERENCES submissions(submission_id) ON DELETE RESTRICT,
    FOREIGN KEY (marked_by) REFERENCES users(user_id) ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS moderation_decisions (
    decision_id INTEGER PRIMARY KEY,
    mark_id INTEGER NOT NULL,
    hod_id INTEGER NOT NULL,
    decision TEXT NOT NULL CHECK (decision IN ('APPROVE','RETURN_FOR_CORRECTION')),
    reason TEXT NOT NULL DEFAULT '',
    decided_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    FOREIGN KEY (mark_id) REFERENCES marks(mark_id) ON DELETE RESTRICT,
    FOREIGN KEY (hod_id) REFERENCES users(user_id) ON DELETE RESTRICT,
    CHECK (decision <> 'RETURN_FOR_CORRECTION' OR length(trim(reason)) > 0)
);

CREATE TABLE IF NOT EXISTS mark_releases (
    mark_id INTEGER PRIMARY KEY,
    released_by INTEGER NOT NULL,
    released_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    FOREIGN KEY (mark_id) REFERENCES marks(mark_id) ON DELETE RESTRICT,
    FOREIGN KEY (released_by) REFERENCES users(user_id) ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS mark_audit (
    audit_id INTEGER PRIMARY KEY,
    mark_id INTEGER NOT NULL,
    changed_by INTEGER NOT NULL,
    old_raw_mark REAL,
    new_raw_mark REAL,
    old_final_mark REAL,
    new_final_mark REAL,
    reason TEXT NOT NULL CHECK (length(trim(reason)) > 0),
    changed_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    FOREIGN KEY (mark_id) REFERENCES marks(mark_id) ON DELETE RESTRICT,
    FOREIGN KEY (changed_by) REFERENCES users(user_id) ON DELETE RESTRICT
);

CREATE INDEX IF NOT EXISTS idx_users_department_role ON users(department_id,role);
CREATE INDEX IF NOT EXISTS idx_courses_department ON courses(department_id);
CREATE INDEX IF NOT EXISTS idx_assignments_course ON assignments(course_id);
CREATE INDEX IF NOT EXISTS idx_submissions_student ON submissions(student_id,assignment_id);
CREATE INDEX IF NOT EXISTS idx_marks_status ON marks(status);
CREATE INDEX IF NOT EXISTS idx_moderation_mark ON moderation_decisions(mark_id,decided_at);
CREATE INDEX IF NOT EXISTS idx_audit_mark ON mark_audit(mark_id,changed_at);

-- Role and relationship validation for school records.
CREATE TRIGGER IF NOT EXISTS trg_lecturer_course_role BEFORE INSERT ON lecturer_courses
BEGIN
    SELECT RAISE(ABORT,'Assigned user must be an active lecturer')
    WHERE NOT EXISTS (SELECT 1 FROM users WHERE user_id=NEW.lecturer_id AND role='LECTURER' AND active=1);
END;
CREATE TRIGGER IF NOT EXISTS trg_enrolment_student_role BEFORE INSERT ON enrolments
BEGIN
    SELECT RAISE(ABORT,'Enrolled user must be an active student')
    WHERE NOT EXISTS (SELECT 1 FROM users WHERE user_id=NEW.student_id AND role='STUDENT' AND active=1);
END;
CREATE TRIGGER IF NOT EXISTS trg_assignment_lecturer BEFORE INSERT ON assignments
BEGIN
    SELECT RAISE(ABORT,'Assignment creator must teach the course')
    WHERE NOT EXISTS (SELECT 1 FROM lecturer_courses lc JOIN users u ON u.user_id=lc.lecturer_id
                      WHERE lc.lecturer_id=NEW.created_by AND lc.course_id=NEW.course_id AND u.active=1);
END;
CREATE TRIGGER IF NOT EXISTS trg_extension_valid BEFORE INSERT ON individual_extensions
BEGIN
    SELECT RAISE(ABORT,'Extension requires enrolment and authorised lecturer')
    WHERE NOT EXISTS (SELECT 1 FROM assignments a JOIN enrolments en ON en.course_id=a.course_id
                      JOIN lecturer_courses lc ON lc.course_id=a.course_id
                      WHERE a.assignment_id=NEW.assignment_id AND en.student_id=NEW.student_id AND lc.lecturer_id=NEW.granted_by);
    SELECT RAISE(ABORT,'Revised deadline must be later than original deadline')
    WHERE julianday(NEW.revised_due_at) <= (SELECT julianday(due_at) FROM assignments WHERE assignment_id=NEW.assignment_id);
END;
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
CREATE TRIGGER IF NOT EXISTS trg_mark_valid BEFORE INSERT ON marks
BEGIN
    SELECT RAISE(ABORT,'Marking lecturer must be assigned to the course')
    WHERE NOT EXISTS (SELECT 1 FROM submissions s JOIN assignments a ON a.assignment_id=s.assignment_id
                      JOIN lecturer_courses lc ON lc.course_id=a.course_id
                      WHERE s.submission_id=NEW.submission_id AND lc.lecturer_id=NEW.marked_by);
    SELECT RAISE(ABORT,'Raw mark exceeds assignment maximum')
    WHERE NEW.raw_mark > (SELECT a.maximum_mark FROM submissions s JOIN assignments a ON a.assignment_id=s.assignment_id WHERE s.submission_id=NEW.submission_id);
    SELECT RAISE(ABORT,'Final mark cannot exceed raw mark') WHERE NEW.final_mark > NEW.raw_mark;
END;
CREATE TRIGGER IF NOT EXISTS trg_mark_update_valid BEFORE UPDATE OF raw_mark,final_mark ON marks
BEGIN
    SELECT RAISE(ABORT,'Raw mark exceeds assignment maximum')
    WHERE NEW.raw_mark > (SELECT a.maximum_mark FROM submissions s JOIN assignments a ON a.assignment_id=s.assignment_id WHERE s.submission_id=NEW.submission_id);
    SELECT RAISE(ABORT,'Final mark cannot exceed raw mark') WHERE NEW.final_mark > NEW.raw_mark;
    SELECT RAISE(ABORT,'Released marks cannot be edited') WHERE OLD.status='RELEASED';
END;
CREATE TRIGGER IF NOT EXISTS trg_moderation_hod BEFORE INSERT ON moderation_decisions
BEGIN
    SELECT RAISE(ABORT,'HoD is not authorised for this department')
    WHERE NOT EXISTS (SELECT 1 FROM marks m JOIN submissions s ON s.submission_id=m.submission_id
       JOIN assignments a ON a.assignment_id=s.assignment_id JOIN courses c ON c.course_id=a.course_id
       JOIN users u ON u.user_id=NEW.hod_id WHERE m.mark_id=NEW.mark_id AND u.role='HOD'
       AND u.active=1 AND u.department_id=c.department_id);
END;
CREATE TRIGGER IF NOT EXISTS trg_release_approved BEFORE INSERT ON mark_releases
BEGIN
    SELECT RAISE(ABORT,'Only approved marks can be released')
    WHERE NOT EXISTS (SELECT 1 FROM marks WHERE mark_id=NEW.mark_id AND status='APPROVED');
    SELECT RAISE(ABORT,'Release requires an HoD approval decision')
    WHERE NOT EXISTS (SELECT 1 FROM moderation_decisions WHERE mark_id=NEW.mark_id AND decision='APPROVE');
    SELECT RAISE(ABORT,'Only departmental HoD can release marks')
    WHERE NOT EXISTS (SELECT 1 FROM marks m JOIN submissions s ON s.submission_id=m.submission_id
       JOIN assignments a ON a.assignment_id=s.assignment_id JOIN courses c ON c.course_id=a.course_id
       JOIN users u ON u.user_id=NEW.released_by WHERE m.mark_id=NEW.mark_id AND u.role='HOD'
       AND u.active=1 AND u.department_id=c.department_id);
END;
CREATE TRIGGER IF NOT EXISTS trg_release_set_status AFTER INSERT ON mark_releases
BEGIN
    UPDATE marks SET status='RELEASED',updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE mark_id=NEW.mark_id;
END;

-- Student-facing query: ONLY released marks are visible.
CREATE VIEW IF NOT EXISTS student_released_results AS
SELECT s.student_id, s.assignment_id, s.submission_id, m.final_mark, m.feedback, r.released_at
FROM marks m JOIN submissions s ON s.submission_id=m.submission_id
JOIN mark_releases r ON r.mark_id=m.mark_id
WHERE m.status='RELEASED';

-- Submission attempt summary. The latest attempt is the one with highest attempt_number.
CREATE VIEW IF NOT EXISTS latest_submissions AS
SELECT s.* FROM submissions s WHERE s.attempt_number =
 (SELECT MAX(s2.attempt_number) FROM submissions s2 WHERE s2.assignment_id=s.assignment_id AND s2.student_id=s.student_id);

COMMIT;

-- APPLICATION RULES (implement in C++ services, inside SQLite transactions):
-- 1. Verify username/password hash, roles and department scope on every protected operation.
-- 2. Calculate late days against applicable_due_at; clarify rounding at Gate 1.
-- 3. Proposed penalty = min(raw_mark, 0.10 * maximum_mark * late_days) for 1..3 days.
--    final_mark = max(0, raw_mark - penalty); if >3 days late, final_mark = 0.
--    Interpretation of '10% of available mark' must be confirmed at Gate 1.
-- 4. When editing a mark, INSERT mark_audit with old/new values, actor and reason
--    in the SAME transaction as the UPDATE; this is essential for FR-13.
-- 5. After HoD decision, update marks.status to APPROVED or CORRECTION_REQUIRED in
--    the same transaction; only then may an approved mark be inserted into mark_releases.
-- 6. For results, query student_released_results WHERE student_id = authenticated ID.
-- 7. Store timestamps in UTC ISO-8601; protect the local computer clock.
-- 8. Use prepared statements; never interpolate user input into SQL strings.
