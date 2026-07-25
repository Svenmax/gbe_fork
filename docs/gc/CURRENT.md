# GC 重构当前真相（唯一入口）

> 任何 Agent 动手前只先读这一份。深入表见文末阅读顺序。
> 本文件描述代码现状，不声明“重构已完成 / 已验收”。

**最后同步：** 2026-07-25（对照 `dll/`、`tools/` 与 `docs/gc/ACTIVE_QUEUE.md`）

## 状态一句话

**收敛出口（2026-07-25）：** 本轮双轨收敛规划任务已完成；R2 Dependencies、R3 capability-header 审计收尾、R4.1–R4.6 局部生命周期单向化、queued launch/runtime、monotonic launch phase、generic capture state/identity/options/custom_game 与聚合 apply、payload 纯 snapshot projection、显式 host capture 同步边界、source-aware shared launch/runtime identity restore、steam-auth 元数据、4511 标记/restore、owner/options/cache/custom_game/generation/generic_lobby_id restore、lifecycle/postgame state apply 单写入切片已完成。generic metadata publish 已实施：7009 host sync、client observe 与 cache/replay/details pure projection 调用面由审计契约保护；8052 lifecycle pre-write 已实施：direct/wrapped 统一经 `LocalLifecyclePreWrite` action 在 runtime queue / fallback publish 前一次写入 Local lifecycle 字段组；postgame chat tombstone 已实施：old-channel 7272 与 7014 retrieval 经显式 tombstone channel/generation helper 消费；Local/shared merge inventory guard 已实施：shared-to-local restore 字段组 entrypoints、owner 与字段组标签由 `LOCAL_LOBBY_USAGE.md` 和剥离注释后的函数定义级 audit 保护，§5 截止于任意下一个二级标题，缺失 §5、重复清单行、block comment 与 line comment 伪定义由回归覆盖；registry inventory guard 已实施：production registry kTable 与 routing inventory §1 的 emsg 集合、HandlerId、modes、lifecycle 与重复 emsg 行由审计保持同步，§1 截止于任意下一个二级标题，且缺失 §1 时 audit 失败；template-only inventory guard 已实施：template replay 的 `TEMPLATE_ONLY` switch case、routing inventory 白名单与重复 emsg 由审计保持同步，`TEMPLATE_ONLY` inventory 行限定在 template_replay §4，§4 截止于任意下一个二级标题，且缺失 §4 时 audit 失败；template registry-defensive routing 已实施：`8879`、`8095`、`8009`、`7091` 经显式 helper 防御转调，§4 routing inventory 集合与重复 emsg 行由审计保护，§4 截止于任意下一个二级标题，且缺失 §4 时 audit 失败；direct conditional fallback routing 已实施：`8744` observe-only 与 `5410`/`5432` late-steam conditional consume 经显式 helper，§2 routing inventory 集合与重复 emsg 行由审计保护，§2 截止于任意下一个二级标题，且缺失 §2 时 audit 失败；wrapped hard miss routing 已实施：wrapped registry miss 经显式 helper 保持 log + return false，HARD_MISS inventory 标记限定在 fallback §2，§2 截止于任意下一个二级标题，且缺失 §2 时 audit 失败；legacy wrapped parser guard 已实施：`GBE_ExtractWrappedDotaDirectContext` 保持 `LEGACY_UNUSED`，该 marker 限定在解析层 §5，§5 截止于任意下一个二级标题，缺失 §5 时 audit 失败，生产 `.cpp` 无调用 contract 由审计保护；inventory section slicing 已收敛为 `inventory_section(...)`，routing 与 Local/shared merge audit 共用同一二级标题边界实现，且 heading lookup 要求 Markdown heading 边界匹配；inventory table cell split 已收敛为 `markdown_cells(...)`，routing 与 Local/shared merge audit 共用同一表格拆分语义；routing inventory duplicate tracking 已收敛为 `record_duplicate(...)`，duplicate issue 输出已收敛为 `append_duplicate_issues(...)`，numeric emsg 排序已收敛为 `sorted_numeric_values(...)`。Phase D **边界+默认不实施**；Phase E **主线关闭**（plan L1.5 + GP-10 设计；L2/L4 链接 spike 失败已书面延期，见 `PHASE_E_EXIT.md`）。日常 D0 修 bug。Store 门控在；local/shared 双轨与上帝类仍为已知债。

