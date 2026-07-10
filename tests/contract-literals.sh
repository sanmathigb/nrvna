#!/usr/bin/env bash
# Contract discipline: if a string names a file or directory inside a
# workspace, it lives in contract.hpp or it's a bug.
set -euo pipefail
cd "$(dirname "$0")/.."

pattern='"(input/ready|input/writing|processing|output|failed|prompt\.txt|type\.txt|result\.txt|error\.txt|embedding\.json|transcript\.txt|audio\.wav|meta\.json)"'
violations="$(grep -rnE "$pattern" src cli include \
    --include='*.cpp' --include='*.hpp' \
    | grep -v 'include/nrvna/contract.hpp' || true)"

if [ -n "$violations" ]; then
    echo "contract literals outside contract.hpp:" >&2
    echo "$violations" >&2
    exit 1
fi
echo "contract-literals: clean"
