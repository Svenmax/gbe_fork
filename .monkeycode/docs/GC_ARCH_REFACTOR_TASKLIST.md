# GC 架构收口任务清单

## 目标

- 让 `Steam_Game_Coordinator` 回到 GC 基础设施角色。
- 让 Dota lobby 生命周期进入独立的 Dota domain。
- 让高风险副作用通过统一 action/executor 承接。
- 让后续 GC 维护显著变轻。

## 执行原则

- 按小步渐进执行，每轮只改一条高风险路径。
- 每轮完成后运行 `tools/run_gc_verification.sh`。
- 优先收敛职责边界，后做物理文件拆分。
- 保持协议顺序、payload 输出和 replay 行为稳定。

## 第一轮

- [ ] 1. 将 `Steam_Game_Coordinator` 明确为 GC 基础设施入口
  - 收敛职责说明，后续 handler 只负责消息入口和流程编排。
  - 让 Dota lobby 生命周期逻辑进入 planner/domain 层。

- [ ] 2. 改造 `GBE_PushDotaLaunchStateToClientPeer` 为样板路径
  - [x] 建立完整 launch push context，集中收集 source lobby、target、shared snapshot、captured lobby、last pushed game state。
  - [x] 将多段 `LaunchStatePushPlanInput` 填充收敛为 context -> planner 入口。
  - [x] 让 planner 返回 skip reason、payload build request、action sequence。
  - [x] 将 launch push 动作接入统一 `GBE_DotaActionList` 或等价统一 action 模型。
  - [x] 保持顺序为 `RecordCacheSubscription -> PushCacheSubscribed -> PushDetailsUpdate -> ReapplyRichPresence -> SetLastGameState`。
  - [ ] 继续评估是否可以把 restore/capture 前的阶段性 skip 判断进一步合并，同时保持日志粒度。

- [ ] 3. 为 launch push 补强测试护栏
  - [x] 增加 planner 单测，覆盖 valid push、suppressed source lobby、invalid target、suppressed shared lobby、no captured lobby、duplicate game state。
  - [x] 增加 context mapping 和统一 action list 顺序单测。
  - [ ] 增加 smoke test，断言 launch push 的 action sequence 与 skip path 行为。
  - [ ] 为 handler smoke test 包装器纳入 launch coordinator，补齐必要 stub 后再接入 `GBE_PushDotaLaunchStateToClientPeer` 行为测试。

- [ ] 4. 改造 `GBE_HandleDotaPracticeLobbyCreateRequest`
  - 建立 create lobby context，集中 request、pre-reset lobby、custom game、generic lobby 可用性、settings 状态读取。
  - [x] 使用现有 reset plan 决定 previous cache unsubscribed，并把 25 纳入 create action list。
  - 抽出 reset plan，决定 `ResetGCMemory`、previous cache unsubscribed、reconnect context 处理。
  - 抽出 create lobby state plan，集中生成新的 local lobby 状态。
  - [x] 将 `PushCacheUnsubscribed`、publish、record cache subscription、push 24、push 7055 收敛到统一 create action list。
  - 将 `ResetGCMemory`、`CreateGenericLobby` 收敛到 action list。
  - [x] 保持 create path 顺序为 `25 -> 24 -> 7055`。

- [ ] 5. 为 create lobby 补强测试护栏
  - [x] 增加 create action list 单测，覆盖可选 25 以及 `24 -> 7055` 顺序。
  - 增加 smoke test，覆盖真实 handler 的 `25 -> 24 -> 7055`。
  - [x] 验证 record cache subscription 在 push 前。
  - 验证 custom game create 的状态归一化。

- [ ] 6. 改造 `GBE_HandleDotaPracticeLobbyJoinRequest`
  - 建立 join lobby context，集中 request lobby id、pass key、matched generic lobby、当前 local lobby、settings sync 状态读取。
  - 抽出 join lobby merge plan，决定 JoinLobby、SyncSettingsLobby、pass key 更新、local lobby 合并。
  - [x] 将 JoinLobby、SyncSettingsLobby、publish、record cache subscription、push 24、push 7113 收敛到 join action list。
  - [x] 保持 join path 顺序为 `24 -> 7113`。

- [ ] 7. 为 join lobby 补强测试护栏
  - [x] 增加 action list 单测，覆盖 direct join、matched generic lobby、无 join response 的 `24` only 顺序。
  - [x] 保持 smoke test 覆盖 matched generic lobby 和 `24 -> 7113` 顺序。
  - 增加 smoke test，覆盖 empty local lobby、pass key。

- [ ] 8. 第一轮检查点
  - 运行 `tools/run_gc_verification.sh`。
  - 确认 launch/create/join 三条路径通过 full verification。
  - 确认新增逻辑没有扩大 handler 直接写 `GBE_local_lobby` 和直接执行副作用的范围。

## 第二轮

- [ ] 9. 收敛 `gbe_dota_lobby_flow` 的功能混杂
  - 拆出 `gbe_dota_lobby_launch_flow.{h,cpp}`，承载 launch push context、planner、skip reason、payload build request。
  - 拆出 `gbe_dota_lobby_member_flow.{h,cpp}`，承载 member 查找、upsert、connected/hero、owner transfer、slot normalize。
  - 拆出 `gbe_dota_lobby_payload_flow.{h,cpp}`，承载 cache/details payload routing、authoritative payload data 选择规则。
  - 拆出 `gbe_dota_chat_flow.{h,cpp}`，承载 chat display name 和 join chat member list。
  - 更新测试源列表和审计白名单，保持 `tools/run_gc_offline_tests.sh` 与审计脚本通过。

- [ ] 10. 改造 abandon / signout / postgame 生命周期收尾
  - 建立 teardown context，集中 active lobby、launch failed before connect、ready for abandon teardown、postgame channel、pending flags、shared state。
  - 建立 teardown plan，统一表达 abandon、normal signout、postgame、cache unsubscribe、shared state clear 的状态转移。
  - 将 `DiscardLaunchMessages`、`MarkAbandonedSuppressed`、`PushCacheUnsubscribed`、`SetPendingReset`、`QueuePostGameJoin`、`ClearSharedState`、`LeaveGenericLobby`、`ClearLocalLobby` 收敛到 action list。
  - 保持 25、7010、shared state clear、generic lobby leave 的时序稳定。

- [ ] 11. 为 teardown 路径补强测试护栏
  - 增加 replay fixture，覆盖 abandon、postgame channel leave、normal signout、host/client shared state。
  - 增加 smoke test，覆盖 cache unsubscribe、pending reset、postgame join、shared state clear 的 action 顺序。

- [ ] 12. 第二轮检查点
  - 运行 `tools/run_gc_verification.sh`。
  - 确认拆分后无 header zombie declaration、无 source-list inclusion 问题、无 replay 行为回归。

## 收尾验收

- [ ] 13. 完成架构收口验收
  - handler 主要承担 `parse -> context -> planner -> payload builder -> executor -> log`。
  - launch/create/join/teardown handler 中直接写 `GBE_local_lobby` 的新增逻辑显著减少。
  - launch/create/join/teardown handler 中直接 `push_incoming_now`、publish、generic lobby 操作显著减少。
  - Dota lobby domain 负责业务决策，coordinator/executor 负责执行副作用。
  - `gbe_dota_lobby_flow` 不再同时承载 launch、chat、member、payload 多类职责。

## 建议执行顺序

1. 先完成第一轮的 2、3、4、5、6、7、8。
2. 样板稳定后再完成第二轮的 9、10、11、12。
3. 最后执行 13 做收尾验收。