## 硬规则（违反即停手）

**routing inventory audit helper（2026-07-25）：** `gc-inventory-emsg-list-helper` 已将 routing inventory emsg 集合 diagnostics 与期望 emsg 迭代统一到 `sorted_numeric_values(...)`，保持比较语义与生产 C++ 行为不变。

**routing inventory diff helper（2026-07-25）：** `gc-inventory-emsg-diff-helper` 已将 routing inventory emsg set mismatch diagnostics 统一到 `append_emsg_set_diff_issue(...)`，保持 numeric sort 与 issue 标签格式一致。

**inventory section heading constants（2026-07-25）：** `gc-inventory-section-heading-constants` 已将 routing 与 Local/shared inventory audit 的 section heading lookup 收敛为命名常量，保持 section slicing 与缺失段落诊断不变。

**inventory case emsg helper（2026-07-25）：** `gc-inventory-case-emsg-helper` 已将 template replay routing audit 的 `case` label emsg 提取收敛为 `extract_case_emsgs(...)`，统一 named Dota constant 到 numeric emsg 的映射。

**inventory request emsg helper（2026-07-25）：** `gc-inventory-request-emsg-helper` 已将 direct conditional fallback audit 的 `request_emsg == ...` 提取收敛为 `extract_request_emsg_comparisons(...)`，统一 named request token 到 numeric emsg 的映射。

**inventory numeric cell helper（2026-07-25）：** `gc-inventory-numeric-cell-helper` 已将 routing inventory 单 emsg Markdown cell 解析收敛为 `numeric_markdown_cell(...)`，明确 exact cell 与 first-number matching 两种语义。

**inventory multi numeric cell helper（2026-07-25）：** `gc-inventory-multi-numeric-cell-helper` 已将 template-only routing inventory 多 emsg Markdown cell 解析收敛为 `numeric_markdown_cell_values(...)`，保持 cell 内 emsg 顺序与 duplicate 检测语义。

**audit text between markers helper（2026-07-25）：** `gc-audit-text-between-markers-helper` 已将 routing audit helper body slicing 收敛为 `text_between_markers(...)`，保持缺失 marker 时空 body 语义。

**audit function call helper（2026-07-25）：** `gc-audit-function-call-helper` 已将 lifecycle 与 reconnect ownership audit 的同构函数调用 token 检测收敛为 `contains_function_call(...)`，保持 word-boundary 与 escaped-symbol 语义。

**audit request body marker slicing（2026-07-25）：** `gc-audit-request-body-marker-slicing` 已将 direct/wrapped post-login request body audit slicing 复用到 `text_between_markers(...)`，保持缺失 marker 时空 body 语义。

**audit word token helper（2026-07-25）：** `gc-audit-word-token-helper` 已将 retired lifecycle/reconnect/shared-lobby audit 的 whole-word token 检测收敛为 `contains_word_token(...)`，保持 escaped-token 与 word-boundary 语义。

**audit marker order helper（2026-07-25）：** `gc-audit-marker-order-helper` 已将 direct/wrapped post-login routing audit 的 marker 顺序比较收敛为 `marker_appears_after(...)`，保持缺失 marker 时不触发顺序诊断的语义。

**audit any token helper（2026-07-25）：** `gc-audit-any-token-helper` 已将 wrapped hard-miss audit 的禁用 route token 检测收敛为 `contains_any_token(...)`，保持 substring membership 语义。

**audit emsg token resolver（2026-07-25）：** `gc-audit-emsg-token-resolver` 已将 registry inventory audit 的 numeric/`GBE_k...` emsg token 解析收敛为 `resolve_emsg_token(...)`，保持 unknown token 诊断不变。

**audit adapter handler call helper（2026-07-25）：** `gc-audit-adapter-handler-call-helper` 已将 post-login registry adapter body 的 request handler call 提取收敛为 `extract_adapter_handler_calls(...)`，保持现有 adapter 诊断不变。

**audit replay label helper（2026-07-25）：** `gc-audit-replay-label-helper` 已将 post-login dispatch audit 的 replay fixture label 提取收敛为 `extract_replay_fixture_labels(...)`，保持三字段行解析与 malformed line ignore 语义。

