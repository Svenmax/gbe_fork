# GC 行为闸与状态收口实施计划

> 依据：独立代码摸底（`Steam_Game_Coordinator` 双实例 + 共享 Store、post-login 分发双轨、deferred 双轨、host hero/wearable/7034/2569 热路径）。  
> 原则：先测后改；一次只收一条真相源；无失败用例不改 host 业务逻辑；禁止仅以 docs 关闸作为完成标准。  
> 验证入口：`bash tools/run_gc_verification.sh`（阶段 A/B 可先加子集目标，最终仍需全量绿）。

## 阶段 A — 地图与闸门（不改生产 host 行为）

- [x] 1. 固化 host 状态权威表（可执行约束）
  - [x] 1.1 在测试源内建立 `HOST_AUTHORITY_TABLE` 注释块（或同目录 `host_authority.md` 仅作表，不写关闸叙事）
    - 产出：`.monkeycode/specs/2026-07-12-gc-behavior-gate-stabilization/host_authority.md`
    - 字段覆盖：`owner_hero_id` local/shared、preserve publish/adopt、peer restore、equipped cache、showcase/wearable one-shot。
    - 代码锚点：`gbe_dota_lobby_state.cpp`、`match_handlers`、`inventory_handlers`、`steam_game_coordinator.cpp`。
  - [x] 1.2 建立 post-login 入口清单（registry vs if 兜底）
    - 产出：`post_login_entry_inventory.md`（registry ~27 条 + if 兜底高风险表）。
  - [x] 1.3 建立 deferred 双轨现状清单
    - 产出：`deferred_dual_track_inventory.md`（三组 bool/Slot 双写 + 调用点）。

- [x] 2. 实现双 GC host 测试 harness（少 stub，对齐生产装配语义）
  - [x] 2.1 新建测试目标目录与构建接入
    - 目录：`tools/gbe_dota_dual_gc_host_test/`
    - 接入：`tools/run_gc_offline_tests.sh`（default 路径，非仅 --full）。
  - [x] 2.2 实现 `DualGcFixture` 构造
    - 共享 `Store` + client/server 两份 `GBE_LocalLobby` + 各自 generation counter + server one-shot 键。
    - 语义对齐 `steam_client` 双角色共享 Store；未链接完整 `Steam_Game_Coordinator`（Phase A 纯状态闸，避免 stub 假绿）。
  - [x] 2.3 实现消息与队列辅助（Phase A 范围）
    - peer restore / publish / adopt / one-shot 键辅助已落地。
    - **未做**：真 7034/2569 body 注入与 outbound 队列 drain（留待完整 GC 链接或 Phase B 扩展）。
  - [x]* 2.4 为 harness 增加最小烟雾用例
    - `test_harness_smoke`：双 local + publish shared 无崩溃。

- [x] 3. 落地 host 五场景（允许先红，禁止为绿改生产逻辑）
  - [x] 3.1 H1：server hero 0 + client 已知 → peer restore 填 server（纯规则镜像 match_handlers）
  - [x] 3.2 H2：showcase one-shot 同 generation 只一次
  - [x] 3.3 H3：wearable one-shot 同 steam+hero 只一次
  - [x] 3.4 H4：generation 前进后 one-shot 失效可再 mark
  - [x] 3.5 H5：adopt 保留 known local hero；publish 保留 known shared hero
  - [x] 3.6 检查点：harness 可运行；H1–H5 有明确 assert
    - 运行结果（2026-07-12）：`gbe_dota_dual_gc_host_test: all passed`
    - **未改任何生产 host 业务逻辑。**
    - 另含 deferred Slot 模型单测（为阶段 B 单轨化铺路）。

## 阶段 B — P0 行为收敛（仅修闸内问题）

