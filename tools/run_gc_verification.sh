#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FULL=1
RUN_AUDIT=1
RUN_STYLE=1

usage() {
    printf 'usage: %s [--fast] [--full] [--skip-audit] [--skip-style]\n' "$(basename "$0")"
    printf '  --fast        run fast offline tests instead of --full\n'
    printf '  --full        run full offline tests (default)\n'
    printf '  --skip-audit  skip tools/_audit_gc_refactor.py\n'
    printf '  --skip-style  skip git diff --check\n'
}

for arg in "$@"; do
    case "$arg" in
        --fast)
            FULL=0
            ;;
        --full)
            FULL=1
            ;;
        --skip-audit)
            RUN_AUDIT=0
            ;;
        --skip-style)
            RUN_STYLE=0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
done

cd "$ROOT_DIR"

if [[ "$FULL" -eq 1 ]]; then
    tools/run_gc_offline_tests.sh --full
else
    tools/run_gc_offline_tests.sh
fi

if [[ "$RUN_AUDIT" -eq 1 ]]; then
    python3 tools/_audit_gc_refactor.py
fi

if [[ "$RUN_STYLE" -eq 1 ]]; then
    git diff --check
fi

printf 'GC verification passed\n'