**SourceTV metadata local apply helper（2026-07-25）：** 4508 game server info 的 `tv_secret_code` / `tv_port` 通过 `apply_source_tv_metadata(...)` 写入 Local，零值输入保留既有字段，post-login handler 保持原 publish guard 与顺序。

**runtime connect local apply helper（2026-07-25）：** 4508 game server info 的 `runtime_connect` 通过 `apply_runtime_connect(...)` 写入 Local，空值和相同值保持 no-op，LAN preserve guard、shared Store compare_update 与日志顺序仍归 post-login handler。

**owner connected local apply helper（2026-07-25）：** launch member connection 的 owner 分支通过 `apply_lobby_owner_connected(...)` 写入 Local，并让 shared restore 复用同一字段变更语义，成员 connected changed 聚合保持不变。

**owner team/slot local apply helper（2026-07-25）：** 7034 draft owner team/slot 通过 `apply_lobby_owner_team(...)` 与 `apply_lobby_owner_slot(...)` 写入 Local，并让 shared restore 复用同一字段变更语义，draft owner slot 零值 guard 仍在 handler。

**chat channel local apply helper（2026-07-25）：** 7009 join 与 7272 leave 的 chat channel 字段组通过 `apply_chat_channel(...)` / `clear_chat_channel(...)` 写入 Local，host sync、7010/7014 push 与 postgame tombstone 顺序保持不变。

1. **新消息**只进 `GBE_ProductionDotaHandlerRegistry()`（`dll/gbe_dota_post_login_dispatcher.cpp`），不得只加 if-chain / template。
2. **生产写 shared lobby** 只走 Store generation 门控 API；禁止裸 `publish` / `update` / `clear`（审计 `audit_store_write_discipline`）。
3. **CompositionRoot** 仅 offline 测试；生产装配在 `dll/steam_client.cpp`，禁止生产构造 CompositionRoot。
4. **owner_hero_id / showcase / wearable** 只允许 `HOST_AUTHORITY.md` 列出的 writer；禁止在 match / inventory / restore 各写一套。
5. 改路由或 host 字段时，**先改对应真相表，再改代码**。

## 当前主风险（最多 5）

1. 路由仍多轨（registry / 条件 fallback / 白名单 template / Hello welcome），但每轨已有归属标签。
2. `GBE_local_lobby` 与 shared Store 双轨；restore/publish 字段级 merge 重（写入口清单见 `LOCAL_LOBBY_USAGE.md`）。
3. `Steam_Game_Coordinator` 仍是上帝类（成员 handler 面过大；Phase D）。
4. lifecycle 门闩是具名 gate token，载荷在 planner/action_list；不回退 `Legacy*` EffectKind。
5. 测试护栏强、真协议 L4/GP-09 L2 **已延期**；`verification passed` ≠ 协议正确。

## 进行中工作

见 [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md)。`gc-inventory-sorted-numeric-helper`、`gc-inventory-duplicate-issues-helper`、`gc-inventory-duplicate-helper`、`gc-inventory-markdown-cells-helper`、`gc-inventory-section-exact-heading`、`gc-inventory-section-heading-anchor`、`gc-inventory-section-helper`、`gc-parser-generic-section-end`、`gc-fallback-generic-section-end`、`gc-template-replay-generic-section-end`、`gc-registry-generic-section-end`、`gc-local-shared-merge-generic-section-end`、`gc-local-shared-merge-section-presence-regression`、`gc-legacy-wrapped-parser-section-presence-guard`、`gc-registry-section-presence-guard`、`gc-wrapped-hard-miss-section-presence-guard`、`gc-direct-conditional-section-presence-guard`、`gc-registry-defensive-section-presence-guard`、`gc-template-only-section-presence-guard`、`gc-template-only-section-guard`、`gc-legacy-wrapped-parser-section-guard`、`gc-wrapped-hard-miss-section-guard`、`gc-direct-conditional-duplicate-inventory-guard`、`gc-registry-defensive-duplicate-inventory-guard`、`gc-template-only-duplicate-inventory-guard`、`gc-registry-duplicate-inventory-guard`、`gc-local-shared-merge-duplicate-inventory-guard`、`gc-local-shared-merge-line-comment-guard`、`gc-local-shared-merge-comment-definition-guard`、`gc-local-shared-merge-definition-guard`、`gc-local-shared-merge-fieldgroup-guard`、`gc-local-shared-merge-inventory-guard`、`gc-generic-metadata-publish`、`gc-8052-lifecycle-pre-write`、`gc-postgame-chat-tombstone`、`gc-registry-inventory-guard`、`gc-registry-metadata-inventory-guard`、`gc-template-only-inventory-guard`、`gc-template-registry-defensive-routing`、`gc-direct-conditional-fallback-routing`、`gc-wrapped-hard-miss-routing` 与 `gc-legacy-wrapped-parser-guard` 已完成。R2、R3、R4.1–R4.6、queued launch/runtime、monotonic launch phase、generic capture state/identity/options/custom_game 与聚合 apply、payload 纯 snapshot projection、显式 host capture 同步边界、source-aware shared launch/runtime identity restore、steam-auth 元数据、4511 标记/restore、owner/options/cache/custom_game/generation/generic_lobby_id restore、lifecycle/postgame state apply 单写入切片已完成。重启 L2/L4 须满足 PHASE_E_EXIT / GP10 §13。

