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

- [x] 5. host hero 读写收口（对齐权威表，消灭第三套 preserve）
  - [x] 5.1 梳理写点 → 统一到 helpers / `GBE_ApplyOwnerHeroId`
    - 纯函数：`should_preserve_*_on_publish/adopt`、`should_peer_restore_owner_hero_from_client`、`apply_owner_hero_id`、`apply_owner_hero_from_shared`（`gbe_dota_lobby_state.*`）。
    - GC API：`GBE_ApplyOwnerHeroId`；`GBE_SetDotaLobbyMemberRuntimeState` 经此写 owner。
    - restore 增量：`GBE_RestoreSharedDotaLobbyState` 用 `apply_owner_hero_from_shared`。
    - 7034 peer restore 条件改调 `should_peer_restore_owner_hero_from_client`。
    - generic snapshot owner：非 0 时走 `GBE_ApplyOwnerHeroId`。
  - [x] 5.2 单一 apply 入口
    - `GBE_ApplyOwnerHeroId` / `apply_owner_hero_id`（仅非 0）；publish 仍由调用方 lifecycle/SharedLobbyPublish。
  - [x] 5.3 H1、H5 绿；dual_gc 直接调用生产 preserve/peer/apply
  - [x]* 5.4 属性：H5/H5b preserve；H5c apply 单测

- [x] 6. showcase / wearable one-shot 与 7034 路径对齐
  - [x] 6.1 one-shot 键纯函数：`host_showcase_equip_key_matches` / `host_wearable_refresh_key_matches`
    - 生产 `HasPushed*` / `HasRefreshed*` 与 dual_gc / handler stubs 共用；仅 generation+identity，无无 generation 旁路。
  - [x] 6.2 推装顺序文档对齐（match_handlers 注释）
    - 7034：peer restore → OwnerHeroKnownEquipReplay（RefreshHostCache → 可选 wearable one-shot）→ TEAM_SHOWCASE MarkShowcase。
    - 2569：Clear showcase/wearable keys → 同一 `GBE_HandleDotaDirectOwnerHeroKnownEquipReplay`。
    - 跨 GC 仅经 Refresh/Push/PushHeroEquipped 辅助。
  - [x] 6.3 H2、H3、H4 绿（dual_gc 调用生产 key_matches）
  - [x] 6.4 检查点（2026-07-12）：`bash tools/run_gc_verification.sh --fast` **GC verification passed**
    - dual_gc all passed；handler 86/86；audit 74 OK。

## 阶段 C — 分发与 Store 纪律（B 全绿后）

- [x] 7. 高风险 post-login if 迁入 registry
  - [x] 7.1 迁移 `7034` → registry Entry + adapter
    - `HandlerId::Direct7034`；Direct-only；fixture `smoke:test_match_7034_host_showcase_repush_guard_marks_once`。
    - 从 `gbe_dota_post_login_handlers.cpp` 删除对应 if。
  - [x] 7.2 迁移 `2569` equip → registry
    - `HandlerId::EquipItems`；Direct-only；fixture `smoke:test_inventory_equip_full_forward`。
  - [x] 7.3 迁移 abandon / signout / destroy lobby → registry
    - DirectAndWrapped + ForwardWrappedSession；fixture 指向 abandon/signout/destroy smoke。
  - [x] 7.4 迁移 launch 标记链（4506 / 5429 / 8870 / 4511 / 4508）→ registry
    - Direct-only 壳 + 原 handler；4511/4508 为 LobbyLifecycle 并挂 smoke fixture。
  - [x] 7.5 更新 `all_high_risk_entries_have_fixture` 与 audit 基线
    - registry size 37；audit 自动解析 kTable；smoke contract 同步。
  - [x]* 7.6 registry 查找属性：每个 high-risk message_id 在 Direct/Wrapped 下至多一条 Entry
    - 由 `has_unique_message_ids_per_mode` + production registry contract 覆盖。

- [x] 8. Store 写路径收紧
  - [x] 8.1 全库枚举 `Store::publish` / `update` / `publish_if_generation_*` / `compare_*` 调用点
    - 生产 dll：仅 `publish_if_generation_current_or_newer` / `compare_update` / `compare_clear`（coordinator + post_login + steam_game_coordinator）。
    - 裸 `publish`/`update`/`clear` 仅出现在 tools 测试与 Store 自身实现。
  - [x] 8.2 收紧或隔离裸 `update`/`publish`
    - 方案 A：头文件注释标注 test/fixture-only；生产已无调用点可迁移。
  - [x] 8.3 增加 audit 规则：禁止生产 dll 裸 `Store::publish` / `Store::update`
    - `tools/_audit_gc_refactor.py`：`audit_store_write_discipline`（AUDIT 10b）。
  - [x] 8.4 检查点：Store 单测 + dual_gc + 全量 verification 通过
    - 2026-07-13：`bash tools/run_gc_verification.sh --fast` **GC verification passed**；audit Store write discipline issues: 0。

