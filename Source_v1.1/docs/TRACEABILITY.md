# Week 4 Requirements-to-Implementation Traceability

| Week 2 SRS ID | Implemented behaviour | Source file / method | Verification |
|---|---|---|---|
| FR-01 | Username/password authentication | `src/service.cpp` `login`, `src/security.cpp` | Valid and invalid login in CTest |
| FR-02 | Four roles; server-side role + active + department checks | `include/model.hpp`, `LmsService::require` | Role and cross-department denials |
| FR-03 | Create/update/deactivate users; create/update departments and courses | `LmsService::createUser`, `updateUser`, `deactivateUser`, `createDepartment`, `updateDepartment`, `createCourse`, `updateCourse` | Admin menus and seeded tests |
| FR-04 | Lecturer-course links and student enrolments | `assignLecturer`, `enrolStudent`; SQL foreign keys/triggers | Assigned/enrolled workflow test |
| FR-05 | Lecturer creates assignment with due date and max mark | `createAssignment` | Authorized/unauthorized cases |
| FR-06 | Individual extension, revised deadline and reason | `grantExtension`, SQLite trigger | Extension benchmark in CTest |
| FR-07 | Student sees only enrolled course assignments | `LmsService::list("courses"/"assignments")` | Scoped listing test |
| FR-08 | Attempt, timestamp, content/file, lateness and status | `submit` | Persisted on-time/late attempt test |
| FR-09 | Attempts retained; only pre-deadline resubmission | `submit`, `SQLiteSubmissionRepository::latestAttempt` | Two attempts and deadline denial |
| FR-10 | Valid mark and mandatory written feedback | `mark` | Mark and feedback validation |
| FR-11 | 10% of maximum per started late day (up to 3); >3 = 0 | `DailyTenPercentPenalty::calculate`, `submit` | Tests for 2-day and >3-day examples |
| FR-12 | Department HoD approves, then releases; student sees released results only | `review`, `release`, `student_released_results` | Tests for hidden/published and unauthorized HoD |
| FR-13 | Every application-originated mark insert/revision has atomic audit | `mark` transaction + `mark_audit` | Counts and fields checked in tests |
| FR-14 | Input validation and controlled error messages | `isoUtc`, `check`, menu exception handling | Invalid-input tests |
| FR-15 | All persistent school records stored in SQLite | `sql/schema.sql`, `Db` RAII layer | Reopen real SQLite file test |
| FR-16 | Offline operation; no network API | Console/C++/SQLite local file | Architectural review; offline demo pending |

**Caveat:** The automated tests here are initial implementation checks, not the complete Phase 5 V-Model test plan or a verified 70% coverage report. NFR performance and school-hardware acceptance still require representative measurement.
