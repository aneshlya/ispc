Analyze and fix GitHub issue #$ARGUMENTS from https://github.com/ispc/ispc

1. Get issue details: `gh issue view $ARGUMENTS --repo ispc/ispc`
2. Explain the problem and reproduce it
3. Search codebase for relevant files
4. **Create fix plan and get user approval**
5. Create branch: `git checkout -b fix-issue-$ARGUMENTS`
6. Implement the fix
7. Run `clang-format -i` on modified C/C++/header files
8. Write regression tests in `tests/lit-tests/` and verify the fix
9. Commit with message: `Fix #$ARGUMENTS: <summary>`
10. Show results and next steps

Stop and ask if anything is unclear or fails.
