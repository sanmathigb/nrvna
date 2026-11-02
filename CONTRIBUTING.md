# Contributing

Small, focused changes are easiest to review. Open an issue before large
changes to the job contract or public command interface.

## Build and check

Build the source tree and run the contract tests:

```sh
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Keep command output machine-readable. Send diagnostics to stderr. Update the
README or reference docs when a change affects user behavior.

Do not include model files, workspaces, local indexes, or generated media in a
pull request. Do not edit `third_party/llama.cpp` without reviewing the upstream
change and updating the submodule pointer deliberately.
