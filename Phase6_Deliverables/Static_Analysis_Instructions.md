# Optional static analysis gate (not yet executed)

With cppcheck installed, run from repository root:

```sh
cppcheck --enable=warning,style,performance,portability --std=c++17 --inline-suppr --error-exitcode=1 -I include src 2> cppcheck-report.txt
```

Record tool version, output and disposition of findings. Do not claim a clean static analysis run until output has been inspected. `clang-tidy` may be added separately after generating compile_commands.json with CMake.
