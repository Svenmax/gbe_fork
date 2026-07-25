# Phase D 边界（评估；默认不实施）

> 对照代码日期：2026-07-13。**本文件是边界与停手清单，不授权大拆或生产 DI。**
> 深入既有材料：`coordinator-boundaries.md`、`dependency-ownership-map.md`、`dependency-seams.md`、`future-refactor-plan.md`。

## 1. Phase D 目标（代码 KPI）

| KPI | 含义 | 完成信号 |
|-----|------|----------|
| Handler 脱离上帝类 | 新业务副作用经窄 seam / action executor，不在 `steam_game_coordinator.cpp` 堆逻辑 | 新路径不进 core TU；审计 seam 仍绿 |
| 真 DI（可选） | 服务依赖可构造、可测，生产装配仍显式 | **CompositionRoot 仍仅 offline**；生产继续 `steam_client.cpp` |
| 可测子面 | 子对象/ facade 依赖集小，能直测 | 构造参数 ≤ 显式小 context，无需整颗 `Steam_Game_Coordinator *` |

**非目标：** 为行数/OO 外观拆类；CompositionRoot 进生产；一次 PR 同时做状态边界 + 副作用边界 + 对象分解。

## 2. 现状快照（评估基线）

| 项 | 现状 |
|----|------|
| `Steam_Game_Coordinator` | ~1.7k `.cpp` + ~0.5k `.h`；仍是集成宿主与成员 handler 面 |
| 已拆 TU | 多份 `*_handlers.cpp` / `*_coordinator.cpp`（~25+），多数仍是 **成员函数实现文件**，不是独立可注入服务 |
| Lifecycle | Phase C 已收口：具名 gate + `decide_*` / `*_action_list` → Execute |
| Shared lobby | Store 门控写；读用 snapshot；禁止裸 `publish`/`update` |
| CompositionRoot | 存在且 offline 测绿；**禁止生产构造**（CURRENT 硬规则 #3） |
| 上帝类风险 | CURRENT 风险 #3；双轨 local/shared 为风险 #2 |

## 3. 准入条件（全部满足才允许动生产代码）

1. 目标路径已有 **行为/序** 护栏（handler smoke 或 focused pure + executor 测）。
2. 依赖能收成 **小显式 context**（见 `dependency-ownership-map` Extraction Gate），不传整颗 coordinator。
3. 单 PR 只碰 **一个** 边界：state **或** side-effect **或** 对象提取，不混做。
4. 先改真相表（本文件 + 相关 inventory），再改代码。
5. `bash tools/run_gc_verification.sh --full` 与 `git diff --check` 绿。

## 4. 推荐切片顺序（若启动实施）

只在准入满足时按序推进；**默认本周不实施**。

| 优先级 | 切片 | 为何 | 停手 |
|--------|------|------|------|
| D0 | 维持 seam 纪律 | 新逻辑只进 handler/coordinator TU + action/plan | 不向 `steam_game_coordinator.cpp` 加业务 |
| D1 | Response / push 窄 facade（可选） | 高风险序可见；`response-seam-status` 已有债 | 不机械扩 helper；无 recorder 不迁 |
| D2 | Launch-state 剩余 context | planner/action 已稳；restore/capture/log 仍粘 coordinator | 无小 context 不抽 service |
| D3 | 单一 publish/details 路径 | 双轨耦合重；需一条路径有完整序测 | 不并行改 HOST 字段 |
| D4 | 自然子对象（远期） | 仅当依赖收敛且可直测 | 禁止「搬家式」成员迁移 |

### 已完成受控切片：queued launch/runtime apply

- `Steam_Game_Coordinator::GBE_ApplyQueuedLobbyState()` 继续以 `compose_queued_lobby_state_apply_plan()` 计算 queued 输入，随后仅通过 `apply_queued_lobby_state_apply_plan()` 应用 `state`、`game_state` 与 `launch_phase`。
- `Steam_Game_Coordinator::GBE_MarkDotaLaunchPhase()` 仅通过 `advance_launch_phase()` 推进 Local `launch_phase`；既有 generation-gated shared publish 与诊断顺序保持。
- generic lobby capture 通过 `compose_generic_lobby_state_capture_plan()` 决定 `state` / `game_state` 的独立写许可，再由 `apply_generic_lobby_state_capture_plan()` 应用；custom-game 启动后的回退拒绝和 stale-state 日志保持。
- shared restore 通过 `compose_shared_lobby_runtime_restore_plan()` 决定 `state`、`game_state` 与 `launch_phase` 的字段组更新，再由 `apply_shared_lobby_runtime_restore_plan()` 应用；client restore/host 观察语义与 custom-game READYUP 回退拒绝保持。
- steam-auth ack 路径通过 `compose_steam_auth_ack_launch_plan()` 和 `apply_steam_auth_ack_launch_plan()` 一次更新 CRC、sequence 和 ack 标记；generation-gated shared publish 与 synthetic push 顺序保持。
- 匹配 lobby 的 4511 通知仅通过 `mark_launch_4511_seen()` 更新去重标记；首次变更后的 generation-gated shared publish 与 server-id 同步顺序保持。
- shared-to-local 4511 标记恢复仅通过 `restore_launch_4511_seen()` 更新；client restore 的 `changed` 聚合语义保持。
- `LobbyStateApply` lifecycle action 仅通过 `apply_lifecycle_lobby_state()` 更新 `state` 和 `game_state`；action 执行顺序、条件 gate 与后续 publish 保持。
- Local 工作副本、host-only generation-gated shared publish 与 client restore/观察语义保持。
- rich presence、peer push 与 shared publish 仍处于既有顺序；本切片未提取对象，未引入生产 CompositionRoot。

**明确延后：** 真 DI 替换生产装配；`DotaLobbyRuntime` 等大对象包；全量 getter 假封装。

## 5. 与 Phase C / E 的分界

| Phase | 负责 | 不负责 |
|-------|------|--------|
| C（已收口） | Legacy effect 归零；lifecycle decide/action 化 | 拆上帝类 |
| **D** | 集成宿主瘦身、依赖可测、handler 不扩 core | 真协议 L3/L4 验收升级 |
| E | 行为/协议测补强（GOLDEN_PATHS / dual_gc / replay） | 为测而大拆架构 |

## 6. 硬停手（违反即停）

- CompositionRoot 进入生产构造路径
- 为美观横向大拆 / 一次多边界混改
- 无测试先改 lifecycle / shared publish / host 权威字段
- 回退 `Legacy*` EffectKind 或绕过 production registry
- 把整颗 `Steam_Game_Coordinator *` 塞进新「服务」当万能依赖

## 7. 默认下一动作

1. **文档态：** 认领本边界；实施前在 `ACTIVE_QUEUE` 写一条 ≤1 的 D 切片（D0/D1…）。
2. **若只修 bug：** 走现有 handler/coordinator + action 模式，不开启 Phase D 大项。
3. **若补协议测：** 优先 Phase E 预备（GOLDEN_PATHS / smoke），与 D 解耦。

## 8. 维护

- 实施任一 D 切片后：更新本表 §2 基线与 §4 状态。
- 冲突时以 `CURRENT.md` 硬规则 + 本文件停手为准；长文 `future-refactor-plan.md` 作历史设计，不覆盖本边界。
