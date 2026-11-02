# Configuration control and release procedure

1. Verify source and required files in private GitHub `CS4433-LMS`.
2. Create branch `release/v1.0-candidate` from the approved main commit.
3. Require code review and successful CI checks.
4. Record release candidate commit SHA, toolchain, dependency versions, test results and defects.
5. Only after approval: `git tag -a v1.0 -m "CS4433 LMS v1.0"` and `git push origin v1.0`.
6. Publish a GitHub release with reviewed executable package, install guide, release notes and no real SQLite database.
7. Rehearse install and rollback on Windows 11 and log the results.

**Status:** Instructions only; GitHub tag, independent approval and deployed release are not claimed.
