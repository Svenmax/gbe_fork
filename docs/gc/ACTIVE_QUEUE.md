# 活跃队列（WIP ≤5）

> 只保留进行中与本周明确不做。历史勾选见 archive 长文，不在此膨胀。
> **最后更新：** 2026-07-25

## WIP

| ID | 目标 | 触碰文件（预期） | 完成定义 | 验证 | 停手条件 |
|----|------|------------------|----------|------|------|
| — | — | — | — | — | — |

## 本周不做

- 真 DI / 拆 `Steam_Game_Coordinator` 上帝类（Phase D 大实施）
- CompositionRoot 进生产
- 为美观再横向大拆文件
- 重启 GP-10 L4 / GP-09 L2（无新链接策略）
- 新增长篇 delivery / next-agent 清单

## 最近完成（最多 5，旧项归档至 `REFACTOR_EXECUTION_PLAN.md`）

| 日期 | 摘要 | 链接 |
|------|------|------|
| 2026-07-26 | join merge apply helper：7044 join plan 的 Local 快照写回经 `apply_join_lobby_merge_plan(...)`，generation advance/write、generic lobby join/settings sync、local member data、publish 与 response 顺序保持不变 | `.monkeycode/specs/gc-local-join-merge-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | create state apply helper：7038 create plan 的 Local 快照写回经 `apply_create_lobby_state_plan(...)`，generation 写入、normalize、reconnect context 与后续 create actions 顺序保持不变 | `.monkeycode/specs/gc-local-create-state-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | kick member snapshot apply helper：7081 generic kick 成功后的 Local 快照写回经 `apply_lobby_member_kick_snapshot(...)`，kick 调用、shared publish、details update 与日志顺序保持不变 | `.monkeycode/specs/gc-local-kick-member-snapshot-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | local lobby clear helper：runtime clear、postgame 7272 stale shared cleanup 与 recover generation exhausted 的 Local 清空经 `clear_local_lobby(...)`，shared clear、generic leave 与后续副作用顺序保持不变 | `.monkeycode/specs/gc-local-clear-helper/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | generic lobby observation flags helper：kick/adoption 观察标记经 state helper 写入 Local，等待确认、kick suppression 与 owner adoption suppression 的一次性日志语义保持不变 | `.monkeycode/specs/gc-local-generic-observation-flags/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | owner name local apply helper：generic lobby owner adoption 的 Local owner_name 写入经 `apply_lobby_owner_name(...)`，owner adoption、publish 与日志顺序保持不变 | `.monkeycode/specs/gc-local-owner-name-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | members restore helper：shared-to-local runtime restore 的 members 字段组经 `restore_lobby_members(...)` 写入 Local，changed 聚合与后续 sync/rich presence/login sync 顺序保持不变 | `.monkeycode/specs/gc-local-members-restore/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | details update local apply helper：7046 set details 的 options/custom_game 字段组经 `apply_lobby_details_update(...)` 写入 Local，保留 normalize、details push 与日志顺序 | `.monkeycode/specs/gc-local-details-update-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | generation local apply helper：create/join/runtime reset/lifecycle clear/recover 的 Local generation 写入经 `apply_lobby_generation(...)`，restore helper 复用同一 apply 语义 | `.monkeycode/specs/gc-local-generation-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | server id local apply helper：recover coordinator 的 derived server_id 写入经 `apply_lobby_server_id(...)`，runtime metadata helper 复用同一 server id apply 语义 | `.monkeycode/specs/gc-local-server-id-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | generic lobby id local apply helper：7038 create、7040 leave fallback 与 leave-generic clear 的 generic_lobby_id 写入经 `apply_lobby_generic_lobby_id(...)`，restore helper 复用同一 apply 语义 | `.monkeycode/specs/gc-local-generic-lobby-id-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-26 | runtime metadata local apply helper：generic metadata publish 的 connect/server_id Local 写入经 `apply_runtime_metadata(...)`，保留 shared Store compare_update、generic metadata publish 与日志顺序 | `.monkeycode/specs/gc-local-runtime-metadata-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | bot difficulty team local apply helper：7047 set team slot 的 radiant/dire bot difficulty 写入经 `apply_lobby_bot_difficulty_for_team(...)` 选择字段，保留 bot team 推导、member update、normalize、publish/details/ack 顺序 | `.monkeycode/specs/gc-local-bot-difficulty-team-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | custom game loading metadata local apply helper：8052 started loading 的 custom_game_id / start_time 经 `apply_custom_game_loading_metadata(...)` 写入 Local，保留非零 guard、launch setup 计算与 lifecycle decision 顺序 | `.monkeycode/specs/gc-local-custom-game-loading-metadata-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | owner team/slot slot-handler local apply helper：7047 set team slot 的 owner team/slot 写入复用 `apply_lobby_owner_team(...)` 与 `apply_lobby_owner_slot(...)`，保留 member update、bot difficulty、normalize 与 publish 顺序 | `.monkeycode/specs/gc-local-owner-team-slot-handler-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | owner connection lifecycle local apply helper：`on_client_connected(...)` / `on_client_disconnected(...)` 的 owner connected 写入复用 `apply_lobby_owner_connected(...)`，保留 suppress guard 与 postgame publish suppression 顺序 | `.monkeycode/specs/gc-local-owner-connection-lifecycle-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | cache subscription metadata local apply helper：`GBE_RecordDotaLobbyCacheSubscriptionState(...)` 解析后的 cache metadata 字段组通过 `apply_cache_subscription_metadata(...)` 写入 Local，日志与 publish 顺序保持不变 | `.monkeycode/specs/gc-local-cache-subscription-metadata-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | broadcast channel local apply helper：7149 join、7367 update 与 8054 close 通过 `apply_broadcast_channel(...)` / `patch_broadcast_channel(...)` / `clear_broadcast_channel(...)` 写入 Local，保留 publish/details/ack 顺序 | `.monkeycode/specs/gc-local-broadcast-channel-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | chat channel local apply helper：7009 join 与 7272 leave 通过 `apply_chat_channel(...)` / `clear_chat_channel(...)` 写入 Local，保留 host sync、7010/7014 push 与 postgame tombstone 顺序 | `.monkeycode/specs/gc-local-chat-channel-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | owner team/slot local apply helper：7034 draft owner team/slot 通过 `apply_lobby_owner_team(...)` 与 `apply_lobby_owner_slot(...)` 写入 Local，shared restore 复用同一字段变更语义 | `.monkeycode/specs/gc-local-shared-owner-team-slot-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | owner connected local apply helper：launch member connection owner 分支通过 `apply_lobby_owner_connected(...)` 写入 Local，shared restore 复用同一字段变更语义 | `.monkeycode/specs/gc-local-shared-owner-connected-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | runtime connect local apply helper：4508 game server info 的 `runtime_connect` 通过 `apply_runtime_connect(...)` 写入 Local，保持空值 no-op、LAN preserve guard 与 shared Store compare_update 顺序 | `.monkeycode/specs/gc-local-shared-runtime-connect-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | SourceTV metadata local apply helper：4508 game server info 的 `tv_secret_code` / `tv_port` 通过 `apply_source_tv_metadata(...)` 写入 Local，保持零值保留与 publish 顺序 | `.monkeycode/specs/gc-local-shared-sourcetv-metadata-apply/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | audit replay label helper：post-login dispatch audit 的 replay fixture label 提取共用 `extract_replay_fixture_labels(...)` | `.monkeycode/specs/gc-audit-replay-label-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | audit adapter handler call helper：post-login registry adapter body 的 request handler call 提取共用 `extract_adapter_handler_calls(...)` | `.monkeycode/specs/gc-audit-adapter-handler-call-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | audit emsg token resolver：registry inventory audit 的 numeric/`GBE_k...` emsg token 解析共用 `resolve_emsg_token(...)` | `.monkeycode/specs/gc-audit-emsg-token-resolver/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | audit any token helper：wrapped hard-miss audit 的禁用 route token 检测共用 `contains_any_token(...)` | `.monkeycode/specs/gc-audit-any-token-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | audit marker order helper：direct/wrapped post-login routing audit 的 marker 顺序比较共用 `marker_appears_after(...)` | `.monkeycode/specs/gc-audit-marker-order-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | audit word token helper：retired lifecycle/reconnect/shared-lobby audit 的 whole-word token 检测共用 `contains_word_token(...)` | `.monkeycode/specs/gc-audit-word-token-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | audit request body marker slicing：direct/wrapped post-login request body audit slicing 复用 `text_between_markers(...)`，保持缺失 marker 空 body 语义 | `.monkeycode/specs/gc-audit-request-body-marker-slicing/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | audit function call helper：lifecycle 与 reconnect ownership audit 的同构函数调用 token 检测共用 `contains_function_call(...)` | `.monkeycode/specs/gc-audit-function-call-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | audit text between markers helper：routing audit helper body slicing 共用 marker-based text helper，保持缺失 marker 时空 body 语义 | `.monkeycode/specs/gc-audit-text-between-markers-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory multi numeric cell helper：template-only routing inventory 多 emsg Markdown cell 解析共用 helper，保持 cell 内 emsg 顺序与 duplicate 检测语义 | `.monkeycode/specs/gc-inventory-multi-numeric-cell-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory numeric cell helper：routing inventory 单 emsg Markdown cell 解析共用 helper，区分 exact cell 与 first-number matching | `.monkeycode/specs/gc-inventory-numeric-cell-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory request emsg helper：direct conditional fallback audit 共用 `request_emsg == ...` 提取 helper，统一 named request token 到 numeric emsg 的映射 | `.monkeycode/specs/gc-inventory-request-emsg-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory case emsg helper：template replay routing audit 共用 `case` label emsg 提取 helper，统一 named Dota constant 到 numeric emsg 的映射 | `.monkeycode/specs/gc-inventory-case-emsg-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory section heading constants：routing 与 Local/shared inventory audit 的 section heading lookup 共用命名常量，避免 heading literal 漂移 | `.monkeycode/specs/gc-inventory-section-heading-constants/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory emsg diff helper：routing inventory emsg set mismatch diagnostics 共用输出 helper，保持 numeric sort 与标签格式一致 | `.monkeycode/specs/gc-inventory-emsg-diff-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory emsg list helper：routing inventory emsg 集合 diagnostics 与迭代统一使用 numeric sort helper，避免集合输出顺序漂移 | `.monkeycode/specs/gc-inventory-emsg-list-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory sorted numeric helper：routing inventory audit 共用 numeric emsg sort helper，避免排序语义漂移 | `.monkeycode/specs/gc-inventory-sorted-numeric-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory duplicate issues helper：routing inventory audit 共用 duplicate issue 输出 helper，保持 emsg numeric sort 与消息前缀一致 | `.monkeycode/specs/gc-inventory-duplicate-issues-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory duplicate helper：routing inventory audit 共用 duplicate tracking helper，避免 seen/duplicates 记录语义漂移 | `.monkeycode/specs/gc-inventory-duplicate-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory markdown cells helper：routing 与 Local/shared merge inventory audit 共用 Markdown table cell split helper，避免后续解析语义漂移 | `.monkeycode/specs/gc-inventory-markdown-cells-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory section exact heading：shared inventory section helper 要求 heading 边界匹配，避免同前缀更长 heading 误定位 | `.monkeycode/specs/gc-inventory-section-exact-heading/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory section heading anchor：shared inventory section helper 只接受行首 heading，避免正文提及 heading token 时误定位 | `.monkeycode/specs/gc-inventory-section-heading-anchor/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | inventory section helper：routing 与 Local/shared merge inventory audit 共用二级标题 section slicing helper，避免 offset 逻辑漂移 | `.monkeycode/specs/gc-inventory-section-helper/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | parser generic section end：解析层 §5 截止于任意下一个二级标题，避免缺失分隔线时扩大 legacy parser 扫描 | `.monkeycode/specs/gc-parser-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | fallback generic section end：fallback §2 截止于任意下一个二级标题，避免缺失分隔线时扩大 direct/wrapped 扫描 | `.monkeycode/specs/gc-fallback-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | template_replay generic section end：template_replay §4 截止于任意下一个二级标题，避免缺失分隔线时扩大扫描 | `.monkeycode/specs/gc-template-replay-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry generic section end：Registry inventory §1 截止于任意下一个二级标题，避免缺失分隔线时扩大扫描 | `.monkeycode/specs/gc-registry-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge generic section end：Local/shared merge inventory §5 截止于任意下一个二级标题，避免后续章节编号变化扩大扫描 | `.monkeycode/specs/gc-local-shared-merge-generic-section-end/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge section presence regression：补齐 Local/shared merge inventory §5 缺失 fail-fast 的回归测试 | `.monkeycode/specs/gc-local-shared-merge-section-presence-regression/`、`tools/test_audit_gc_refactor.py` |
| 2026-07-25 | legacy wrapped parser section presence guard：缺失解析层 §5 时 legacy wrapped parser audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-legacy-wrapped-parser-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry section presence guard：缺失 registry §1 时 registry inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-registry-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | wrapped hard miss section presence guard：缺失 fallback §2 时 wrapped hard miss inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-wrapped-hard-miss-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | direct conditional section presence guard：缺失 fallback §2 时 direct conditional inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-direct-conditional-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry-defensive section presence guard：缺失 template_replay §4 时 registry-defensive inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-registry-defensive-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | template-only section presence guard：缺失 template_replay §4 时 template-only inventory audit 直接失败，避免回退整篇扫描 | `.monkeycode/specs/gc-template-only-section-presence-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | template-only section guard：`TEMPLATE_ONLY` inventory 行只在 template_replay §4 中有效，避免其它段落行误参与 audit | `.monkeycode/specs/gc-template-only-section-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | legacy wrapped parser section guard：`GBE_ExtractWrappedDotaDirectContext` 的 `LEGACY_UNUSED` 标记只在解析层 §5 中有效，避免其它段落文字误满足 audit | `.monkeycode/specs/gc-legacy-wrapped-parser-section-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | wrapped hard miss section guard：wrapped HARD_MISS inventory 标记只在 fallback §2 中有效，避免其它段落文字误满足 audit | `.monkeycode/specs/gc-wrapped-hard-miss-section-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | direct conditional duplicate inventory guard：direct conditional fallback inventory 重复 emsg 行由 audit 检测，且解析限定在 fallback §2 | `.monkeycode/specs/gc-direct-conditional-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry-defensive duplicate inventory guard：registry-defensive inventory 重复 emsg 行由 audit 检测，且解析限定在 template_replay §4 | `.monkeycode/specs/gc-registry-defensive-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | template-only duplicate inventory guard：template-only inventory 重复 emsg 由 audit 检测，避免 set 比较隐藏重复表格项 | `.monkeycode/specs/gc-template-only-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | registry duplicate inventory guard：registry inventory 重复 emsg 行由 audit 检测，避免 set/dict 比较隐藏重复表格行 | `.monkeycode/specs/gc-registry-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge duplicate inventory guard：Local/shared merge inventory 重复三元组由 audit 检测，避免 set 比较隐藏重复表格行 | `.monkeycode/specs/gc-local-shared-merge-duplicate-inventory-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge line-comment guard：补齐 definition audit 的 line comment 伪定义回归测试，锁住 `//` 注释剥离语义 | `.monkeycode/specs/gc-local-shared-merge-line-comment-guard/`、`tools/test_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge comment definition guard：Local/shared merge definition audit 匹配前剥离 C++ 注释，避免 block comment 伪定义误通过 | `.monkeycode/specs/gc-local-shared-merge-comment-definition-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge definition guard：Local/shared merge entrypoint presence 校验收紧为函数定义匹配，避免调用点或文本引用误判 | `.monkeycode/specs/gc-local-shared-merge-definition-guard/`、`tools/_audit_gc_refactor.py` |
| 2026-07-25 | Local/shared merge field-group guard：Local/shared merge inventory 的 entrypoint、owner、字段组三元组由 audit 保持同步 | `.monkeycode/specs/gc-local-shared-merge-fieldgroup-guard/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | Local/shared merge inventory guard：shared-to-local restore 字段组 entrypoints 由 `LOCAL_LOBBY_USAGE.md` 与 audit 保持同步 | `.monkeycode/specs/gc-local-shared-merge-inventory-guard/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | registry metadata inventory guard：production registry kTable 与 routing inventory §1 的 emsg 集合、HandlerId、modes、lifecycle 由 audit 保持同步 | `.monkeycode/specs/gc-registry-metadata-inventory-guard/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | registry inventory guard：production registry kTable 与 routing inventory §1 的 emsg 集合由 audit 保持同步 | `.monkeycode/specs/gc-registry-inventory-guard/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | template-only inventory guard：template replay 的 `TEMPLATE_ONLY` switch case 与 routing inventory 白名单由 audit 保持同步 | `.monkeycode/specs/gc-template-only-inventory-guard/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | legacy wrapped parser guard：`GBE_ExtractWrappedDotaDirectContext` 保持 `LEGACY_UNUSED`，生产 `.cpp` 无调用 contract 由审计保护 | `.monkeycode/specs/gc-legacy-wrapped-parser-guard/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | wrapped hard miss routing：wrapped post-login registry miss 收敛到显式 hard-miss helper，保持 log + return false，并由 audit 保护 template replay / SetTeamSlot dead fallback 缺席 | `.monkeycode/specs/gc-wrapped-hard-miss-routing/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | direct conditional fallback routing：`8744` observe-only 与 `5410`/`5432` late-steam conditional consume 收敛到显式 helper，并由 audit 保护实现与路由真相表一致性 | `.monkeycode/specs/gc-direct-conditional-fallback-routing/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | template registry-defensive routing：`8879`、`8095`、`8009`、`7091` 从 template-only switch 收敛到显式 defensive helper，并由 audit 保护路由真相表一致性 | `.monkeycode/specs/gc-template-registry-defensive-routing/`、`MESSAGE_ROUTING_INVENTORY.md` |
| 2026-07-25 | postgame chat tombstone：旧 postgame chat channel 显式记录 tombstone channel/generation，并以 helper 统一 old-channel 7272 与 7014 retrieval 的 generation-scoped 消费 | `.monkeycode/specs/gc-postgame-chat-tombstone/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | 8052 lifecycle pre-write：direct/wrapped 统一经 `LocalLifecyclePreWrite` action 在 runtime queue / fallback publish 前一次写入 Local lifecycle 字段组 | `.monkeycode/specs/gc-8052-lifecycle-pre-write/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | generic metadata publish：盘点全部 capture 调用点，冻结 7009 host sync、client observe、cache/replay/details pure projection 契约并增加审计回归 | `.monkeycode/specs/gc-generic-metadata-publish/`、`LOCAL_LOBBY_USAGE.md` |
| 2026-07-25 | D2 规划收尾：generic metadata publish、8052 lifecycle pre-write 与 postgame chat tombstone 已拆为独立待实施规格 | `.monkeycode/specs/gc-generic-metadata-publish/`、`.monkeycode/specs/gc-8052-lifecycle-pre-write/`、`.monkeycode/specs/gc-postgame-chat-tombstone/` |
| 2026-07-25 | D2 切片：same-generation Local generic capture 与 shared snapshot 竞争时，以 source-aware restore plan 保留 Local launch/runtime identity 字段组 | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：7009 host capture 经显式同步边界发布 shared Store，client 保持 observe-only | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：cache、replay 与 details payload 统一 capture facade | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：payload facade 收敛为纯 snapshot projection | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：generic metadata capture 收敛为聚合 plan/apply | `gc-launch-runtime-single-write/tasklist.md` |
| 2026-07-25 | D2 切片：generic capture 的 custom_game 字段组收敛为 plan/apply | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-25 | D2 切片：generic capture 的 options/bot 字段组收敛为 plan/apply | `REFACTOR_EXECUTION_PLAN.md` |
| 2026-07-25 | D2 切片：generic capture 的 room/match/server/connect/start_time 收敛为 plan/apply | `REFACTOR_EXECUTION_PLAN.md` |

## 队列规则

1. 认领：把一行标 `in_progress`（或备注 Agent 名），同时 WIP 不超过 5。
2. 完成：写完成定义勾选结果 → 移入“最近完成” → 更新 CURRENT 风险若变化。
3. 冲突：HOST 字段相关任务与路由大迁移不要并行改同一热路径。
