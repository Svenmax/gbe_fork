# GC 重构基线

## 用途

本文件记录每个重构阶段开始前的可验证基线。重构提交应保持行为范围可说明、构建结果可重复、测试证据可追溯。

## 当前基线

| 项目 | 值 |
| --- | --- |
| 基线日期 | 2026-07-24 |
| 工作分支 | `trae/agent-inRF11` |
| 基线提交 | `d91ee679` |
| 对比主线 | `origin/dev...HEAD` |
| Dota GC 改动规模 | 281 个文件，约 79,257 行新增、20,209 行删除 |
| 主协调器当前规模 | `dll/steam_game_coordinator.cpp` 约 1,716 行 |

## 当前验证证据

已执行：

```bash
./tools/run_gc_offline_tests.sh
```

在 120 秒命令时限前，以下项目已通过：

- header compile checks
- `audit_gc_refactor_test`，74 个 Python 测试
- `gc_message_utils_test`
- `gbe_gc_config_test`
- `gbe_dota_reconnect_network_test`，772 条断言
- `callsystem_execution_guard_test`
- `gbe_dota_lobby_state_store_test`
- `gbe_dota_dual_gc_host_test`
- `gbe_dota_locator_test`
- `gbe_dota_composition_root_test`
- `gbe_dota_lifecycle_state_machine_test`
- `gbe_dota_concurrency_stress_test`
- `gbe_dota_handler_registry_test`，339 条断言
- `gbe_proto_wire_test`
- `gc_replay_test` 的 `minimal`、`practice_lobby`、`game_flow`、`cache_and_items` fixture
- `gbe_dota_gc_payload_helpers_test`，555 条断言
- `gbe_dota_handler_test`，90 个场景

脚本在开始构建 `gbe_dota_behavior_replay` 时达到当前执行环境的 120 秒时限。因此完整脚本仍需在具备更长时限的 CI 或开发机完成，并将结果补充到本文件。

## 已验证行为范围

- Dota Lobby 的创建、加入、退出、销毁和基础 launch/postgame 路径。
- Custom game 的 ready-up、loading、finished-loading 的 direct/wrapped action sequence。
- 7034 runtime member、game state、launch poll 和 host wearable side effect 顺序。
- cache subscription、inventory equip/style、chat leave、invite 和 broadcast channel 的部分顺序约束。
- shared lobby Store 的 snapshot、generation gate、compare update、compare clear。
- reconnect network、dual GC host 和并发压力的离线场景。

## 当前未闭合风险

- `GBE_PublishSharedDotaLobbyState()` 已切换到锁内 generation-gated mutator；跨角色字段级写入集合仍待在后续生命周期收敛阶段明确。
- 生产装配仍以 `Steam_Game_Coordinator` 与 `Steam_Client` 为中心；`CompositionRoot` 当前仅用于离线测试。
- `global_mutex`、Store mutex 与 reconnect 相关访问还未形成机器可验证的锁顺序。
- 生命周期信息同时存在于 LocalLobby、SharedLobby 和 lifecycle state machine。
- 离线 fixture 覆盖不能代替真实版本协议捕获和生产平台编译。
- 当前工作区缺少 `proto_gen/linux/net.pb.h`，直接编译 `steam_client.cpp` 和 `steam_game_coordinator.cpp` 会在该生成文件处中止；生产目标验证需要先运行项目的 protobuf 生成步骤。
- 当前环境未发现 `protoc`，因此无法在本地恢复缺失的 protobuf 生成产物。

## 阶段验收模板

每次开始新阶段时复制本节并填写。

```markdown
## 阶段：R?

- 起始提交：
- 目标：
- 禁止改变的行为：
- 修改范围：
- 新增或更新 fixture：
- 离线验证：
- Linux 生产编译：
- Windows 生产编译：
- 真实协议样本回放：
- 结论：
```
