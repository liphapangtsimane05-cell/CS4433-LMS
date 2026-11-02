# Week 4 Build Guide — Moderated LMS v1.0

## Platform and prerequisites

The system is a console application built in C++17 and does not require a MySQL server, XAMPP, internet connection, or database GUI at runtime. You must have a recent C++17 compiler, CMake 3.16+, SQLite3 development files, and OpenSSL libcrypto development files.

### Windows 11, VS Code and MSYS2 UCRT64

1. Download and install MSYS2 from https://www.msys2.org/.
2. Open the **MSYS2 UCRT64** terminal (not the generic MSYS shell).
3. Run `pacman -Syu`. Reopen the UCRT64 terminal if instructed, and finish the update.
4. Install required tools:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
      mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 \
      mingw-w64-ucrt-x86_64-openssl git
   ```
5. Extract the source ZIP and navigate to its root directory in the UCRT64 terminal. You should see `CMakeLists.txt` and `sql/schema.sql`.
6. Configure, build and test:

   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```
7. Create the first administrator exactly once using `./build/lms.exe --init-admin`; then start normally using `./build/lms.exe`.
8. Optional: launch VS Code from the same UCRT64 terminal (`code .`) to reuse its compiler environment. Add the CMake Tools extension if needed.

**Windows build troubleshooting:** If CMake cannot find SQLite or OpenSSL, confirm that the *UCRT64* versions of the libraries were installed and that CMake is running inside the UCRT64 terminal. If the runtime complains about missing `.dll` files, start from the same UCRT64 terminal whose `PATH` includes `/ucrt64/bin`.

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake git libsqlite3-dev libssl-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/lms --init-admin
./build/lms
```

## Database and workflow

SQLite creates `data/lms.db` on first execution and loads the full schema from `sql/schema.sql`. It uses a local file and runs offline. Each login requires the username and password originally set for that account. A successful administrator should create departments, users, courses, lecturer-course assignments and student enrolments before lecturers and students can use their menus.

Use deadlines in strict ISO UTC format, such as `2026-11-07T20:00:00Z` (UTC, not local clock time). A local attachment may be included; the file is copied to `data/submissions/` and limited to 10 MiB.

## Running the demonstration tests

```bash
ctest --test-dir build --output-on-failure
./build/lms_tests
```

The executable shows the total number of assertions passed. `docs/BUILD_TEST_EVIDENCE.txt` contains the local compiler and test logs from preparation of this submission.

## Publishing as the required Git repository

After verifying the local build, create an **empty** repository under your own GitHub account, then run:

```bash
git init
git branch -M main
git add .
git commit -m "CS4433 Week 4: implement moderated LMS v1.0"
git remote add origin https://github.com/YOUR_ACCOUNT/YOUR_REPOSITORY.git
git push -u origin main
```

On the repository's **Actions** tab, verify the build-and-test workflow succeeds. Submit the real repository URL to your lecturer. The template URL above is intentionally a placeholder; a live repository was not created for you.