- [x] 4. deferred 单轨化（删除 bool+lobby_id 平行字段）
  - [x] 4.1 将 `HasPending*` 改为只读 `GBE_DotaDeferredTaskSlot.pending`（及 generation/lobby 一致性）
    - 文件：`dll/dll/steam_game_coordinator.h`、`dll/steam_game_coordinator.cpp`。
    - 字段删除：`GBE_pending_dota_abandon_finalize_after_7014`、`GBE_pending_dota_abandon_finalize_lobby_id`、`GBE_pending_dota_normal_signout_finalize_after_25`、`GBE_pending_dota_normal_signout_finalize_lobby_id`、`GBE_pending_reset_after_cache_unsubscribed`、`GBE_pending_reset_after_cache_unsubscribed_lobby_id`。
    - 保留：三个 `*_slot` + `GBE_ConsumeDotaDeferredTask`。
  - [x] 4.2 统一 Set/Clear/Consume 只操作 Slot
    - Set：写 `{lobby_id, current_generation, pending=true}`。
    - Clear：slot 置空。
    - Consume：仅 Slot 路径；去掉同步清 bool 的冗余。
    - 调用方：`gbe_dota_chat_handlers.cpp`、`gbe_dota_lobby_handlers.cpp` 及所有 `SetPending*`/`ClearPending*`/`HasPending*` 引用。
    - 测试镜像：`tools/gbe_dota_handler_test/stubs.h`、`free_func_stubs.cpp`、`smoke_test.cpp` 同步单轨。
  - [x] 4.3 deferred 单测
    - `gbe_dota_dual_gc_host_test`：`test_deferred_slot_model`（Empty/Current/Stale）。
    - handler smoke：abandon finalize / normal signout / reset pending / stale postgame。
  - [x] 4.4 检查点（2026-07-12）
    - `gbe_dota_dual_gc_host_test: all passed`
    - `gbe_dota_handler_test`: 86 passed, 0 failed
    - API 仍保留 `ClearPendingResetAfterCacheUnsubscribed(retained_lobby_id)` 形参以兼容调用方，实现仅清空 Slot（`retained` 忽略）。

- [ ] 5. host hero 读写收口（对齐权威表，消灭第三套 preserve）
  - [ ] 5.1 梳理并标记所有直接写 `owner_hero_id` 的点
    - 至少：`match_handlers`、`inventory_handlers`、`lobby_state_coordinator`、`lobby_state` publish/restore。
    - 验收：每个写点有 reason 字符串或统一 API。
  - [ ] 5.2 抽出单一“应用 owner hero”入口（薄封装即可）
    - 例如 `GBE_ApplyOwnerHeroId(uint32 hero_id, const char *reason, bool publish_shared)` 或 lifecycle action；禁止 handler 内再复制 preserve 条件。
    - 文件优先：`gbe_dota_lobby_state_coordinator.cpp` / `match_handlers` / `inventory_handlers`。
  - [ ] 5.3 使 H1、H5 转绿
    - 只改权威表允许的路径；禁止扩大 showcase/wearable 副作用。
  - [ ]* 5.4 属性：publish 后 shared.owner_hero_id 与权威 writer 一致；restore 不覆盖 known local hero

- [ ] 6. showcase / wearable one-shot 与 7034 路径对齐
  - [ ] 6.1 确认 showcase/wearable 键仅依赖 generation + lobby/steam/hero（已有实现）
    - 删除旁路“再 try 一次”的无 generation 分支（若存在）。
  - [ ] 6.2 统一 7034 owner-hero-known 与 2569 推断后的推装顺序
    - 对齐 `gbe_dota_match_handlers.cpp` 与 `gbe_dota_inventory_handlers.cpp` 注释中的步骤表。
    - 跨 GC 调用仅经 `GBE_PushDotaPlayerEquippedItemsCacheToGC` / `GBE_RefreshDotaHostEquippedItemsCache` / `GBE_PushDotaHeroEquippedItemUpdatesToClientGC`。
  - [ ] 6.3 使 H2、H3、H4 转绿
  - [ ] 6.4 检查点：H1–H5 全绿；`run_gc_verification` 通过
    - 确保所有测试通过，如有疑问请询问用户。
    - **一周内同主题 host fix 必须附带失败先行用例（H* 扩展）。**

## 阶段 C — 分发与 Store 纪律（B 全绿后）

- [ ] 7. 高风险 post-login if 迁入 registry
  - [ ] 7.1 迁移 `7034` → registry Entry + adapter
    - `HandlerId` 新增；`fixture` 指向 dual_gc H1/H2 或 smoke 名。
    - 从 `gbe_dota_post_login_handlers.cpp` 删除对应 if。
  - [ ] 7.2 迁移 `2569` equip → registry
    - fixture 覆盖 host 推断与 forward server。
  - [ ] 7.3 迁移 abandon / signout / destroy lobby → registry
    - 保持 wrapped session 策略与现有行为一致。
  - [ ] 7.4 迁移 launch 标记链（8870 / 4511 / 4508 等）→ registry 或明确子表
    - 若体量大，可先 registry 壳 + 原 handler 函数，禁止残留平行 if。
  - [ ] 7.5 更新 `all_high_risk_entries_have_fixture` 与 audit 基线
    - 文件：`gbe_dota_handler_registry.h`、`tools/_audit_gc_refactor.py`（若有 registry 审计项）。
  - [ ]* 7.6 registry 查找属性：每个 high-risk message_id 在 Direct/Wrapped 下至多一条 Entry

