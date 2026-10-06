# Wrapped Hard Miss Routing 任务清单

## 待实施

- [x] 盘点 wrapped post-login miss hard-stop 顺序与 B4 dead fallback 禁止项。
- [x] 先更新 `docs/gc/MESSAGE_ROUTING_INVENTORY.md` 和 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 定义显式 wrapped hard-miss boundary，保持 registry handled path 和 extract gates 不变。
- [x] 增加审计回归，覆盖 hard miss helper、顺序、template replay 缺席和 routing inventory 一致性。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] wrapped hard miss 具有显式边界和审计保护。
- [x] direct fallback、template replay 与 registry 主路径行为保持。
- [x] 全量 GC 验证和 diff check 通过。

## 验证记录

- `python3 tools/test_audit_gc_refactor.py`：93 tests passed。
- `bash tools/run_gc_verification.sh --full`：GC verification passed；handler smoke 92/92；payload helper 555/555；audit issues 全 0。
- `git diff --check`：通过。
