# GC DLL Domain Boundary Cleanup

## 目的

本文件记录 GC dll 后续重构方向：减少概念混杂，避免继续为了拆分而拆分文件。当前目标是固定少数 domain 边界、收薄 `Steam_Game_Coordinator` 的业务面、收拢过度分散的 helper。

## 当前判断

当前 GC 生产代码已经拆出 52 个 production GC translation units，`dll/gbe_dota_*.cpp` 与 `dll/steam_game_coordinator.cpp` 合计约 26k 行。继续单纯增加文件或抽新 dll 会提高跳转成本。

当前主要复杂度来自职责混杂：入口调度、lobby 状态、inventory、chat、custom game lifecycle、reconnect、payload/wire、template replay、welcome/post-login side effect 混在同一个 Coordinator 调用面内。

## 固定 Domain

后续只使用这些 domain 名称描述 GC 业务边界：

| Domain | 责任 | 当前主要文件 |
|--------|------|--------------|
| Coordinator | GC 生命周期、顶层 dispatch、队列与跨 domain side effect seam | `steam_game_coordinator.cpp`、`gbe_dota_post_login_dispatcher.cpp` |
| Lobby | lobby create/join/leave/destroy、slot/member、launch、shared/local state | `gbe_dota_lobby_*` |
| MatchRuntime | 7034 runtime、readyup、loading、owner hero observation | `gbe_dota_match_handlers.cpp`、`gbe_dota_custom_game_lifecycle_*` |
| Inventory | equip/unlock/style、item payload、welcome inventory follow-up | `gbe_dota_inventory_*`、`gbe_dota_payload_item_helpers.cpp` |
| ChatBroadcast | chat channel、broadcast channel、postgame chat tombstone | `gbe_dota_chat_*` |
| Reconnect | reconnect context、network adapter、serialized connection state | `gbe_dota_reconnect_*`、`gbe_dota_serialized_connection_state.cpp` |
| PayloadWire | raw wire parsing、payload patching、template/lobby/item helper | `gbe_dota_gc_wire.cpp`、`gbe_dota_payload_*_helpers.cpp`、`gbe_dota_gc_payload_helpers.cpp` |
| TemplateReplay | template-only responses and defensive replay routing | `gbe_dota_template_replay_*` |

## 后续改动规则

1. 新业务逻辑优先进入已有 domain，避免新增平行 domain。
2. `Steam_Game_Coordinator` 只保留 GC 生命周期、顶层 dispatch、队列 plumbing、跨 domain side effect seam。
3. 文件拆分只在能减少单文件认知负担时进行。
4. 只有单一调用点、上下文跳转大、命名相近的 helper 优先考虑回收或合并到 domain 文件。
5. payload/wire helper 保持纯函数输入输出，避免持有 Coordinator 指针、队列、网络或可变 shared state。
6. lobby 状态继续由 `LOCAL_LOBBY_USAGE.md` 和 `STATE_OWNERSHIP.md` 约束，避免绕过现有 source-aware restore/adopt guard。

## 优先清单

| 优先级 | 工作 | 理由 | 停手条件 |
|--------|------|------|----------|
| P0 | 收束文档入口和 Active Queue | 当前文档膨胀会误导后续 Agent 继续机械扩 guard | `ACTIVE_QUEUE.md` 只保留 WIP 和最近 5 条 |
| P1 | Coordinator 调用面清点 | 明确哪些 `Steam_Game_Coordinator::GBE_*` 方法仍承载 domain 业务 | 只产出清单，不搬文件 |
| P1 | Inventory domain cleanup | inventory 与 lobby lifecycle 耦合较低，适合验证 domain seam | handler/coordinator/API 命名清晰，offline tests 通过 |
| P2 | ChatBroadcast domain cleanup | chat/broadcast 已有字段组 guard，可收拢命名和职责 | 不改变 postgame tombstone 顺序 |
| P3 | Lobby full adopt authority design | 涉及 create/join/restore 权威模型，先设计后改码 | 字段 owner 矩阵明确前不改生产代码 |
| P3 | Owner hero authority design | 涉及 host/client/peer restore 和 wearable replay | 先更新 `HOST_AUTHORITY.md` |

## 近期停止项

- 不继续为了降低 TU 数量而合并或拆分文件。
- 不继续横向给所有 full adopt 字段加 generation marker。
- 不直接把 Local/shared 双轨合并成单一状态源。
- 不在 `Steam_Game_Coordinator` 中新增 domain 业务分支。
- 不启动 Phase D 真 DI 或 CompositionRoot 进生产。

## 可接受的下一批代码改动

下一批代码改动应满足全部条件：

1. 只触碰一个 domain。
2. 不改变外部 emsg 行为。
3. 不新增跨 domain 可变状态访问。
4. 有对应 offline test 或 audit guard。
5. 通过 `bash tools/run_gc_offline_tests.sh --full` 与 `bash tools/check_gc_production_tus.sh --jobs 4`。

推荐首个代码批次是 Inventory domain cleanup。目标是让 inventory handler 与 coordinator seam 的命名、入口和 side effect 边界更清楚，避免继续扩大 Lobby 状态重构范围。
