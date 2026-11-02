# CS4433 LMS Coding Standard — C++17 (v1.0)

1. Compile as C++17, with compiler warnings enabled (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow` on GCC/Clang, `/W4` with MSVC).
2. Types: classes and interfaces in `PascalCase`; functions in `camelCase`; private attributes suffixed `_`; SQL table and column names in `snake_case`.
3. Separate `main.cpp` menu handling, `service.cpp` use-case rules, `repository.cpp` data-access objects, `db.cpp` SQLite preparation/binding/RAII and `security.cpp` passwords.
4. Use prepared statements with typed bound parameters for all user-controlled values; never concatenate user text into SQL statements.
5. Obtain authorization from the current authenticated `Session` and verify current account status/department before protected operations. Never accept arbitrary acting user IDs from the user interface.
6. Use `std::unique_ptr` to express object ownership, RAII for SQLite statements and transactions, and exceptions for invalid use cases.
7. Treat timestamps as ISO 8601 UTC (`YYYY-MM-DDTHH:MM:SSZ`) and reject malformed dates.
8. Store only salted one-way password hashes; do not output password strings or hashes to diagnostic logs.
9. Keep business-rule calculations inside `ILatePenaltyPolicy` implementations. Do not duplicate the 10% late penalty formula in menu code.
10. Treat a mark change and its corresponding audit insertion as one atomic database transaction. Never display marks to students except through released-results queries.
11. Keep each class narrowly focused; descriptive method names, no silent exception swallowing in application logic. Defensive rollback is allowed in `Transaction` destructor.
12. Test successful flows, authorization failures, boundary conditions, and persistence before commits; review diffs for secret leakage and schema changes.
