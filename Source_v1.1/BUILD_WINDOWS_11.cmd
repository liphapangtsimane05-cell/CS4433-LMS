@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>&1 || (echo CMake is required. Install CMake first. & pause & exit /b 1)
where g++ >nul 2>&1 || (echo MinGW-w64 g++ is required. Run this in an MSYS2 UCRT64 terminal, or set up its PATH. & pause & exit /b 1)
cmake -S . -B build-windows -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
if errorlevel 1 goto fail
cmake --build build-windows --config Release -j 4
if errorlevel 1 goto fail
ctest --test-dir build-windows --output-on-failure
if errorlevel 1 goto fail
echo.
echo Build succeeded: build-windows\lms.exe
echo Copy OpenSSL, SQLite and MinGW runtime DLLs from your MSYS2 UCRT64 bin folder next to lms.exe.
pause
exit /b 0
:fail
echo Build failed. Verify CMake, SQLite3 development headers/libraries, and OpenSSL in MSYS2 UCRT64.
pause
exit /b 1
