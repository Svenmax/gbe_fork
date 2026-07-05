# GC Refactor Pre-Merge Checklist

This checklist records the local gates for GC refactor changes. Run the smallest gate that matches the change risk while developing, then run the full gate before handing off a completed stage.

## Fast Gate

Use this for small pure-helper, parser, or focused handler changes while iterating.

```bash
tools/run_gc_offline_tests.sh
```

Expected coverage:
- Core GC message utility tests
- GC config tests
- Proto wire tests
- High-signal replay fixtures
- Payload helper tests
- Handler smoke tests

## Full Gate

Use this before marking a tasklist item complete.

```bash
tools/run_gc_offline_tests.sh --full
```

Expected additional coverage:
- Chat channel replay fixture
- Lobby lifecycle replay fixture
- Wire edge-case replay fixture
- Dota lobby flow focused tests
- Dota lobby state focused tests
- Dota custom game focused tests

## Audit Gate

Use this with the full gate for GC refactor changes that move declarations, split helpers, change dispatch, or update docs with line-sensitive references.

```bash
python3 tools/_audit_gc_refactor.py
```

The audit checks:
- Header declarations have matching definitions
- Shared free-function definitions have declarations
- `REFACTOR_TODO.md` line references remain accurate where applicable
- Post-login dispatch table mappings stay aligned with expected handlers

## Style Gate

Use this after edits and before reporting completion.

```bash
git diff --check
```

This catches trailing whitespace and whitespace errors in the current diff.

## Premake Gate

Use this when build files, project configuration, or compile-unit membership changes.

```bash
./premake5 --with-gc-tests gmake2
```

Then build the generated target for the affected platform/toolchain when available. This gate is optional for logic-only refactors that only touch files already compiled by `tools/run_gc_offline_tests.sh --full`.

## Completion Rule

Before marking a GC refactor task complete, run:

```bash
tools/run_gc_offline_tests.sh --full
python3 tools/_audit_gc_refactor.py
git diff --check
```

Record the pass result in `.monkeycode/specs/gc-refactor-next-steps/tasklist.md` under the completed task.

The same sequence is also available through the wrapper script:

```bash
tools/run_gc_verification.sh
```

For iterative local checks, use:

```bash
tools/run_gc_verification.sh --fast
```

The wrapper preserves `tools/run_gc_offline_tests.sh` as the primary offline test entry point and only adds audit/style orchestration around it.
Run `tools/run_gc_verification.sh --help` to inspect wrapper options before using skip flags.
