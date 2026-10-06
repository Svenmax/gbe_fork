# Template Registry Defensive Routing 任务清单

## 待实施

- [x] 盘点 template replay 中全部 `REGISTRY_DEFENSIVE` cases 与 registry 主路径。
- [x] 先更新 `docs/gc/MESSAGE_ROUTING_INVENTORY.md` 和 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 定义显式 registry-defensive routing boundary，保持 template-only cases 不变。
- [x] 增加 ownership/audit 或 focused 回归，覆盖 `8879`、`8095`、`8009`、`7091`。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] registry-defensive 转调具有显式边界和审计保护。
- [x] template-only replay 与 post-login registry 行为保持。
- [x] 全量 GC 验证和 diff check 通过。

## 验证记录

- `python3 tools/test_audit_gc_refactor.py`：85 tests passed。
- `bash tools/run_gc_verification.sh --full`：GC verification passed；handler smoke 92/92；audit issues 全 0。
- `git diff --check`：通过。
