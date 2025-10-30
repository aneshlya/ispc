Please analyze and fix the GitHub issue: $ARGUMENTS.

The repo is: https://github.com/ispc/ispc
Follow these steps:

1. Use `gh issue view` to get the issue details
2. Understand and if possible reproduce the problem described in the issue
3. Search the codebase for relevant files
4. Create a plan on how are you going to fix the issue and show it for the approve to the user
5. Implement the necessary changes to fix the issue
6. Run `clang-format` on modified `c/cpp/h` files
7. Write and run tests to verify the fix. Regression tests go to `tests/lit-tests` folder. The name of test should be $ARGUMENTS.ispc
8. Create a new branch and descriptive commit message

Remember to use the GitHub CLI (`gh`) for all GitHub-related tasks.
