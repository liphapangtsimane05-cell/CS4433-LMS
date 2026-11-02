# CS4433 — Moderated Learning Management System (LMS)

**Phase 4 Implementation (Weeks 4–5), initial release v1.0**  
**National University of Lesotho — CS4433 Software Engineering**  
**Stack:** C++17, SQLite3, OpenSSL (PBKDF2-HMAC-SHA256), CMake, CTest; terminal application.  
**Design baseline:** Phase 3 Software Design; SQLite schema with 12 tables, 21 foreign keys and 2 views.

## Project contents

```
CMakeLists.txt
README.md
.gitignore
.github/workflows/build-test.yml   GitHub Actions CI definition (runs after repository upload)
include/
    db.hpp                      SQLite RAII wrapper and parameterized queries
    security.hpp                salted PBKDF2 hashing and masked input
    model.hpp                   abstract User, four derived roles, policy interface
    repository.hpp              submission repository interface + SQLite DAO
    service.hpp                 application use cases and role scopes
src/
    db.cpp
    security.cpp
    repository.cpp
    service.cpp
    main.cpp                    console menus for ADMIN, LECTURER, HOD, STUDENT
sql/schema.sql                  original 12-table SQLite implementation schema
tests/smoke.cpp                 automated end-to-end functional/security tests
docs/CODING_STANDARD.md
docs/TRACEABILITY.md
docs/BUILD_TEST_EVIDENCE.txt
```

## Prerequisites

* C++17 compiler: GCC 9+, Clang 10+ or recent Microsoft Visual C++.
* CMake 3.16 or newer.
* SQLite3 development library + headers.
* OpenSSL libcrypto development library + headers.
* Git for version control.
* No Internet is required to use the built LMS.

**Ubuntu / Debian:**

```bash
sudo apt update
sudo apt install -y build-essential cmake git libsqlite3-dev libssl-dev
```

**Windows 11 using MSYS2 UCRT64 (recommended for VS Code):** Install MSYS2 from https://www.msys2.org/. Open **MSYS2 UCRT64**, update using `pacman -Syu`, restart the terminal when prompted, then install:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 \
    mingw-w64-ucrt-x86_64-openssl git
```

Use the UCRT64 shell to configure and build. VS Code can open this project folder directly. Alternatively, use Visual Studio plus vcpkg SQLite3/OpenSSL packages, ensuring CMake resolves both libraries.

## Build (Linux, macOS with dependencies, Windows MSYS2)

From the **root of this source folder**:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

On Windows use `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, then `cmake --build build`.

The executable is `build/lms` (Linux/macOS) or `build/lms.exe` (Windows). CMake copies `sql/schema.sql` into `build/sql/` as well. All output directories and database files are excluded from Git.

## Start the LMS

From the project root, first bootstrap the only initial administrator (this is permitted **only while users is empty**):

```bash
./build/lms --init-admin
```

The program prompts for an initial username, full name and secure password (10–128 characters). Terminal password entry is hidden on interactive supported terminals. On Windows use `./build/lms.exe --init-admin` from UCRT64.

Start normally:

```bash
./build/lms
```

The default SQLite file is `data/lms.db`; it is created automatically, along with all schema tables. To specify paths:

```bash
./build/lms --db data/school.db --schema sql/schema.sql
```

**Important:** Run these commands in the project root. When running from the build directory use its copied `sql/schema.sql`. Do not share `data/lms.db`, user passwords, or learner submission files in a public Git repository.

## Role-specific demonstration workflow

1. **Administrator:** Create a department (note its ID), create three users (LECTURER, STUDENT and HOD, each with that department ID); create a course; assign the lecturer to it; enrol the student.
2. **Lecturer:** Log in, create an assignment for the assigned course using the UTC deadline format `YYYY-MM-DDTHH:MM:SSZ` and a maximum mark above zero. Optionally grant an extension to one enrolled student with a later deadline and a reason.
3. **Student:** Log in, view enrolled courses/assignments, submit text or attach a local file (max 10 MiB). Resubmit while the effective deadline has not passed; each attempt gets a new record.
4. **Lecturer:** Log in, view attempts, mark the latest submission (raw score up to the maximum) and provide written feedback. The policy computes the final score from late days. Initial marking and revisions are audited.
5. **HoD:** Log in, review the mark in the same department, approve it, then release it. Alternatively return it for correction with a reason.
6. **Student:** Confirm results are absent before release and visible with feedback after release.

**Rule interpretations from Gate 1 baseline:** Positive fractional lateness counts as a started late day; 10% of assignment maximum per day for days 1–3, score zero after three days. An individual extension overrides the deadline for one student; resubmissions are valid only before the effective deadline. Over-three-day first attempts are stored with status `OVER_THREE_DAYS`, then score zero when marked. A marked superseded attempt cannot be approved/released; only the latest attempt is eligible.

## Security and integrity

* Passwords are stored as random-16-byte-salt **PBKDF2-HMAC-SHA256** hashes (OpenSSL, 310,000 iterations, 32-byte output); password verification uses constant-time comparison.
* Queries with supplied data use bound SQLite parameters through a RAII `Statement` data-access wrapper; `SQLiteSubmissionRepository` implements the repository abstraction.
* Session role, current active status, and department scope are checked in protected services.
* Each mark insert or revision and its old/new-value audit event commit in a single transaction.
* `mark_releases` is insert-only in application workflows; database triggers check approved status and HoD permission. Student results are taken exclusively from `student_released_results`.
* `PRAGMA foreign_keys=ON` is enabled for every database connection. Invalid data produces a controlled error and does not silently commit.
* Attachments are copied into managed `data/submissions/` storage; ensure only appropriate local operating-system users can access the data directory. For production, run under a dedicated OS account and implement backup/restore procedures.

## Testing and results

`tests/smoke.cpp` is a CTest executable for login, denied permissions, 4-role workflow, individual extensions, pre-deadline resubmission, 10% late rules, HoD gating, audit records, timestamps, and persistence across database restart.

Use `ctest --test-dir build --output-on-failure` to reproduce. Evidence from the author's container build is preserved in `docs/BUILD_TEST_EVIDENCE.txt`. These are initial implementation smoke/integration tests only; Week 5 will require a formal test plan, line-coverage measurement and acceptance tests.

## Git repository submission (manual account action)

```bash
git init
git branch -M main
git add .
git commit -m "Implement LMS v1.0 Phase 4 baseline"
# On GitHub, create an EMPTY repository first; then substitute YOUR repository URL:
git remote add origin https://github.com/YOUR_ACCOUNT/YOUR_REPOSITORY.git
git push -u origin main
```

If Git requests an identity, run `git config user.name "Your Name"` and `git config user.email "your@school.email"`. Do not claim a hosted repository or completed CI run until the repository has actually been uploaded and Actions has passed.

## Scope and known limitations

* Initial v1.0 supports **individual extensions only**. Whole-class extensions are reserved for Week 7 maintenance CR1 / v1.1.
* This is a local console application. Multi-computer network deployment would require deliberate shared-filesystem locking and access-control evaluation.
* The current code uses local UTC wall-clock timestamps; administrators should synchronize the PC clock. Further clock manipulation mitigations, UI usability testing and backups belong to rollout review.
* Database administrator edits made outside this application can bypass application-level auditing; protect SQLite file permissions and use only authorized application workflows.
* Actual 800-student hardware responsiveness, Phase 5's 70% coverage objective and final school acceptance are not claimed until separately measured.
* The GitHub Actions workflow is **configuration supplied**, not evidence of a hosted successful run.