- [ ] 8. Store 写路径收紧
  - [ ] 8.1 全库枚举 `Store::publish` / `update` / `publish_if_generation_*` / `compare_*` 调用点
    - 生产路径跨 role 写必须 generation 门闩。
  - [ ] 8.2 收紧或隔离裸 `update`/`publish`
    - 方案 A：标记 deprecated，仅测试可用；方案 B：改为 private + friend 测试。
    - 迁移所有生产调用到 `publish_if_generation_current_or_newer` 或 `compare_update`/`compare_clear`。
  - [ ] 8.3 增加 audit 规则：禁止新增裸 `shared_lobby_store->update` / `->publish`（白名单测试目录）
  - [ ] 8.4 检查点：Store 单测 + dual_gc + 全量 verification 通过
    - 确保所有测试通过，如有疑问请询问用户。

- [ ] 9. 跨 GC 装备端口收窄（薄重构，不改协议）
  - [ ] 9.1 将 `GBE_Push*` / `GBE_Refresh*` 声明迁出 `gbe_dota_gc_internal.h` 到 `gbe_dota_inventory_ports.h`（或等价）
    - `gc_internal.h` 仅保留日志、const 表、hello cache 等真跨 TU 符号。
  - [ ] 9.2 切断 `steam_networking_sockets.cpp` 对 `gbe_dota_gc_internal.h` 的依赖（若仅需 reconnect 相关，改 include 窄头）
  - [ ]* 9.3 include 自包含编译：新 ports 头单独 TU 可编

## 阶段 D — 结构拆分与验真升级（C 之后）

- [ ] 10. 拆超大 TU（只搬家，不改行为）
  - [ ] 10.1 拆 `gbe_dota_lobby_handlers.cpp` 为 create/join/leave/kick/list 等
    - 保持成员函数签名；更新 premake/源列表。
  - [ ] 10.2 拆 `gbe_dota_lobby_state_coordinator.cpp` 为 publish/restore/recover/stale 策略
  - [ ] 10.3 行为等价：dual_gc + handler smoke + verification 全绿

- [ ] 11. 测试验真升级
  - [ ] 11.1 扩展 `gc_replay_test` 或 dual_gc：input 序列 → 期望 outbound emsg 序列 golden
  - [ ] 11.2 队列 + deferred + generation 交错压测（可扩 `gbe_dota_concurrency_stress_test`）
  - [ ] 11.3 audit：生产装配与 CompositionRoot 所有权图一致，或显式标记 CompositionRoot 为 offline-only 并禁止 docs 声称生产已注入
  - [ ] 11.4 检查点：全量 verification + 新增 golden/压测通过
    - 确保所有测试通过，如有疑问请询问用户。

- [ ] 12. 协议资产与内部头收尾
  - [ ] 12.1 大模板 hex 移出逻辑 TU（`template_replay` / `payload_lobby_helpers`）到数据文件或独立 `*_templates.cpp`
  - [ ] 12.2 `gbe_dota_gc_internal.h` 再瘦身：无 free equip、无 networking 泄漏
  - [ ] 12.3 最终检查点：全量 verification；H1–H5 仍绿；无 deferred 双轨；7034/2569/abandon/signout/destroy 均在 registry
    - 确保所有测试通过，如有疑问请询问用户。

## 明确不在本清单内（禁止顺带做）

- 新功能（非 host/稳定性相关 feat）
- 仅 `docs(gc): close * phase` 而无代码/测试变更
- 在阶段 A 完成前修改 7034/2569/host wearable 生产逻辑
- 在阶段 B 完成前做 CompositionRoot 进生产或大规模文件拆分
- 无失败用例的同主题 host fix

## 阶段退出标准速查

| 阶段 | 退出标准 |
|------|----------|
| A | DualGc harness 可跑；H1–H5 有 assert；权威表/入口清单/deferred 清单齐；**未改 host 生产逻辑** |
| B | deferred 单轨；H1–H5 绿；verification 绿；host 改动均有失败先行用例 |
| C | 7034/2569/abandon/signout/destroy 在 registry；Store 生产路径无裸 publish/update；ports 头就位 |
| D | 大文件已拆；replay/压测升级；gc_internal 瘦身；最终检查点全绿 |
