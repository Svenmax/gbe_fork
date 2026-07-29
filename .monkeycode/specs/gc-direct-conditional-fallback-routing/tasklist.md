# Direct Conditional Fallback Routing 任务清单

## 待实施

- [x] 盘点 direct post-login fallback 中全部条件路径与 template catch-all 顺序。
- [x] 先更新 `docs/gc/MESSAGE_ROUTING_INVENTORY.md` 和 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 定义显式 direct conditional fallback boundary，保持 registry 和 template-only 行为不变。
- [x] 增加审计回归，覆盖 `8744`、`5410`、`5432` 与 routing inventory 一致性。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] direct conditional fallback 具有显式边界和审计保护。
- [x] template replay 与 wrapped hard miss 行为保持。
- [x] 全量 GC 验证和 diff check 通过。

## 验证记录

- `python3 tools/test_audit_gc_refactor.py`：89 tests passed。
- `bash tools/run_gc_verification.sh --full`：GC verification passed；handler smoke 92/92；payload helper 555/555；audit issues 全 0。
- `git diff --check`：通过。
