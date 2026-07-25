# Registry Inventory Guard 任务清单

## 待实施

- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 增加 protocol constants 解析和 registry/inventory emsg set 审计。
- [x] 增加审计回归，覆盖当前通过、kTable drift、inventory drift。
- [x] 更新 `docs/gc/MESSAGE_ROUTING_INVENTORY.md` 标记 registry inventory audit 保护。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] production registry emsg 集合由 inventory 审计保护。
- [x] `GBE_k*` constants 在审计中可解析。
- [x] 全量 GC 验证和 diff check 通过。
