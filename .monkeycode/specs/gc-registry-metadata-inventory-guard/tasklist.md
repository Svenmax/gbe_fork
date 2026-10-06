# Registry Metadata Inventory Guard 任务清单

## 待实施

- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 扩展 `audit_registry_inventory_guard` 校验 HandlerId、modes、lifecycle。
- [x] 增加审计回归，覆盖 production metadata drift 与 inventory metadata drift。
- [x] 更新 `docs/gc/MESSAGE_ROUTING_INVENTORY.md` 标记 metadata audit 保护。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] registry metadata 由 inventory 审计保护。
- [x] emsg set guard 保持。
- [x] 全量 GC 验证和 diff check 通过。
