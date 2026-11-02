# Release v1.1 — CR1 whole-class deadline extensions

- Lecturer menu option 8 grants or extends one class-wide deadline per assignment.
- Students see the latest effective deadline on the assignment list.
- Effective deadline = latest of original deadline, student-specific extension and class extension.
- Existing submission, resubmission, lateness and marks rules use the effective deadline.
- Class extension revisions are recorded in `class_extension_audit`.
- Existing individual extensions remain supported.
- Existing databases require `sql/migrations/001_cr1_class_extensions.sql` **once**, after taking a backup.
- Fresh databases use the updated `sql/schema.sql`.
- Verified: local C++ Release build and 50 regression assertions. Windows compilation and a published GitHub v1.1 tag remain pending.
