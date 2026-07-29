# Deferred 单轨清单（B.4 完成后）

> 源：`dll/dll/steam_game_coordinator.h` 成员；`dll/steam_game_coordinator.cpp` Set/Has/Clear/Consume。
> 状态：**bool+lobby_id 平行字段已删除**；仅 `GBE_DotaDeferredTaskSlot`。

## 1. 三组 pending 任务（单轨）

| 逻辑名 | Slot 字段 | task_name |
|--------|-----------|-----------|
| abandon finalize after 7014 | `GBE_pending_dota_abandon_finalize_slot` | abandon_finalize_after_7014 |
| normal signout after 25 | `GBE_pending_dota_normal_signout_finalize_slot` | normal_signout_finalize_after_25 |
| reset after cache unsubscribed | `GBE_pending_reset_after_cache_unsubscribed_slot` | reset_after_25 |

Slot 结构：`{ lobby_id, generation, pending }`（`GBE_DotaDeferredTaskSlot`）。

已删除平行字段：`*_after_7014` / `*_after_25` bool、`*_lobby_id`（6 个）。

## 2. API（单轨）

| API | 行为 |
|-----|------|
| `HasPending*` | 读 `slot.pending` |
| `SetPending*` | 写 `{lobby_id, current_generation, pending=true}` |
| `ClearPending*` | slot 置空；`ClearPendingResetAfterCacheUnsubscribed(retained_lobby_id)` 形参保留兼容，实现忽略 retained |
| `ConsumePending*` | `GBE_ConsumeDotaDeferredTask(slot, name)` 仅 Slot 路径 |

## 3. 调用点

| API | 调用位置 |
|-----|----------|
| Set abandon | `gbe_dota_chat_handlers.cpp` ~538 |
| Set/Clear reset；Set normal signout | `gbe_dota_custom_game_lifecycle_coordinator.cpp` ~137–143 |
| Clear reset / abandon | `steam_game_coordinator.cpp` ResetGCMemory 等 |
| Has* 日志/诊断 | `steam_game_coordinator.cpp` RunCallbacks 附近 |
| Consume* 执行 | `steam_game_coordinator.cpp` RunCallbacks 路径 |

## 4. 验证（2026-07-12）

- `gbe_dota_dual_gc_host_test: all passed`（含 `test_deferred_slot_model`）
- `gbe_dota_handler_test`: 86 passed（含 abandon/signout/reset/stale）
