# Template Only Inventory Guard 任务清单

## 待实施

- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 增加 `TEMPLATE_ONLY` expected whitelist 和 switch/inventory 审计。
- [x] 增加审计回归，覆盖当前通过、production switch drift、inventory drift。
- [x] 更新 `docs/gc/MESSAGE_ROUTING_INVENTORY.md` 标记 audit 保护。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] template-only whitelist 由审计保护。
- [x] registry-defensive 与 template-only 归属保持分离。
- [x] 全量 GC 验证和 diff check 通过。
