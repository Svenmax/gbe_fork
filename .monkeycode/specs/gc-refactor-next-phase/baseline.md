# GC 后续重构基线

- Baseline commit: `f9d7bc484bf0f57d24a01d5b94e1fbdd7a3a35be`
- Base branch commit: `4882fd2023bc859000729991901f9e197e8acfbd`
- Verification command: `bash tools/run_gc_verification.sh --full --base-sha origin/dev`
- Payload helper assertions: 240 passed
- Handler smoke tests: 64 passed
- Replay fixtures: 7 passed
- GC audit checks: 8 passed with 0 issues
- Diff check: `git diff --check` passed
- Production build gates: Windows and Linux `api_experimental` x64 release required for pull requests

后续阶段完成后，使用相同验证命令，并比较 payload、handler、replay、audit 和 production build 结果。
