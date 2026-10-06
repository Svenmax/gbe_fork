# gc-parser-generic-section-end tasklist

## Tasks

- [x] Create requirements/design/tasklist spec.
- [x] Make parser §5 end at any next level-2 heading.
- [x] Add regression test for missing separator after parser §5.
- [x] Update `docs/gc/CURRENT.md` and `docs/gc/ACTIVE_QUEUE.md` completion notes.
- [x] Run `python3 tools/test_audit_gc_refactor.py`.
- [x] Run `python3 tools/_audit_gc_refactor.py`.
- [x] Run `git diff --check`.
- [x] Run `bash tools/run_gc_verification.sh --full`.

## Done Criteria

- [x] Parser §5 parsing ignores rows after any following level-2 heading.
- [x] Production C++ routing behavior remains unchanged.
- [x] Full GC verification and diff check pass.
