# GitHub Actions CI — setup and proof

1. Create a GitHub repository for this LMS. Upload the contents of **Updated_Source** to the repository **root** (so `CMakeLists.txt` and `.github/workflows/ci.yml` are at the root). Never commit real `data/lms.db`, passwords or student records.
2. Open GitHub → Actions → `LMS C++17 Build and Tests`. Push a commit or choose `Run workflow`.
3. Confirm the Linux and Windows jobs are green. Download the Windows artifact from a successful run. The uploaded executable is **not a standalone installer**; required runtime DLLs must be deployed alongside it and verified on Windows 11.
4. Capture screenshots of the two successful jobs and test logs for submission; don't claim success until they run.
5. For review evidence, open a pull request, ask a second reviewer to inspect the changes, record comments/corrections and approval.

Local verification (Linux, 2026-11-01): `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON`, `cmake --build build`, `ctest --test-dir build --output-on-failure` — 1 suite / 1 passed. This is **not** evidence of a GitHub Actions or Windows execution.

Dependencies: SQLite3 and **OpenSSL Crypto** (PBKDF2-HMAC-SHA256); not libsodium. Official Windows CI uses MSYS2 UCRT64.