- [x] 9. 跨 GC 装备端口收窄（薄重构，不改协议）
  - [x] 9.1 将 `GBE_Push*` / `GBE_Refresh*` 声明迁出 `gbe_dota_gc_internal.h` 到 `gbe_dota_inventory_ports.h`（或等价）
    - 新头：`dll/gbe_dota_inventory_ports.h`；`gc_internal.h` 去掉三装备 free 声明。
  - [x] 9.2 切断 `steam_networking_sockets.cpp` 对 `gbe_dota_gc_internal.h` 的依赖
    - 改为 `gbe_dota_locator.h` + reconnect_shared + lobby_state_store。
  - [x]* 9.3 include 自包含：ports 头仅 forward + steamtypes + vector；调用方显式 include。

## 阶段 D — 结构拆分与验真升级（C 之后）

- [x] 10. 拆超大 TU（只搬家，不改行为）
  - [x] 10.1 拆 `gbe_dota_lobby_handlers.cpp` 为 create/join/leave/kick/list 等
    - 新 TU：`lobby_handler_helpers` + create/list/join/invite/lifecycle/slot；原文件为空壳。
    - offline：`test_wrapper.cpp` 改为 include 拆分单元；audit baseline/exemptions/teardown 门同步。
  - [x] 10.2 拆 `gbe_dota_lobby_state_coordinator.cpp` 为 publish/restore/recover/stale 策略
    - 新 TU：publish / member / restore / recover；原文件为空壳。
    - generation counter sync 归属 restore；teardown 门归属 member；audit 同步。
  - [x] 10.3 行为等价：dual_gc + handler smoke + verification 全绿
    - 2026-07-13：`bash tools/run_gc_verification.sh --fast` **GC verification passed**（D.10.1+10.2 后）。

- [x] 11. 测试验真升级
  - [x] 11.1 扩展 behavior_replay：input 序列 → 期望 outbound emsg 序列 golden
    - `behavior_replay.cpp`：每步 `outbound_emsg=` + 全局 `sequence_outbound_emsg=`。
    - 序列：create(7038) → launch(7041) → leave(7040) → destroy(8246)。
    - golden：`tools/gbe_dota_handler_test/fixtures/lobby_behavior.expected.txt`（24,7055,26,25）。
  - [x] 11.2 队列 + deferred + generation 交错压测
    - `gbe_dota_concurrency_stress_test`：`test_queue_deferred_generation_interleave`。
    - audit 要求 stress 含 deferred consume / stale / current 边界。
  - [x] 11.3 CompositionRoot offline-only
    - 头文件注释标记 Offline-only；audit 禁止 steam_client 引用 CompositionRoot。
    - ARCHITECTURE.md 必须保留 offline-only + Steam_Client runtime authority；禁止 docs 声称生产注入。
  - [x] 11.4 检查点：全量 verification + 新增 golden/压测通过
    - 2026-07-13：`bash tools/run_gc_verification.sh --fast` **GC verification passed**。

- [x] 12. 协议资产与内部头收尾
  - [x] 12.1 大模板 hex 移出逻辑 TU 到独立 `gbe_dota_template_replay_templates.{h,cpp}`
    - handlers 仅 include 模板头；数组显式 size 保证跨 TU `sizeof` 正确。
    - audit：TEMPLATE_BLOB_OWNER + handlers 禁止持有 static blobs。
  - [x] 12.2 `gbe_dota_gc_internal.h` 再瘦身
    - 去掉 payload_lobby / locator / reconnect re-export；equip 仍仅在 `inventory_ports.h`。
    - 调用方显式 include；audit 5b `audit_gc_internal_slim_boundary`。
  - [x] 12.3 最终检查点（2026-07-13）
    - `bash tools/run_gc_verification.sh --fast` **GC verification passed**
    - dual_gc H1–H5 绿；handler 88/88；behavior_replay golden `24,7055,26,25`
    - registry 37 条；Store 生产无裸 publish/update；deferred 单轨

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