## 必跑验证

```bash
bash tools/run_gc_verification.sh --full
```

改 host / equip / 7034 时额外：

```bash
# dual_gc H1–H5 与 HOST_AUTHORITY 一致
# handler smoke 中与 GOLDEN_PATHS 相关的用例
```

## 深入阅读顺序

1. [MESSAGE_ROUTING_INVENTORY.md](./MESSAGE_ROUTING_INVENTORY.md) — 四轨入口表
2. [HOST_AUTHORITY.md](./HOST_AUTHORITY.md) — hero / wearable / showcase
3. [LOCAL_LOBBY_USAGE.md](./LOCAL_LOBBY_USAGE.md) — local/shared 写入口清单
4. [LEGACY_LIFECYCLE_EFFECTS.md](./LEGACY_LIFECYCLE_EFFECTS.md) — lifecycle 具名门闩盘点
5. [PHASE_D_BOUNDARY.md](./PHASE_D_BOUNDARY.md) — Phase D 边界与停手
6. [PHASE_E_BOUNDARY.md](./PHASE_E_BOUNDARY.md) / [PHASE_E_EXIT.md](./PHASE_E_EXIT.md) — Phase E 边界与出口
7. [GP10_L4_HARNESS.md](./GP10_L4_HARNESS.md) — GP-10 L4 设计 + S0 延期
8. [GOLDEN_PATHS.md](./GOLDEN_PATHS.md) — 行为黄金路径
9. [AGENT_PLAYBOOK.md](./AGENT_PLAYBOOK.md) — 多 Agent 协作
10. [ACTIVE_QUEUE.md](./ACTIVE_QUEUE.md) — 当前任务

## 历史文档（勿作权威入口）

以下保留作设计/勾选档案，**不以其中“已完成”表述覆盖 CURRENT**：

- `follow-up-task-list.md`、`next-agent-task-list.md`、`future-refactor-plan.md`
- `refactor-next-tasklist.md`、`launch-state-*-tasklist.md`
- `.monkeycode/docs/GC_DELIVERY_SUMMARY.md` 等交付长文
- specs 下阶段 tasklist（可作细节，冲突时以本目录真相表 + 代码为准）

## 收敛阶段（代码 KPI，文档不算完成）

| Phase | 目标 | 收敛态 |
|-------|------|--------|
| A | 冻结路由/host 规则 + 黄金路径 | **收口** |
| B | 单路径路由（registry 主轨） | **收口** |
| C | Legacy effect 收敛 / lifecycle decide | **收口** |
| D | 真 DI / 脱离上帝类 | **边界文档；默认不实施** |
| E | L2/L3/L4 测补强 | **主线关闭**（L1.5+设计+延期） |

**本轮收敛完成定义（已满足）：** A–C 代码 KPI 收口 + D/E 边界与停手书面化 + 关键路径至少 L1/L2 护栏（GP-01…07 L2；GP-09 L1.5；GP-10 延期）+ `verification --full` 可重复绿。

**明确不在本轮完成：** Phase D 大实施；GP-09 L2 / GP-10 L4 真协议；消灭 local/shared 双轨。
