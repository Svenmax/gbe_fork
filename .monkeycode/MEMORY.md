# 用户指令记忆

本文件记录了用户的指令、偏好和教导，用于在未来的交互中提供参考。

## 格式

### 用户指令条目
用户指令条目应遵循以下格式：

[用户指令摘要]
- Date: [YYYY-MM-DD]
- Context: [提及的场景或时间]
- Instructions:
  - [用户教导或指示的内容，逐行描述]

### 项目知识条目
Agent 在任务执行过程中发现的条目应遵循以下格式：

[项目知识摘要]
- Date: [YYYY-MM-DD]
- Context: Agent 在执行 [具体任务描述] 时发现
- Category: [代码结构|代码模式|代码生成|构建方法|测试方法|依赖关系|环境配置]
- Instructions:
  - [具体的知识点，逐行描述]

## 去重策略
- 添加新条目前，检查是否存在相似或相同的指令
- 若发现重复，跳过新条目或与已有条目合并
- 合并时，更新上下文或日期信息
- 这有助于避免冗余条目，保持记忆文件整洁

## 条目

[Dota2 4506 后应优先复用当前 runtime 26，而不是硬编码大号 official 018]
- Date: 2026-05-02
- Context: Agent 在继续顺序对照 `officialconsole.log:833-875`、`console.log:820-839` 与 `gbe_gc_debug.log:479-507` 时发现
- Category: 代码模式
- Instructions:
  - 当前样本里，官方在 `4506 / k_EMsgGCServerAvailable` 之后首先出现的是两条约 `511-byte` 的 `26`，其中第二条才把 `CSODOTALobby.state` 从 `SERVERSETUP` 推到 `RUN`。
  - 若这里继续回放硬编码的 `official packet 018 after 4506`，当前实现会产出一条约 `916-byte` 的 `26`，并顺带改动 `2015 extra_startup_messages` 与 `2016 lobby_event_points`，这比官方同窗口更早、更重地污染 dashboard 消费到的 lobby/SO 状态。
  - 这个阶段更安全的最小对齐方式是复用当前 runtime `26` builder：先补一条保持 `state=1/game_state=0` 的 prelude，再补一条 `state=2/game_state=0` 的 `RUN` 更新，避免在 `4506` 边缘额外注入大 `2015` startup payload。

[Dota2 保留 4511 后 donor 2015 仍不足以单独修复 dashboard]
- Date: 2026-05-02
- Context: 用户在合入 `fix: preserve launch cache donor startup payload` 后再次复测，反馈问题依旧并重新上传日志
- Category: 代码模式
- Instructions:
  - 仅把 `4511` 后 official-template `24` 的 `2015` 改为保留 donor startup payload，并不能单独让主界面从 loading/host loading 切到官方的 return to game / leave game。
  - 后续分析仍需继续顺序核对 `4511 -> 24 -> 021 -> 024/025 -> 030 -> 032 -> 043/046` 整条链，而不是把“首个大 8869”视为已确认唯一根因。

[Dota2 4511 后的 official-template 24 不应重写 2015 为大 8869]
- Date: 2026-05-02
- Context: Agent 在继续顺序对照 `officialconsole.log`、`console.log` 与 `gbe_gc_debug.log` 的首个 launch `24` 窗口时发现
- Category: 代码模式
- Instructions:
  - 当前样本里，`4511` 后 server_id 同步阶段发送的 `official-template CacheSubscribed` 是客户端第一次稳定看到 `CSODOTAServerLobby.extra_startup_messages[0]` 的地方。
  - 官方该窗口中的 `8869` 是短 payload，而当前实现因为对这条 `24` 开启了 `rewrite_2015`，把 donor 的 `2015` 改写成了带完整账号数据的大 payload，导致首个可见缓存状态从一开始就偏离官方。
  - 这个阶段更安全的最小对齐方式是保留 donor 的 `2015`，只继续重写 `2004/2014/2016` 等运行态字段，不要在 `4511` 后的 official-template `24` 上追加本地大 `8869`。

[Dota2 hero-selection 仅补第三条 current 26 仍不足以修复 dashboard]
- Date: 2026-05-02
- Context: 用户在合入 `fix: queue hero-selection lobby update before personas` 后再次复测，反馈主界面状态仍然没有变化，并重新上传日志
- Category: 代码模式
- Instructions:
  - 在当前样本里，单独把一条额外的 `current direct 26 details update` 提前到 hero-selection persona 之前，并不能单独驱动 dashboard 从 loading 切到“返回游戏 / 离开游戏”。
  - 后续分析应继续顺序核对 hero-selection 窗口前后的 `24/26/766` 组合与 cache 元数据，而不是把“缺第三条 26”当成已确认根因。

[Dota2 hero-selection 主界面若仍卡载入中，可先补当前 direct 26 再发 persona]
- Date: 2026-05-02
- Context: Agent 在顺序对照 `officialconsole.log` 第 `1112-1118` 行、`console.log` 第 `1122-1129` 行与 `gbe_gc_debug.log` 第 `796-863` 行时发现
- Category: 代码模式
- Instructions:
  - 当前样本里，官方在 dashboard 切回前的 hero-selection 窗口可见连续 3 条 `26`，而 `gbe` 原先只有 `043/046` 两条 `26`，中间夹着成对 `766` persona，导致 dashboard 可能仍停在 host loading。
  - 若 profile 已正常切到 `PRIVATE_LOBBY`，但主界面仍未切按钮，优先尝试在第一次进入 `state=2 && game_state=4` 时补发一次当前 runtime `26` details update，并让它排在额外 persona 之前，而不是先提前整组 `24/26` snapshot。
  - 这个 hero-selection `26` 只应补发一次，避免与更晚的 `8673` 运行时恢复快照形成无限重复或过度刷屏。

[Dota2 若个人页已切私人房间但主界面仍载入中，优先怀疑 dashboard 专属链路]
- Date: 2026-05-02
- Context: 用户在复测“进入英雄选择后个人信息界面正常变为私人房间，但主界面仍显示载入中”后反馈，并上传新日志要求继续排查
- Category: 代码模式
- Instructions:
  - 当 profile/persona 视图已经在英雄选择时正确显示 `PRIVATE_LOBBY`，说明本地 rich presence 与至少一条 persona 链已经足够驱动个人信息界面刷新。
  - 这时剩余问题应优先怀疑 dashboard/主界面专属的下游链路，例如 cache subscribed prelude、`24/26` snapshot、或其他只被大厅面板消费的 GC 时序，而不应再优先修改纯 persona/rich-presence 切换条件。

[Dota2 英雄选择首次 RUN 边缘要提前补齐成对的 private-lobby 766]
- Date: 2026-05-02
- Context: Agent 在继续排查“GC 日志已切到 PRIVATE_LOBBY，但用户实测 UI 仍没变化”并顺序对照 `console.log`、`gbe_gc_debug.log` 与 `steam_game_coordinator.cpp` 的 persona 排队时发现
- Category: 代码模式
- Instructions:
  - 当前样本里，`state=2` 且 `game_state` 第一次进入 `>=2` 时，`gbe` 只会先补一条 `766 size=520` 的本地 `private-lobby` persona；而官方在这个窗口更像是成对补发两条 `766`，分别覆盖带 `server_id` 的 persona 与本地 persona。
  - 如果只提前补本地 `private-lobby` persona，而把带 `server_id` 的 `private-lobby` persona 继续拖到更晚 `046` 之后，主界面按钮和个人页状态可能不会在英雄选择时一起及时刷新。
  - 因此英雄选择首次 `RUN` 边缘的最小对齐方式是：在现有本地 `private-lobby` persona 之外，再提前补一条 `...ServerPrivateLobbyHex`，同时不要阻断 `046` 后官方样式的重复 persona 链。

[Dota2 单人本地练习房在英雄选择前后就应切到 PRIVATE_LOBBY，而不必等 pregame persona 全链]
- Date: 2026-05-02
- Context: Agent 在顺序对照 `officialconsole.log`、`console.log` 与 `gbe_gc_debug.log` 的“建房 -> 开始游戏 -> 英雄选择”窗口时发现
- Category: 代码模式
- Instructions:
  - 当前样本里，`gbe` 在 `4506` 之后虽然已经进入 `CSODOTALobby.state=RUN`，但本地 rich presence 仍保持 `#DOTA_RP_FINDING_MATCH`，并一路持续到 `7034` 把 `game_state` 推进到 `1/2`；这正对应用户看到的“主机载入中 / 寻找比赛中”滞后。
  - 这类面板切换不应再强依赖 `GBE_kDotaLaunchPeripheralStagePregameRunPersona` 或 `game_state==4`；只要 run persona 时序已建立，并且 lobby 已推进到 `state=2` 且 `game_state>=2` 的英雄选择边缘，就应立即切到 `#DOTA_RP_PRIVATE_LOBBY + RUN`。
  - 即使 private-lobby persona 已提前补发，也不应阻断后续晚期 Steam 链的补消息；像 `5410/5432` 驱动的后续补发仍应允许继续完成。

[Dota2 2016 的 SO 摘要必须使用 server-static formatter]
- Date: 2026-05-01
- Context: Agent 在继续排查 Dota2 练习房英雄选择临时错位，并顺序核对 `gbe_gc_debug.log` 与 `dll/steam_game_coordinator.cpp` 的 SO summary 输出时发现
- Category: 代码模式
- Instructions:
  - `2016` 对应 `CSODOTAServerStaticLobby`，其 `member[0]` 摘要必须走 `GBE_FormatDotaServerStaticLobbyMemberSummary(...)`，不能复用 `GBE_FormatDotaLobbyMemberStateSummary(...)`。
  - 如果 `2016` 调试日志继续按 lobby member schema 打印，就会出现伪 `team=0 slot=0 leaver_status=...`，这类输出只能说明 formatter 用错，不能当成真实运行态证据。

[Dota2 practice lobby 的 2016 实际是 CSODOTAServerStaticLobby，不能按 team/slot member 重写]
- Date: 2026-05-01
- Context: Agent 在继续排查“hero selection 暂时把 Dire 3 显示成 Dire 1”并重新核对 `dota_gcmessages_common_lobby.proto`、`console.log` 与 `steam_game_coordinator.cpp` 的 `2016` 重写逻辑时发现
- Category: 代码模式
- Instructions:
  - `2016` 对应的是 `CSODOTAServerStaticLobby`，其 `all_members[0]` 原始字段布局与 `CSODOTAServerStaticLobbyMember` 一致：常见字段是 `steam_id/rank_tier/coach_rating/favorite_team_packed/disabled_random_hero_bits/banned_hero_ids`，并不包含 `team/slot/hero_id/leaver_status` 这套 `CSODOTALobbyMember` 语义。
  - 因此 donor/runtime `2016` 不能继续复用 `GBE_RewriteDotaLobbyTemplateMemberObject(...)` 去补 `field 3/7/2/16/28`；那会把错误 schema 的字段塞进 server-static 对象，污染英雄选择阶段读取到的 server static lobby 数据。
  - `2016` 的最小安全处理应只修正它真实存在的身份字段，例如 `all_members[].steam_id`，而本地 scratch 构造的 `2016` 也应至少保持为 server-static 的最小合法形态，而不是伪造出 lobby member 状态对象。

[Dota2 单人本地练习房的英雄选择阶段会固定显示在 1 号位]
- Date: 2026-05-02
- Context: 用户在官方客户端复测单人、本地、练习房后补充说明，并要求回收此前围绕英雄选择临时 1 号位的推断性修复
- Category: 代码模式
- Instructions:
  - 当前已确认的现象仅限于“单人、本地、练习房”场景：进入英雄选择界面时，本地玩家会显示在 1 号位，这与官方行为一致，不应再作为 bug 继续修复。
  - 基于该现象做出的推断性改动，例如规范化 `2015.all_members`、清理 donor `2004.field124/132`、在缺失时强行补 `2004.owner_state.account_id`，后续默认应视为无效假设并优先回收。
  - 多人练习房是否也会表现为相同行为目前尚未测试；后续若继续分析座位显示问题，必须明确区分“单人本地练习房已知官方行为”和“多人场景未验证行为”。

[Dota2 英雄选择前后的 donor 2004 并不会在切阶段时改 owner slot]
- Date: 2026-05-01
- Context: Agent 在继续排查“hero selection 把 Dire 3 暂时显示成 Dire 1”并顺序复核 `gbe_gc_debug.log` 第 520-719 行与 `console.log` 第 1207-1218 行后发现
- Category: 代码模式
- Instructions:
  - 当前样本里 `official packet 021/024/025/030/032 after 7034` 的 donor `26` 在 `2004` 上都保持同一模式：`owner_state.team/slot` 持续为目标 owner 的真实值，`field121` 始终是单个 `0`，而 `field124` 在这些 donor `26` 里缺失。
  - 从 `WAIT_FOR_PLAYERS_TO_LOAD` 进入 `HERO_SELECTION` 时，客户端日志可见唯一明确 SO 变化是 `CSODOTALobby.game_state` 切换；没有观察到同窗口里新的 slot 字段更新。
  - 因此若英雄选择界面出现临时错位，应优先怀疑“进入该阶段后 UI 重新解释了先前缓存下来的辅助布局字段”，而不是误判为切阶段瞬间有新的 `owner_state.slot`/`2016.slot` 被写坏。

[Dota2 若 7047/launch/046 全链都保持同一 team/slot，进游戏错位更可能发生在 GC 下游]
- Date: 2026-05-01
- Context: Agent 在顺序读完整轮 `gbe_gc_debug.log`，并验证“主机载入中”已解决后继续排查 slot 错位时发现
- Category: 代码模式
- Instructions:
  - 如果日志里从 `7047`、`7041`、`4511/4506`、多轮 official `7034 -> 26` 到最终 `046`/runtime snapshot 都持续显示同一个 `owner_state.team/slot` 与 `2016.member.team/slot`，则 GC/SO builder 侧大概率已经把槽位保持住了。
  - 这类情况下，后续不应继续优先修改 `2004/2016/7034` 的 GC 重写逻辑；更应怀疑游戏启动后的下游消费方，例如 server/game init 对 slot 的解释方式、team-local slot 编码或其他非 GC 数据源。
  - 本轮样本里 `team=1 slot=3` 在 `7047`、launch donor rewrite、`043/046` 以及 runtime private snapshot 中都保持一致，可作为“GC 侧未丢槽”的判据样本。

[Dota2 当前 lobby 运行态应先捕获快照，再交给 24/26 builder 消费]
- Date: 2026-05-01
- Context: Agent 在为 practice lobby 做最小去补丁化收束、梳理 `cache_template_replay`、`cache_payload`、`details_update` 与 `replay_current_private_lobby_snapshot` 的公共输入时发现
- Category: 代码模式
- Instructions:
  - `24/26` 相关 builder 不应各自零散地直接读取 `GBE_local_lobby` 多个字段；应先通过统一入口捕获一次当前 lobby 快照，再把该快照传给 builder，减少同一语义被多条路径重复拆装。
  - 这个统一入口默认可以先执行 `GBE_RestoreSharedDotaLobbyState(...)`，但从 `GBE_RestoreSharedDotaLobbyState(...)` 内部触发的 snapshot replay 路径必须禁用二次 restore，否则会形成 restore -> replay -> restore 的递归链。
  - prelaunch/launch 的阶段判断也应复用同一个谓词，例如以 `server_id/match_id/game_start_time/connect` 是否都为空来统一定义 prelaunch，而不是在不同函数里重复写判断条件。

[Dota2 scratch prelaunch 2004.field17 不能继续发两个空 team_details]
- Date: 2026-05-01
- Context: Agent 在继续排查 prelaunch direct `26` 仍需保留 scratch builder 的前提下，复查 `GBE_BuildDotaPracticeLobbySOObjectData(...)` 与 `go-dota2` 的 `CSODOTALobby.team_details` 结构时发现
- Category: 代码模式
- Instructions:
  - `CSODOTALobby.field 17` 的元素类型是 `CLobbyTeamDetails`，不是可随意留空的占位字段；当前 scratch builder 若连续发两个空 message，会把客户端看到的队伍详情与完成度视图压扁。
  - 在 launch 前 `26` 仍不得误走 official `046` donor 的前提下，scratch `2004` 至少应保留两个最小非空 `team_details` 条目，而不是两个空壳字段。
  - 一个可接受的最小形态是显式写出 `team_complete=false`，并用 `is_home_team=true/false` 区分两侧，这样既不引入额外猜测字段，也避免覆盖掉 team-details 结构本身。

[Dota2 同 lobby 的 runtime 恢复不能只重放 rich presence，还要补当前 private lobby snapshot]
- Date: 2026-05-01
- Context: Agent 在继续排查“dashboard 仍显示主机载入中”并顺序对照最新 `gbe_gc_debug.log` 与 `GBE_RestoreSharedDotaLobbyState(...)` 后发现
- Category: 代码模式
- Instructions:
  - 新的 client coordinator 实例走 `restore_client_runtime` 增量恢复路径时，虽然会同步 `state/game_state/server_id/connect` 并重放 rich presence，但旧逻辑不会像 `initialize_gc` / `restore_client_full_adopt` 那样调用 `GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(...)`。
  - 当 lobby 已进入 `state=2, game_state=4` 时，只重放 `#DOTA_RP_PRIVATE_LOBBY` 仍可能不足以让 dashboard 恢复当前 SO 视图，表现为 profile 正常但大厅仍停在主机载入中。
  - 因此同 lobby 的 runtime 恢复完成后，也应补发当前 private lobby 的 `24/26` snapshot，而不是只补 rich presence。

[Dota2 prelaunch direct 26 不能无条件复用 official 046 donor]
- Date: 2026-05-01
- Context: Agent 在顺序完整阅读本轮 `/workspace/console.log` 与 `/workspace/gbe_gc_debug.log`，并对照 `GBE_BuildDotaPracticeLobbyDetailsUpdatePayload(...)` 后发现
- Category: 代码模式
- Instructions:
  - 建房后的 `7047 / SetTeamSlot` 与 `7046 / SetDetails` 会在 launch 之前立即触发 direct `26 / LobbyDetailsUpdate`；本轮客户端日志里这三次更新都固定收到 `26 size=470`，对应服务器日志里的 `current direct 26 details update`。
  - 当这些 prelaunch `26` 误走 official `046` donor 路径时，日志会出现 `lobby_id/server_id/match_id/game_start_time/connect patch skipped` 或 `size mismatch`，而客户端侧虽然不一定立刻报错，但房间名和槽位更新会失效，表现为 lobby SO 被带坏。
  - 因此 `GBE_BuildDotaPracticeLobbyDetailsUpdatePayload(...)` 至少要按 runtime 字段分流：`server_id/match_id/game_start_time/connect` 仍为空的 prelaunch 阶段继续使用旧的本地直拼 `26`，只有进入 launch/runtime 后才允许复用 official `046` donor。

[Dota2 7034 请求里的 draft 字段也可作为 owner_team/owner_slot 的回填来源]
- Date: 2026-05-01
- Context: Agent 在继续排查“当前测试里没有 7047，但 owner_team 长期停在 0”并核对 `dll/steam_game_coordinator.cpp` 的 `7034` 解析与 builder 后发现
- Category: 代码模式
- Instructions:
  - `7034` 的 protobuf 顶层 `field 16` 是 draft 条目，内部结构与 connected players 回复里构造的 draft 一致：`field 1=steam_id`、`field 2=team`、`field 3=team_slot`。
  - 当前代码原先只从 `7034` 提取 `connected_player.steam_id/hero_id`，没有解析 draft，因此在 `7047 / SetTeamSlot` 缺失时，`owner_team/owner_slot` 会一直保留初始化值。
  - 当 `7034.draft.steam_id` 命中 lobby owner 时，应优先把 `draft.team` 回填到 `owner_team`，并把零基 `draft.team_slot` 转回本地一基 `owner_slot`。

[Dota2 lobby owner_team 必须保留原始 Dota 队伍号而不是压成 0/1]
- Date: 2026-05-01
- Context: Agent 在继续排查“rich presence 已经是 PRIVATE_LOBBY，但 dashboard 仍显示主机载入中”并顺序阅读最新 `console.log` 与 `gbe_gc_debug.log` 后发现
- Category: 代码模式
- Instructions:
  - `7047 / SetTeamSlot` 里记录的队伍号在当前场景下是 Dota 原始队伍号，日志可见真实 host 被分到 `team 2`；不能假设只有 `0/1` 两种值。
  - 如果在 `2016.member.team`、`7034 draft.team` 或 runtime `2016` builder 里用 `owner_team <= 1 ? owner_team : 0` 这类逻辑，会把真实的 `team 2/3` 错写成 `0`，从而出现 `hero_id` 已正确同步但 `owner_state.team/member.team` 长期为 `0` 的视图断层。
  - `2004.owner_state.team`、`2016.member.team`、`7034` 相关 draft/team 字段应统一保留当前 `owner_team` 原值；若需要判断 dire 侧，至少要兼容 `team=3`。

[Dota2 晚期 Steam 侧 5410/5432 更适合作为时序闩锁而不是固定字节模板]
- Date: 2026-05-01
- Context: Agent 在继续实现 practice lobby host startgame 的官方晚链 `5429 -> 5410 -> 5432/5575 -> 766`，并对照 `/workspace/steamhoststart-hero/` 与 `/workspace/steamhoststartlobbyandleave/` 的官方抓包时发现
- Category: 代码模式
- Instructions:
  - 官方两份样本里的晚期 `5410 / k_EMsgClientGamesPlayedWithDataBlob` 与 `5432 / k_EMsgClientAuthList` 都是客户端外发消息，应该在 `SendMessage_ -> handle_dota_client_message -> GBE_HandleDotaDirectPostLoginRequest(...)` 路径上观察并驱动阶段推进，而不是伪造成客户端入站消息。
  - 这两类消息的包体长度在不同官方样本里并不固定，例如 `5410` 既有 `114` 也有 `116` bytes，`5432` 既有 `139` 也有 `40` bytes，因此更稳妥的实现是把它们当作“晚期官方时序标记”来消费，而不是做固定 donor 字节白名单匹配。
  - `046` 后的第二轮 `5575` 与最终 `766 PRIVATE_LOBBY` 应在观察到晚期 `5410 -> 5432` 顺序后再补发；`7197/8673` 不应继续抢在这条官方 Steam 链之前触发第一次 `PRIVATE_LOBBY` 切换。

[Dota2 official 032 后的 037/038 CacheSubscribed 不能改成 lobby owner]
- Date: 2026-05-01
- Context: Agent 在继续排查 `032` 后两条 `24` 导致客户端立即报 `Lobby object destroyed`，并直接解码 `/workspace/lobbystartgamedota2/037_in_5453_k_EMsgClientFromGC.bin` 与 `038_in_5453_k_EMsgClientFromGC.bin` 的 protobuf 后发现
- Category: 代码模式
- Instructions:
  - 官方 `037/038` 的 `CMsgSOCacheSubscribed.owner_soid` 是 `type=1, id=<owner steamid>`，不是 `type=3, id=<lobby_id>`。
  - 其中 `037` 没有任何对象，`038` 只包含 `type_id=1` 与 `type_id=2010`，并不携带 `2004` 大厅对象。
  - 因此在重放这两条官方 prelude `24` 时，不能像 lobby `24` 那样强行调用 `GBE_ForceDotaLobbyCacheOwnerSOID(...)` 把 owner 改成当前 lobby；否则客户端会把当前 lobby cache 覆盖成一组不含大厅对象的缓存，触发 `Lobby object destroyed`。

[Dota2 的 28->29 仅回 owner_soid 不足以恢复官方 8744 后半程]
- Date: 2026-05-01
- Context: Agent 在按顺序完整阅读用户新上传的 `gbe_gc_debug.log`，并确认 `2433f7ae` 已让 `28 / CacheSubscriptionRefresh -> 29 / CacheSubscribedUpToDate` 成功发生后发现
- Category: 代码模式
- Instructions:
  - 当日志里已经出现 `replying req=28 resp=29`，但 `29` 仍是最小 `owner_soid only` 版本时，官方 `8744 -> 8745 -> 043 -> 046` 链依然可能完全不出现。
  - 此时下一步不应再重复补“有没有 29”，而应优先让 `29` 镜像最近一次 lobby `24 / CacheSubscribed` 中的 `version`、`service_id`、`service_list`、`sync_version` 元数据。
  - 这些字段应以最近一次实际发送给客户端的 lobby `24` 为准，尤其是 `032` 之后那组 launch prelude `24`，避免凭空猜测或跨阶段复用错误的缓存版本。

[Dota2 official 032 之后到 8744 之前还有一段当前实现未接线的 cache prelude]
- Date: 2026-05-01
- Context: Agent 在按顺序对照 `/workspace/lobbystartgamedota2/` 与 `dll/steam_game_coordinator.cpp` 的 practice lobby host startgame 链路时发现
- Category: 代码模式
- Instructions:
  - 官方样本在 `032` 之后、`8744` 之前还有一段 `5429 + 24 + 24` 的 prelude，其中首条小 `24` 对应当前代码里的 `GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedPreludeTemplateReplay(...)`。
  - 该 prelude builder 目前只有定义，没有任何实际 launch 状态跃迁会调用它，因此日志里虽然已有 `032/8744/043/046`，但仍会完全缺失 `037/038` 这两条 `CacheSubscribed`。
  - 后续如果 dashboard 仍停在 host loading，应优先检查这段 prelude cache 是否已在 `032 -> 8744` 窗口按时入队，而不是只盯 `043/046` 或 rich presence。

[Dota2 official late launch 还会重复发送 5501 与 5575 外围消息]
- Date: 2026-05-01
- Context: Agent 在继续对照 `/workspace/lobbystartgamedota2/032-048` 与 `steam_game_coordinator.cpp` 的 practice lobby startgame 后半段时发现
- Category: 代码模式
- Instructions:
  - 官方样本除了早期已有的 `5501/5575` 之外，在 `032` 之后还会再出现一轮 `034_in_5501 -> 035_in_5429 -> 036_in_5575`，并且在 `8744/8745 -> 043/046` 附近还会再出现 `041_in_5501` 与 `048_in_5575`。
  - 因此如果日志里已经有 `032 -> 5429 -> 24 -> 24 -> 8744 -> 8745 -> 043 -> 046`，但 dashboard 仍停在 host loading，不能只关注 `24/26`；还要核对这些重复的 `5501/5575` 是否也在相邻窗口按时重放。
  - 现有 `5501` 模板大小与官方 `034/041` 一致，`5575` 的 stage1/stage2 模板大小也与官方 `036/048` 一致，后续优先复用这些外围模板补齐时序，而不是先发明新的 payload 结构。

[分析上传日志时必须顺序逐行阅读]
- Date: 2026-05-01
- Context: 用户要求“上传了，不要跳着读，逐行读寻找问题”
- Instructions:
  - 后续当用户上传新的日志文件并要求排查问题时，必须按文件顺序分段逐行阅读，不要先用 grep 跳读后再下结论。
  - 允许把大文件按连续区间分段读取，但每段都应保持原始顺序，直到读完整个相关日志范围。

[Dota2 official 046 后的 766 persona 应立即切到 PRIVATE_LOBBY]
- Date: 2026-05-01
- Context: Agent 在继续对照 `/workspace/steamhoststartlobbyandleave/` 的官方 `766 / PersonaState` 序列时发现
- Category: 代码模式
- Instructions:
  - 官方序列里 `018_in_766_k_EMsgClientPersonaState.bin` 仍然是 `#DOTA_RP_FINDING_MATCH + SERVERSETUP`，但紧接着的 `021_in_766_k_EMsgClientPersonaState.bin` 与 `022_in_766_k_EMsgClientPersonaState.bin` 已经切成 `#DOTA_RP_PRIVATE_LOBBY + RUN`，而不是等到后面的 `7197/8673` 才切换。
  - 因此 practice lobby launch 的 `046` 后半程不应继续发送 `FINDING_MATCH` persona；若需要复用 server 变体模板，也必须把其 rich presence/status 同步改成 `#DOTA_RP_PRIVATE_LOBBY`。
  - 当前 `7197/8673` 更适合作为补发或兜底时机，不应作为第一次切到 `PRIVATE_LOBBY` 的主触发点。

[Dota2 抓包分析默认以 steamhoststartlobbyandleave 为主]
- Date: 2026-05-01
- Context: 用户要求“以后抓包数据主要看 steamhoststartlobbyandleave 里的”
- Instructions:
  - 后续分析 Dota2 练习房间/返回面板相关抓包时，默认优先参考 `/workspace/steamhoststartlobbyandleave/` 中的数据。
  - 如需对比其他目录抓包，应以 `steamhoststartlobbyandleave` 为主真值来源，避免混用不同场景样本得出错误结论。

[Dota2 本地 rich presence 变更后需要立即发本地刷新回调]
- Date: 2026-05-01
- Context: Agent 在继续排查 dashboard 仍显示 host loading，并对照 `steamhoststartlobbyandleave` 的 `7501/766/815` 刷新链与 `steam_friends.cpp` 时发现
- Category: 代码模式
- Instructions:
  - 当前 `Steam_Friends::SetRichPresence()` / `ClearRichPresence()` 如果只更新 `us.rich_presence` 并做网络广播，但不对本地自己触发 `FriendRichPresenceUpdate_t` 与 `k_EPersonaChangeRichPresence`，端侧 UI 可能拿不到即时刷新信号。
  - 官方样本在 `7501/766` persona 切换后还会伴随额外的好友数据刷新请求，因此本地实现至少要保证 rich presence 改动时立即给本地派发 rich presence/persona 变更回调，避免 dashboard/profile 停留在旧文案。

[Dota2 launch donor/runtime 的 2016 member 也必须同步 owner hero_id]
- Date: 2026-05-01
- Context: Agent 在分析“个人页面已显示私人房间，但大厅仍显示主机载入中”的新 `gbe_gc_debug.log` 时发现
- Category: 代码模式
- Instructions:
  - 当 `2004.state/game_state` 与本地 rich presence 都已经推进到 `state=2, game_state=4, #DOTA_RP_PRIVATE_LOBBY`，但 dashboard 仍卡在 loading，需继续核对 `2016.member[0]`。
  - 当前 donor `2016` 重写和 runtime `2016` 构建如果只写 `steam_id`，会导致 `member[0].hero_id` 长期停留在 `0`；即使 `2004.owner_state.hero_id` 已经被 `7034` 更新为正确英雄，大厅仍可能按旧 member 视图判定为未完成加载。
  - 修复时至少要把当前 `owner_hero_id` 同步写入 `2016.member[0].field 2`，并尽量保持 donor 原有的其他 member 字段不变。

[Dota2 新 coordinator 客户端实例必须从 shared lobby 完整恢复并重放 rich presence]
- Date: 2026-05-01
- Context: Agent 在继续排查“日志已到 PRIVATE_LOBBY 但 dashboard 仍显示主机载入中”并对照 `steam_game_coordinator.cpp` 的 constructor、`GBE_RestoreSharedDotaLobbyState(...)` 与最新 `gbe_gc_debug.log` 时发现
- Category: 代码模式
- Instructions:
  - launch 后半段可能出现新的 `Steam_Game_Coordinator` 客户端实例；如果此时新的实例 `GBE_local_lobby.active == false`，旧的“只对已激活且同 lobby 的客户端做增量同步”逻辑会直接跳过共享 lobby 恢复。
  - 这会导致日志里虽然已经执行过 `#DOTA_RP_PRIVATE_LOBBY` rich presence 更新，但切回 dashboard 时实际生效的新实例既没有当前 lobby runtime，也没有按当前 `state/game_state/server_id` 重新补 rich presence。
  - 修复此类问题时，应允许客户端在 `shared_lobby.active == true` 且本地 lobby 为空时完整 adopt shared lobby，并按当前阶段重放 rich presence；`state=1,game_state=0,server_id=0` 对应 `#DOTA_RP_INIT/SERVERSETUP`，`state=1,game_state=0,server_id!=0` 对应 `#DOTA_RP_FINDING_MATCH/SERVERSETUP`，`state=2` 对应 `RUN`，其中 `game_state=4` 对应 `#DOTA_RP_PRIVATE_LOBBY`。

[Dota2 退出练习房间时官方 rich presence 会先停留在 PRIVATE_LOBBY 再回到 INIT]
- Date: 2026-05-01
- Context: Agent 在分析最新的 `steamhoststartlobbyandleave.zip` 官方抓包时发现
- Category: 代码模式
- Instructions:
  - 官方在点击返回主界面后的离场阶段，不是直接从 `#DOTA_RP_PRIVATE_LOBBY` 跳到 `#DOTA_RP_INIT`；中间会先看到一次 `7501/766` 仍然维持 `#DOTA_RP_PRIVATE_LOBBY` 与 `party_state: IN_MATCH`，随后才出现 `#DOTA_RP_INIT` 的回落。
  - 这说明离场逻辑需要保留一个短暂的私房间态过渡，而不是在收到 leave/destroy 时立即把 rich presence 清成空值；最终回到主界面后才应切到 `#DOTA_RP_INIT`。
  - 官方抓包里还出现了 `PostGame_<lobby_id>` 相关字符串和一次额外的 `RequestFriendData`/`ServiceMethodResponse` 刷新，说明返回主界面阶段会伴随一次好友数据重载。

[Dota2 客户端同 lobby 的 shared runtime 恢复也必须同步 state 和 game_state]
- Date: 2026-05-01
- Context: Agent 在分析一轮修复后的新 `gbe_gc_debug.log` 时发现 `PRIVATE_LOBBY` 已设置成功，但很快又被客户端侧 `restore_client_runtime` 重放回 `FINDING_MATCH`
- Category: 代码模式
- Instructions:
  - 当客户端 `GBE_local_lobby` 已经 active 且 lobby_id 与 shared lobby 相同，旧逻辑若只同步 `server_id/connect/match_id/game_start_time`，会保留陈旧的 `state=1,game_state=0`。
  - 这会导致后续 `GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(...)` 按旧状态再次写出 `#DOTA_RP_FINDING_MATCH / SERVERSETUP`，把刚刚进入 `PRE_GAME` 时设置的 `#DOTA_RP_PRIVATE_LOBBY / RUN` 覆盖掉。
  - 因此客户端增量恢复路径必须至少同步 `state`、`game_state`，并一并带上 `owner_team/owner_slot` 等与 connected player/rich presence 推断相关的运行态字段。

[Dota2 PRE_GAME 的 owner hero 必须从 7034 connected player 贯穿到 2004 member field 2]
- Date: 2026-04-30
- Context: Agent 在继续排查“已进入游戏但 dashboard 仍显示主机载入中”并对照 `console.log` 与官方 `046` donor 时发现
- Category: 代码模式
- Instructions:
  - `7034 / PLAYER_HERO` 请求里的 connected player `field 2 = hero_id` 不是可忽略噪音；当本地真实请求 hero 与 donor 宿主 hero 不一致时，后续 donor `046` 或 runtime `26` 若继续保留 donor 原值，会把 `CSODOTALobby.all_members[0].hero_id` 固定在错误英雄上。
  - practice lobby 的 `2004.field 120` 成员对象里，`field 2` 就是 member hero_id；donor 重写函数和 runtime `GBE_BuildDotaPracticeLobbySOObjectData(...)` 都必须同步写入当前 owner hero。
  - 运行态上应在收到 `7034` 的 connected player hero 后更新共享 lobby state，并在 create/leave/destroy 时显式清零该 hero，避免旧局 hero 残留到下一次 lobby 生命周期。

[Dota2 official 8745 donor 本身是零 job header]
- Date: 2026-05-01
- Context: Agent 在继续对照 `/workspace/lobbystartgamedota2/039-046` 官方 startgame 样本并比对本地 `8744 -> 8745` 日志时发现
- Category: 代码模式
- Instructions:
  - 官方 `042_in_5453_k_EMsgClientFromGC.bin` 的总长度是 `56` bytes，字节前缀与当前 `GBE_kDotaOfficial8745TemplateHex` 一致，说明 `8745` 的 body 模板本身没问题。
  - 官方 `042` donor 的 protobuf 扩展头里没有 `job_id_target`，因此它只能证明“wrapped 官方样本的 donor 头长这样”，不能单独推出当前 direct 运行时回复也必须保持空 job 头。

[Dota2 8744/8745 要区分 wrapped 官方样本与 direct 运行时路径]
- Date: 2026-05-01
- Context: Agent 在继续排查“客户端收到 8745 但 10 秒后仍报 reply timeout”并对照最新 `console.log`、`gbe_gc_debug.log` 与官方 `039/042` 样本时发现
- Category: 代码模式
- Instructions:
  - `/workspace/lobbystartgamedota2/039_out_5452_k_EMsgClientToGC.bin` 与 `042_in_5453_k_EMsgClientFromGC.bin` 属于 wrapped `5452/5453` 路径；它们的内层 `8744/8745` donor 可没有 direct protobuf 扩展头里的 job 对应关系。
  - 当前真实运行日志里的 `8744` 却是 direct 请求：`Send msg 8744, 17 bytes`，并且 `GBE_HandleDotaDirectPostLoginRequest(...)` 解析到 `source_job=28`。
  - 因此在当前 direct 路径下，`8745` 不能简单照搬 wrapped donor 的“空 job 头”语义；若客户端明确带了 `source_job` 发起 direct 请求，回复必须像其他 direct replay 一样回填 `job_id_target`，否则客户端虽然会打印 `Recv msg 8745`，仍可能继续保留 pending reply 并在 10 秒后报超时。

[Dota2 那次 ingame -> dashboard UI 切换是手动操作]
- Date: 2026-05-01
- Context: 用户澄清 `DOTA_GAME_UI_DOTA_INGAME -> DOTA_GAME_UI_STATE_DASHBOARD` 那次切换是手动切出，不应再当作自动回退现象分析
- Instructions:
  - 后续排查“主机载入中”时，不要再把该次 `console.log` 中的 `DOTA_GAME_UI_DOTA_INGAME -> DOTA_GAME_UI_STATE_DASHBOARD` 视为 GC/lobby 自动 bounce 证据。
  - 仍应继续关注真正未解决的问题：大厅 UI 长期停留在“主机载入中”。

[Dota2 game_state=10 后的 7034 不能先落到 runtime 26 fallback]
- Date: 2026-05-01
- Context: Agent 按顺序完整阅读用户上传的 `gbe_gc_debug.log` 后发现 `8744` 已修复但官方后半程仍被截断
- Category: 代码模式
- Instructions:
  - 当日志已出现官方 `8744 -> 8745(size=56)`，且 lobby 已推进到 `state=2, game_state=10` 时，下一条或接下来几条 `7034` 仍属于官方 `043/046` donor 链的一部分，不能先掉进 `7034_launch_poll` 的通用 runtime `26` fallback。
  - 如果 `send_reason` 解析不稳定，应该通过 `8744` 后的显式闩锁来保证下一条 `7034` 优先回放官方 `043`，随后再进入 `046`，而不是仅靠 `send_reason == 2/5` 的单点判断。
  - 一旦处于 `pending_043` 或 `pending_046` 这类官方后续阶段，应先消费官方 donor 链，再考虑 runtime `26` 兜底。

[Dota2 PRE_GAME 后不要用通用 runtime 26 覆盖 donor 046 建立的 lobby 视图]
- Date: 2026-04-30
- Context: Agent 在对照新上传的 `gbe_gc_debug.log` 与 `console.log`，排查 dashboard 仍显示 host loading 时发现
- Category: 代码模式
- Instructions:
  - `PRE_GAME` 阶段首个 `PLAYER_HERO` `7034` 之后，若后续同类 `7034_launch_poll` 回落到当前通用 `GBE_SendDotaPracticeLobbyDetailsUpdate(...)` 生成的精简 runtime `26`，客户端会删除 donor `046` 刚建立的 `CSODOTALobby.all_members[0].hero_id`，并清空一批 `team_details`、事件显示和 lobby 时间字段。
  - 这类字段回退比顶层 `CSODOTALobby.state` 更像 dashboard 仍显示 loading 的根因；官方 `043/046` 的 `2004.state` 始终保持 `RUN`，问题不在继续推进顶层 state。
  - 因此 `state=RUN && game_state=PRE_GAME && send_reason=PLAYER_HERO` 的后续轮询，应继续使用 `046` 风格 donor，而不是通用 runtime `26`。

[Dota2 PRE_GAME 后的 official 046 只应在 043 之后消费一次]
- Date: 2026-04-30
- Context: Agent 在继续对照 `/workspace/lobbystartgame.log` 与本地 `PLAYER_HERO` 轮询日志，排查“返回主界面仍显示主机载入中”时发现
- Category: 代码模式
- Instructions:
  - 官方 practice lobby host startgame 在 `8744/8745 -> 043` 进入 `PRE_GAME` 后，只观察到一次 `PLAYER_HERO` 的 `7034 (39 bytes)` 紧跟一条 `046` 风格 `26 (514 bytes)`，用于把 `CSODOTALobby.all_members[0].hero_id` 推到客户端。
  - 后续周期性的 `PLAYER_HERO` `7034` 轮询不应继续无限重放 official donor `046`；否则本地会持续发出额外的 `519-byte 26`，并可能让 dashboard 里的 host 状态长期停留在 loading 样式。
  - 实现上应把 `046` 视为 `043` 之后的一次性 follow-up：在进入 `PRE_GAME` 的 official `043` 路径置位，首个命中的 `PRE_GAME + send_reason=PLAYER_HERO` 请求消费后清掉，后续回到常规 runtime lobby update。

[Dota2 late-stage official 26 donor 应按阶段决定是否重写 2015]
- Date: 2026-04-30
- Context: Agent 在继续压缩 practice lobby host startgame 后半段 `26` 包体并对照官方 `024/025/030/032/043/046` donor 时发现
- Category: 代码模式
- Instructions:
  - `official 018` 与 `official 021` 这类 launch early-stage donor 仍需要重写 `2015`，并按当前 owner account 补一份 startup account data。
  - `official 024/025/030/032/043/046` 这类 late-stage donor 的 `2015` 应优先保留 donor 原始小对象，不要再统一强制清空并追加 startup account data；否则本地 `26` 容易从官方 `512/514` 膨胀到 `922/924`。
  - 继续排查 UI 状态错乱时，应优先关注这些 late-stage `26` 的 `2015` 是否仍被错误放大，而不是先改动官方消息顺序。

[Dota2 官方 launch prelude 里的首个 7034 不能提前回复 018 或 runtime 26]
- Date: 2026-04-30
- Context: Agent 在继续按 `/workspace/lobbystartgamedota2/` 与 `/workspace/lobbystartgame.log` 逐条核对 `4508 -> 7034 -> 4511 -> 24 -> 4506 -> 26` 时发现
- Category: 代码模式
- Instructions:
  - 官方 practice lobby host startgame 的 prelaunch 窗口里，首个 `7034` 出现在 `4508` 之后、`4511` 之前，这一拍不应提前下发 `018` 风格的 `26`，也不应回退成 runtime `26`。
  - 当本地 lobby 仍处于 `state=1, game_state=0` 时，`7034` 应等待官方后续 `4511 -> 24 -> 4506 -> 018/26` 的时序推进，而不是把缺失 `4506` 的恢复逻辑硬挂在这个首个 `7034` 上。
  - 如果后续仍看不到 `4506`，应先复查本地为什么没进入官方链路，而不是先在首个 `7034` 上发明替代回复。

[Dota2 4511 后的 016 donor 不应强依赖旧 steam_id varint 模板命中]
- Date: 2026-04-30
- Context: Agent 在基于 `a29a71dc` 新增的阶段日志复查 `4511` 后 official launch cache `24` 构建失败时发现
- Category: 代码模式
- Instructions:
  - `practice lobby launch official cache template` 在 `4511` 窗口使用的 donor `016` 可能不暴露 `GBE_kOldDotaSteamIdVarint` 这组旧 `steam_id` 模板字节。
  - 当 `account_id` 语义改写已只是 skipped/no replacements，而失败点收敛到 `launch cache template identifier patch failed` 时，应优先把顶层 `steam_id` varint 模板替换从 hard failure 放宽为“未命中则记录日志并继续”。
  - 后续真正需要保证的运行态字段，应优先依赖 inner `24` 对象重写与 owner SOID 修正，而不是要求 donor 一定包含旧 `steam_id` 顶层模板字节。

[Dota2 host startgame 已可进入完整 launch 状态链，但 4511/4506 后包体大小仍未完全贴齐官方]
- Date: 2026-04-30
- Context: Agent 在复查基于 `7f9a3342` 的新一轮 `/workspace/gbe_gc_debug.log` 与 `/workspace/console.log` 时发现
- Category: 代码模式
- Instructions:
  - 当前实现已能稳定走通 `4511 -> 24 -> 4506 -> 26 -> 7034 -> 021 -> 8870 -> 024/025 -> 8330/8331 -> 030 -> 032 -> 8744/8745 -> 043 -> 046`，并成功进入游戏。
  - 其中关键转折是：放宽 `4511` 后 official launch cache `24` 的顶层 `steam_id` varint 模板依赖后，`queued official-template CacheSubscribed after server_id sync` 成功出现，后续整条状态机恢复正常。
  - 当前本地包体大小仍与官方抓包不完全一致，例如 `4511` 后 `24` 为 `919` 而不是官方 `873`，`4506` 后首个 `26` 为 `919` 而不是官方 `873`，后续多数 `26` 为 `922/924` 而不是 `911/917/512/514`；因此若后续目标是字节级贴齐，仍需继续精修对象集合和字段重写。

[Dota2 4511 后目标 873-byte CacheSubscribed donor 对应抓包 016 而不是 038]
- Date: 2026-04-30
- Context: Agent 在继续对照 `/workspace/lobbystartgamedota2/` 中 `ClientFromGC` 样本大小与 `/workspace/lobbystartgame.log` 的 `4511 -> 24` 时序时发现
- Category: 代码模式
- Instructions:
  - `016_in_5453_k_EMsgClientFromGC.bin` 的文件大小是 `913`，与日志中的内层 `24 / CacheSubscribed` `873 bytes` 高度对应，说明它很可能就是 `4511` 后那条目标 donor wrapped 样本。
  - 当前误用的 `038_in_5453_k_EMsgClientFromGC.bin` 文件大小是 `6417`，对应日志里那条大 `24 / CacheSubscribed` `6377 bytes`，不能再拿它当 `4511` 后的 official cache template。
  - 后续若再核对 `4511` 后的 official cache template，应优先从 `016` 这类 `913 -> 873` 的 wrapped 样本验证，而不是从 `038` 这类大缓存样本出发。
  - 进一步直接解包后已确认：`016` 的内层消息类型就是 `24 / CacheSubscribed`，对象顺序为 `2004 -> 2013 -> 2014 -> 2015 -> 2016`，各 `object_data` 长度分别约为 `183 / 0 / 13 / 405 / 201`。
  - 同样直接解包后已确认：`038` 的内层虽然也是 `24 / CacheSubscribed`，但对象只有两类，分别是 `type_id=1` 的 51 个条目和 `type_id=2010` 的 87 个条目，属于完全不同的大缓存集合，不是 practice lobby launch 的那套对象。

[当官方抓包已给出正确时序时，优先反查本地处理而不是删消息]
- Date: 2026-04-30
- Context: 用户在指出“抓包里 4511 之后就是 24，这是官方正确时序”时明确纠正 Agent 的排障方式
- Instructions:
  - 当官方抓包已经明确给出正确时序时，不能因为本地遇到异常就先假设“跳过某条消息”或“官方不需要这条消息”。
  - 优先反查本地对该消息的构造、重写、封装、owner、对象顺序和字段处理，先证明是自己的处理不一致，再谈调整时序。
  - 下结论前必须先仔细核对抓包数据与本地日志，避免把“本地处理有问题”误判成“官方链路不需要这条消息”。

[Dota2 host startgame 必须严格按官方抓包顺序和结构构建回复]
- Date: 2026-04-30
- Context: 用户在上传新日志时再次强调“严格按照我抓包的数据顺序、结构来构建和回复”
- Instructions:
  - Dota2 host startgame 相关的 GC 构造与回复，必须以官方抓包为唯一时序和结构真值。
  - 构造 `24/26` 等消息时，优先复用官方 donor/template，并严格保持官方的消息顺序、对象顺序、顶层结构和封装方式；只有在确认字段需要运行态替换时才做最小改写。
  - 遇到本地异常时，优先证明“本地构造与官方哪里不一致”，而不是先发明新时序、新结构或删除官方存在的消息。

[Dota2 官方 018 donor 的 2015 对象不能重复追加 startup account data]
- Date: 2026-04-30
- Context: Agent 在对比 `/workspace/gbe_gc_debug.log` 中 `official_018_local_reply` 与 `GBE_kDotaOfficial018PracticeLobby26Hex` 的 inner `26` 时发现
- Category: 代码模式
- Instructions:
  - 当前 `official_018_local_reply` 比 donor inner `26` 多出 405 bytes，其中核心差异是 `type=2015` 对象从 donor 的 411 bytes 膨胀到本地的 814 bytes。
  - 本地 `2015` 中 `local[256..639]` 会被整块重复到 `local[659..1042]`，根因是 `GBE_RewriteDotaLobbyTemplateObject2015(...)` 忽略了 `clear_existing_startup_data`，保留 donor 现有 startup account data 后又额外 append 一份。
  - donor-based `2015` 重写路径必须先移除已有的 `8869 / k_EMsgDotaLobbyAdditionalAccountData` 条目，再按当前 owner account 只补一份 startup data。

[Dota2 官方 018 donor 的 2004 对象必须显式重写 lobby_id]
- Date: 2026-04-30
- Context: Agent 在解码最新 `official_018_local_reply` 的 `2004/2015/2014/2016` 对象并与 donor `018` 对比时发现
- Category: 代码模式
- Instructions:
  - 即使 `server_id`、`match_id`、`game_start_time`、`connect` 等字段已经被后续 proto 重写覆盖，`GBE_RewriteDotaLobbyTemplateObject2004(...)` 仍可能遗漏 `field 1 = lobby_id`，导致 donor 原始 lobby id 被完整透传。
  - 当 `official_018_local_reply` 已收敛到接近官方大小但后续 `7034 -> 021 -> 8870` 仍未触发时，应优先解码 `type=2004`，确认 `lobby_id` 是否仍停留在 donor 值而不是当前运行态 lobby id。
  - donor-based `2004` 重写路径在 `rewrite_runtime_fields == true` 时必须显式重写 `field 1 = lobby_id`，不能只依赖前面的模板字节替换或 owner SOID 修正。

[Dota2 018 关键字段全对后若仍无第二个 7034，应转向排查客户端状态机]
- Date: 2026-04-30
- Context: Agent 在修复 `official_018_local_reply` 的 `2015` 重复块和 `2004.lobby_id` 残留后，复查新一轮 `gbe_gc_debug.log` 时发现
- Category: 代码模式
- Instructions:
  - 当 `official_018_local_reply` 已收敛到 `919 bytes`，且 `2004` 中的 `lobby_id/match_id/server_id/game_start_time/connect` 都已经是当前运行态值，但日志仍停在首个 `7034 -> 018` 之后，没有继续出现第二个 `7034 -> 021 -> 8870` 时，GC `018` 包体不再是最高优先级嫌疑。
  - 这类情况下应优先查看游戏侧/客户端侧状态机日志，确认是否真的进入了 `DOTA_GAMERULES_STATE_WAIT_FOR_PLAYERS_TO_LOAD`，以及为什么没有继续发送官方链路中的下一个 `7034`。

[Dota2 donor 26 重写后必须强制刷新 top-level owner_soid]
- Date: 2026-04-30
- Context: Agent 在对照 `/workspace/console.log` 与 `official_018_local_reply` 顶层字段时发现收到 `919` bytes 的 `018` 后立即出现 `Lobby object destroyed, previous lobby_id=0, match_id=0`
- Category: 代码模式
- Instructions:
  - donor-based `26 / CMsgSOMultipleObjects` 即使内部 `2004/2015/2014/2016` 字段都已改成运行态值，顶层 `owner_soid` 仍可能保留 donor 旧 lobby id。
  - 这种情况下 `gbe_gc_debug.log` 可见 `field 6` 仍是 `type=3 id=<donor_lobby_id>`，而 `console.log` 会在收到 `26` 后立刻打印 `Lobby object destroyed`，且没有后续 `WAIT_FOR_PLAYERS_TO_LOAD` 或第二个 `7034`。
  - 所有 donor 重写得到的 `26`（包括 `official 018/021/...` 和 launch stage donor `26`）在最终返回前都必须调用 `GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id)`，不能只改内部对象字段。

[Dota2 4511 后的 synthetic runtime CacheSubscribed 可能需要完全跳过]
- Date: 2026-04-30
- Context: Agent 在连续对照 `/workspace/console.log` 与 `/workspace/gbe_gc_debug.log` 时发现 `4511` 后收到 runtime `24 size=704` 之前就会先出现两次 `Lobby object destroyed, previous lobby_id=0, match_id=0`
- Category: 代码模式
- Instructions:
  - 即使已经把 `4511` 后的双 `24` 收敛成单条 runtime `24`，这条 synthetic `CacheSubscribed` 仍可能在客户端侧先把 lobby cache 冲掉，然后导致后续 `7034 -> official 018` 收到后再次 `Lobby object destroyed`。
  - 当日志呈现 `4511 -> recv 24(size=704) -> Lobby object destroyed x2 -> 4508 -> 7034 -> recv 26(size=919) -> Lobby object destroyed` 这种模式时，应优先尝试完全跳过 `4511` 后下发给客户端的 runtime `24`，只保留共享运行态同步与后续 donor `26` progression。

[Dota2 4511 后的 24 应优先使用官方 launch cache template replay]
- Date: 2026-04-30
- Context: Agent 在收到用户纠正“抓包里 4511 之后就是 24”后，继续对照 `/workspace/lobbystartgame.log` 与本地日志时发现官方 `4511` 后的 `24` 是 `873 bytes`，而本地 synthetic `24` 只有 `704 bytes`
- Category: 代码模式
- Instructions:
  - `4511` 后的 `24` 不能简单删除；官方链路里确实存在这条 `CacheSubscribed`。
  - 当本地 `4511` 后的 `24` 明显比官方小很多（例如 `704` vs `873`），并且客户端在收到该 `24` 时立刻出现 `Lobby object destroyed`，应优先把该 `24` 的构造从 `GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(...)` 切换到 `GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplay(...)`，复用官方 launch cache donor 模板再做运行态字段重写。
  - “跳过 `4511` 后的 `24`”只能作为临时排障猜想，不应作为最终修复方向；用户已确认官方抓包里存在这条 `24`。

[当前 4511 后选到的 official cache template 仍与抓包不符]
- Date: 2026-04-30
- Context: Agent 在将 `4511` 后的 `24` 改为 `GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplay(...)` 后复查新日志时发现
- Category: 代码模式
- Instructions:
  - 当前本地 `4511` 后的 `24` 已不再是 synthetic `704 bytes`，但变成了 `6376 bytes`，而官方抓包里的对应 `24` 仍是 `873 bytes`。
  - 这说明“改用 donor template”这个方向是对的，但当前选用的 template/提取方式/封装层级仍然不对，不能把 `6376` 当成已经贴近官方。
  - 下一步应继续严格对照抓包，确认 `GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex` 是否对应错误样本、是否提取了错误层级，或是否把额外对象/封装一并发给了客户端。

[Dota2 官方 018 donor 的剩余 LaunchTemplate 模板 patch 应一次性放宽]
- Date: 2026-04-30
- Context: Agent 在连续多轮 host startgame 调试中发现首个 `7034 -> official 018` 路径会沿着 `lobby_id -> match_id -> server_id -> game_start_time` 逐个暴露新的固定模板命中失败
- Category: 代码模式
- Instructions:
  - 为了尽快让首个官方 `018` 真正发出并产出 `official_018_local_reply`，`GBE_PatchDotaPracticeLobbyLaunchTemplate(...)` 中剩余依赖 donor 固定模板字节/字符串的 patch 应一次性放宽，而不是每轮只改一个。
  - 这包括 `steam_id fixed64`、`account_id fixed32`、`lobby_id/match_id/game_start_time` 的 varint 宽度不匹配与模板字节命中失败，以及 `connect` 固定字符串覆盖失败。
  - 这些 donor/模板命中类 patch 在 debug 阶段应记录 skipped 日志并继续发送；真正的字段残留问题在拿到 `official_018_local_reply` 完整 dump 后再做精确核对。

[Dota2 官方 018 donor 的 game_start_time patch 也不能走严格命中]
- Date: 2026-04-30
- Context: Agent 在继续修复 host startgame 的首个 `7034 -> official 018` 路径时，对照最新一轮 `gbe_gc_debug.log` 发现 `lobby_id`、`match_id`、`server_id` 放宽后又卡在 `game_start_time` 固定模板替换
- Category: 代码模式
- Instructions:
  - 当 `official packet 018 after 7034 fallback missing 4506` 已进入 donor 重放路径时，`LaunchTemplate` 内的 `game_start_time` 固定字节替换也不能再作为 hard failure。
  - 如果 donor 不暴露预期的 `game_start_time` 模板字节，应记录 skipped 日志并继续发送，优先让首个官方 `018` 真正发出，再利用 `official_018_local_reply` 完整 dump 精确核对残留字段。

[Dota2 官方 018 donor 的 server_id patch 也不能走严格命中]
- Date: 2026-04-30
- Context: Agent 在继续修复 host startgame 的首个 `7034 -> official 018` 路径时，对照最新一轮 `gbe_gc_debug.log` 发现 `lobby_id` 与 `match_id` 放宽后又卡在 `server_id` 固定模板替换
- Category: 代码模式
- Instructions:
  - 当 `official packet 018 after 7034 fallback missing 4506` 已进入 donor 重放路径时，`LaunchTemplate` 内的 `server_id` 固定字节替换也不能再作为 hard failure。
  - 如果 donor 不暴露预期的 `server_id` 模板字节，应记录 skipped 日志并继续发送，优先拿到真实发出的 `official_018_local_reply` 完整 dump，再判断 donor 中是否仍残留旧 `server_id`。

[Dota2 官方 018 donor 的 match_id patch 也不能走严格命中]
- Date: 2026-04-30
- Context: Agent 在继续修复 host startgame 的首个 `7034 -> official 018` 路径时，对照前一次测试日志发现 `lobby_id` 放宽后又卡在 `match_id` 固定模板替换
- Category: 代码模式
- Instructions:
  - 当 `official packet 018 after 7034 fallback missing 4506` 已经进入 donor 重放路径时，`LaunchTemplate` 内的 `match_id` 固定字节替换也不能再作为 hard failure。
  - 如果 donor 不暴露预期的 `match_id` 模板字节，应像 `lobby_id` 一样记录 skipped 日志并继续发送，先验证首个官方 `018` 是否能完整发出，再用完整 hex dump 对比残留字段。

[Dota2 官方 018 donor 的 lobby_id patch 不能走严格命中]
- Date: 2026-04-30
- Context: Agent 在继续修复 host startgame 缺失 `4506` 时，对照新一轮 `/workspace/gbe_gc_debug.log` 发现首个 `7034` 已命中官方 `018` 分支，但构建阶段仍失败
- Category: 代码模式
- Instructions:
  - 当首个 `7034` 作为缺失 `4506` 的恢复窗口去重放官方 `018` donor 时，`LaunchTemplate` 内的 `lobby_id` 不应再要求模板字节必须命中。
  - 如果日志出现 `Launch lobby_id patch failed/skipped stage=official packet 018 ...`，应把这类 `lobby_id` patch 视为可选项：找不到 donor 模板字节时继续发送后续语义改写后的消息，而不是整包失败并回退到 runtime `26`。

[Dota2 server connect 后首个 7034 可能替代缺失的 4506]
- Date: 2026-04-30
- Context: Agent 在逐条阅读新一轮 `/workspace/console.log` 与 `/workspace/gbe_gc_debug.log`、定位 host startgame 仍回主界面时发现
- Category: 代码模式
- Instructions:
  - 当前本地链路里，`7041` 启动后 server 侧可能不会实际发出 `4506`，而是直接在 `4511/4508` 之后收到首个 `7034`。
  - 如果此时 lobby 仍停在 `state=1, game_state=0`，不能让该 `7034` 落回 runtime `26`；应优先把它视作缺失 `4506` 的恢复窗口，先下发官方 `018` 风格的 `26`，把状态推进到 `2/0`。
  - 否则后续 `7034` 不会进入官方 `021/024/025/...` donor progression，容易再次触发客户端 `Lobby object destroyed`。

[Dota2 4511 后的 launch CacheSubscribed 应改为单条 runtime snapshot]
- Date: 2026-04-30
- Context: Agent 在继续修复 practice lobby host `start game` 卡在 `DOTA_GAMERULES_STATE_INIT`、并收口 `4511` 后 donor 双 `24` 时发现
- Category: 代码模式
- Instructions:
  - 当 `server_id` 在 `4511` 窗口首次从 `0` 同步成真实 gameserver SteamID 时，优先下发一条按当前运行态直构的 `24 / CacheSubscribed`，并携带 `extra_startup_account_id=owner account_id`。
  - 不要在这条 `4511` 后窗口继续发送 donor-based prelude 小 `24` 加 donor 大 `24` 的双 `24` 组合；这组 `41 + 6376/6377` 更容易触发客户端 `Lobby object destroyed`。
  - 这条 runtime `24` 应沿用当前权威 lobby 的 `state/game_state/server_id/match_id/connect`，不要把状态硬回退到模板默认值。

[网络问题处理约束]
- Date: 2026-04-28
- Context: 用户要求后续处理网络问题时避免修改 DNS 配置
- Instructions:
  - 以后遇到网络问题，不要修改 DNS 配置。
  - 如需处理网络访问异常，优先做只读诊断或采用不改 DNS 的替代方案，并先向用户说明。

[旧 steamapi 仓库的有效参考边界]
- Date: 2026-04-28
- Context: Agent 在全面排查 `/workspace/steamapi/unpacked/steam_api` 是否能帮助当前 Dota2 practice lobby / GC 卡点时发现
- Category: 代码结构
- Instructions:
  - 旧 `steamapi` 中的 `Steam_Game_Coordinator` 基本只是 `ISteamGameCoordinator` 的队列壳，真实的出站消费由 `SKYNET_API_Worker` 完成，不包含 Dota2-specific practice lobby、match auth、server hello/welcome 逻辑。
  - 该仓库真正可借鉴的是单进程内 client/server 双实例共享 `Networking`、通过本地回环和 lobby 快照/增量消息做状态同步的架构模式，而不是具体 GC 协议实现。

[Dota2 client/server GC 实例构造位置]
- Date: 2026-04-28
- Context: Agent 在继续排查 practice lobby start-game 阶段的 server-side GC 上下文同步时发现
- Category: 代码结构
- Instructions:
  - 当前 `gbe_fork` 会在同一个 `dll/steam_client.cpp` 中同时构造 client-side `steam_game_coordinator` 与 server-side `steam_gameserver_game_coordinator`。
  - 两个 coordinator 共享同一个 `network` 对象，但分别持有 `settings_client` 和 `settings_server`，因此 lobby 运行态默认仍是各自实例内状态，不能假设会自动同步。

[Dota2 7038 初始 CacheSubscribed 的 helper 边界]
- Date: 2026-04-28
- Context: Agent 在继续修复 `7038 / PracticeLobbyCreate` 崩溃、对齐提交 `a3561972e01a81c2c6c37ef9b070c5c284cc6182` 时发现
- Category: 代码模式
- Instructions:
  - `7038` 建房后发出的初始 `24 / CacheSubscribed` 只应按旧版 helper 语义改写 `2004` 和 `2014`，不要在这一步处理 `2015/2016`。
  - 当 `rewrite_runtime_fields == false` 时，`2004 / CSODOTALobby` 的成员相关字段 `120/121/122/123/124` 应保留 donor 原始结构，不要做运行态瘦身、重排或补写 `121=0`。
  - `2015/2016` 与 `2004` 成员结构压缩只保留给 `7041` 之后的运行态改写路径使用，不要混入建房初始 `24`。

[Dota2 开始游戏卡在本地网络连接状态回调]
- Date: 2026-04-28
- Context: Agent 在分析 `STEAM_LOG_1395829013.log` 与 `gbe_gc_debug.log` 的新一轮开始游戏日志时发现
- Category: 代码模式
- Instructions:
  - 如果 `7041` 后客户端已经把 lobby rich presence 推进到 `SERVERSETUP` 和 `RUN`，并且随后能看到 `SteamGameServer::InitGameServer()`、`SendMessage_() 4007`、`BeginAuthSession()`、`7450 -> 7451`，说明 GC 启动包主链路已经基本打通。
  - 这时若 Steam 日志开始持续刷 `Steam_Networking_Sockets_Serialized::PostConnectionStateMsg() // TODO`，且看不到对应的真实连接状态回调消费，应优先排查本地 `SteamNetworkingSocketsSerialized` 的连接状态回调实现，而不是继续优先改 `7038/7041` 的 GC 包序。
  - 在该阶段，`GetCertAsync() // TODO`、`GetSTUNServer() // TODO` 和重复的 `PostConnectionStateMsg() // TODO` 组合，是“客户端已启动本地 server 但没有继续进入英雄选择/游戏中”的高优先级信号。

[gbe_fork 记忆文件归档位置]
- Date: 2026-04-28
- Context: 用户要求后续在 `gbe_fork` 相关工作中统一记录记忆位置
- Instructions:
  - 以后涉及 `gbe_fork` 的用户指令、偏好和项目知识，统一记录到 `gbe_fork/.monkeycode/MEMORY.md`。
  - 不要再把 `gbe_fork` 相关记忆写到工作区根目录的 `.monkeycode/MEMORY.md`。

[Dota2 选人阶段卡 INIT 的高优先级缺口]
- Date: 2026-04-28
- Context: Agent 在分析 `/workspace/gbe_gc_debug.log` 与 `/workspace/console.txt` 时发现
- Category: 代码模式
- Instructions:
  - 当本地服务器已经启动、客户端进入 `DOTA_GAME_UI_DOTA_INGAME`，但游戏状态仍长期停留在 `DOTA_GAMERULES_STATE_INIT` 时，优先检查 server-side `7450 / k_EMsgServerToGCRequestBatchPlayerResources` 是否被回 `7451`。
  - 当前日志中缺失 `7451` 会直接导致 `BatchPlayerResources - Failed to get accounts`，这是比 `8880/8096/7504/8801` 这些非关键超时更高优先级的缺口。
  - 分析 `7041` 时序问题时，要注意日志是否仍显示旧的 `0.12` 外围延时；如果是，说明运行结果还没有包含最新本地时序修正。

[Dota2 7041 启动 26 的 account_id patch 风险]
- Date: 2026-04-28
- Context: Agent 在复查新一轮 `/workspace/gbe_gc_debug.log` 与 `/workspace/console.txt` 时发现
- Category: 代码模式
- Instructions:
  - 当 `7450 -> 7451` 已经恢复、`BatchPlayerResources - Failed to get accounts` 已消失，但进房后仍无英雄可选时，优先检查 `7041` 发出的 4 条 `26 / PracticeLobbyDetailsUpdate` 里 donor `account_id` 是否仍然残留。
  - 如果日志出现 `skipping account_id varint replacement req=7041 resp=26 ... encoded_size=5 expected=4`，说明当前账号的 `account_id` varint 比 donor 模板更长，旧的等长字节替换会失效。
  - 日志里的 `0.117/0.118/0.119/...` 经过 `%.2f` 输出会显示成多个 `0.12`，这不代表仍在运行旧版 `7041` 外围时序。
  - donor 模板里的 `account_id` varint patch 现在应只按 protobuf 语义改写 `field 1`，不要再回退到无字段语义的原始字节扫描替换。
  - 如果 donor 模板本身是“外层包裹消息”或“带 8 字节 direct proto 头的消息”，不能把整包直接当纯 protobuf 做语义重写；应先定位内层消息体，再对消息体中的 `field 1` 做 `account_id` 改写。

[新生成用户 SteamID 的 account_id 范围]
- Date: 2026-04-28
- Context: 用户要求把通用 `generate_account_id()` 全局收敛到 4 字节 varint 范围
- Category: 代码模式
- Instructions:
  - 通用 `generate_account_id()` 应只生成 `1..0x0FFFFFFF` 范围内的值，确保所有基于它生成的新 `account_id` 在 protobuf 中始终不超过 4 字节 varint。
  - 这会同时影响 user、anon user、server、anonserver、lobby 等依赖 `generate_account_id()` 的新 ID 生成路径。

[Dota2 hoststartgame 后段 direct 处理顺序]
- Date: 2026-04-23
- Context: 用户要求继续补齐 `7041` 之后的 host direct 请求时明确指定实现优先级
- Instructions:
  - 先把 `8729 -> 8730` 补进现有 direct replay 表。
  - 再处理 `8727 -> 8728` 和 `8886 -> 8887`。
  - `4007` 继续单独处理，不要混进 direct replay 表，因为它更像系统级 `GCServerHello`。

[Dota2 INIT 卡点的新高优先级握手缺口]
- Date: 2026-04-29
- Context: Agent 在分析本轮 `/workspace/console.log` 与 `/workspace/gbe_gc_debug.log`、并对照旧 `steamapi` 的 gameserver 登录时序时发现
- Category: 代码模式
- Instructions:
  - 当 `7034` 预热已经生效、`7450 -> 7451` 正常、`PR:OnFullyJoinedServer` 也已出现，但仍卡在 `DOTA_GAMERULES_STATE_INIT` 时，优先检查握手阶段是否出现 `Server SteamID in handshake is ... but SteamID from SteamNetworkingSockets is ...`。
  - 当前 `gbe_fork` 的 `Steam_GameServer::GetSteamID()` 受 `logged_in` 门控，而 `logged_in` 原本要等 `RunCallbacks()` 延迟约 `0.1s` 后才置真，这会在本地连接握手窗口里泄露空的 anon gameserver SteamID `72057594037927936`。
  - 旧 `steamapi` 在 `LogOn/LogOnAnonymous` 时就立即置 `logged_in = true`；后续若再遇到同类握手不一致，应优先沿用这一路径排查，而不是先回到 `7034` 或 `Steam_Networking_Sockets_*`。
  - 如果提前置 `logged_in` 后握手里仍然出现空的 anon gameserver SteamID，说明当前引擎取 `GetSteamID()` 的时机仍可能早于或绕过登录态判断；这种情况下应直接让 `Steam_GameServer::GetSteamID()` 始终返回 `settings->get_local_steam_id()`，不要再用 `logged_in` 把它降级成空 anon server id。
  - 如果以上两步都已做且日志仍在 `S2C_CHALLENGE` 之后立刻报 `Server SteamID in handshake is 72057594037927936 ...`，则根因更可能位于底层连接握手包本身或更靠近 `SteamNetworkingSockets` 的身份填充路径，而不是 `7034`、`7450/7451` 或普通 `ISteamGameServer::GetSteamID()` 导出路径。

[Dota2 host startgame 的 direct 7040 过渡窗口]
- Date: 2026-04-30
- Context: Agent 在继续排查 practice lobby `start game` 回主界面问题时发现
- Category: 代码模式
- Instructions:
  - 如果日志显示 `7038` 建房成功后立刻收到一个空的 direct `7040`，并在其后紧跟多条 direct `7041`，则这个 `7040` 更像启动过渡信号而不是真正离房。
  - 在这种窗口里，若 `match_id/server_id/game_start_time` 仍全为 `0`，`7040` 处理可以继续给客户端下发 `25 / CMsgSOCacheUnsubscribed`，但不能立刻清空内部 `GBE_local_lobby`。
  - 否则后续 `7041` 会因为 `no local lobby is active` 被全部丢弃，直接把 host startgame 链路在本地截断。

[Dota2 官方 host startgame 的 7041 后续 SO 节奏]
- Date: 2026-04-30
- Context: Agent 在对照 `/workspace/lobbystartgame.log` 与 `lobbystartgamedota2.zip` 的官方抓包时发现
- Category: 代码模式
- Instructions:
  - 官方 `7041 / PracticeLobbyLaunch` 之后，客户端不会立刻发送 `7035 / AbandonCurrentGame`；如果本地日志在启动包之后立刻出现空 `7035`，应视为异常回退信号。
  - 官方链路会先收到一条较大的 `24 / CacheSubscribed`，其中 `CSODOTALobby` 会先落地 `server_id`、`match_id`、`state=SERVERSETUP`，并带上 `CSODOTAServerLobby.extra_startup_messages[0] = 8869`。
  - 在这条 `24` 之后，官方才继续通过多条 `26 / UpdateMultiple` 把 lobby `state/game_state` 依次推进到 `RUN / WAIT_FOR_PLAYERS_TO_LOAD / HERO_SELECTION / STRATEGY_TIME / PRE_GAME`。
  - 如果本地实现只重放启动 `26`，但没有提供这条带 `server_id/match_id/8869` 的大 `24`，客户端和服务器虽然可能进入 `INGAME` UI，但游戏规则状态容易长期停在 `INIT`。

[Dota2 启动期 24 贴近官方优先于最小构造]
- Date: 2026-04-30
- Context: 用户在上传新一轮测试日志后要求继续调整 host startgame 启动期 `24 / CacheSubscribed`
- Instructions:
  - 启动期额外补发的 lobby `24` 应尽量贴近官方抓包中的大 `24`，不要只停留在最小字段构造。
  - 后续优先通过新日志与官方样本逐字段缩小 `2004/2015/2016/2014` 对象差异，再决定是否继续补更多启动后续消息。

[Dota2 官方 038 donor 的包装层约束]
- Date: 2026-04-30
- Context: Agent 在把 `/workspace/lobbystartgamedota2.zip` 中的 `038_in_5453_k_EMsgClientFromGC.bin` 接入 launch-time `24` 重放时发现
- Category: 代码模式
- Instructions:
  - 官方 `038_in_5453_k_EMsgClientFromGC.bin` 不是裸的 direct `24`，而是一条外层 `5453 / k_EMsgClientFromGC`，真实的 direct `24 / CacheSubscribed` 在外层 protobuf `field 3` 的 payload 里。
  - 后续若继续复用这条 donor 或类似的官方 wrapped 消息，必须先解 outer `5453`，再对 inner direct 消息做 `account_id/steam_id/lobby_id/server_id/match_id` 和 SO object patch，不能把整条 wrapped 消息直接当 direct `24` 交给 SO 改写逻辑。

[Dota2 host startgame 官方抓包的关键 launch 窗口顺序]
- Date: 2026-04-30
- Context: Agent 在复查 `/workspace/lobbystartgamedota2.zip` 与 `/workspace/lobbystartgame.log` 的 launch 窗口时发现
- Category: 代码模式
- Instructions:
  - `033/035/036` 之后，官方会先收到一条很小的 wrapped `5453`（`037`，内层是小 `24`），紧接着再收到大 `24`（`038`）。
  - 大 `24`（`038`）之后，官方紧邻的 direct 交互至少包括：`039 out 8744 -> 042 in 8745`，以及 `040/045 out 7034 -> 043/046 in 26`。
  - 这说明 `038` 不是孤立 donor；它前面有一个小 `24` 过渡，后面紧跟 guild contracts 与 connected players 驱动的状态推进。如果后续仍卡在 `INIT/WAIT_FOR_PLAYERS_TO_LOAD` 之后，可以优先对照这一窗口补齐或重排相邻 direct 消息。

[Dota2 启动期 037 小 24 的接入方式]
- Date: 2026-04-30
- Context: Agent 在把官方 `037_in_5453_k_EMsgClientFromGC.bin` 接入 launch-time `24` 预热窗口时发现
- Category: 代码模式
- Instructions:
  - 官方 `037` 是一条非常小的 wrapped `5453`，适合作为 `server_id` 首次落地后的 `24` 预热包，排在大 `038` 之前。
  - 这类小 donor 不应强制要求存在 `lobby_id` 或 `steam_id fixed64` 的模板占位；应按“若字段存在则 patch”的方式处理可选标识符，再保留其余 payload 原貌。
  - 当前 `gbe_fork` 已将该小 `24` 放在 `queued launch CacheSubscribed after server_id sync ...` 之前发送，后续若继续对齐官方 launch 窗口，应以这一顺序为基线继续比对日志。

[Dota2 官方 038 大 24 的标识符改写约束]
- Date: 2026-04-30
- Context: Agent 在根据新一轮 `gbe_gc_debug.log` 排查 launch-time 大 `24` 未实际发出的问题时发现
- Category: 代码模式
- Instructions:
  - 官方 `038` 大 `24` 在当前 donor 中不能再假设一定存在可供原始字节替换的 `lobby_id` 模板片段；否则 strict `lobby_id` patch 会直接导致整条 launch `24` 构建失败。
  - 这条 donor 的 `lobby_id`、`steam_id fixed64` 等标识符也应按“若字段存在则 patch”的策略处理，而不是要求模板字节必须命中后才允许发送。
  - `032` 后第二个 `24` 应直接重放 wrapped `038` donor 提取出的内层大缓存，并保留 donor 原始对象集；不要再用当前 runtime lobby cache builder 按 `state=2/game_state=10` 重写出一个小很多的替代包。
  - 这条 `038` builder 只需要做可选标识符 patch 和 owner SOID 对齐，不应继续做 runtime state / `2015` 重写；否则即使顺序对了，包体对象集合也会退回非官方的 `918`-byte 级别替代物。
  - 当大 `038` 已经成功下发后，客户端可能会先发一条 `28 / k_ESOMsg_CacheSubscriptionRefresh`，请求体只带当前 lobby 的 `owner_soid(type=3,id=lobby_id)`；如果这一步无人响应，后续官方 `039 out 8744` 往往根本不会出现。
  - 这种情况下应优先回一条最小合法的 `29 / CMsgSOCacheSubscribedUpToDate`，至少带相同的 `owner_soid`，先把客户端从 cache refresh 分支带回后续 launch 主链，再继续核对 `8744/043/046`。
  - 如果日志出现 `Failed replacing lobby_id bytes` 或 `failed building launch CacheSubscribed after server_id sync`，应优先检查是否又回到了 strict 标识符替换路径。

[Dota2 7041 过早推进到 RUN 会诱发 7035]
- Date: 2026-04-30
- Context: Agent 在分析本轮 `gbe_gc_debug.log` 与 `console.log`、确认大 `038` 已成功下发后的新卡点时发现
- Category: 代码模式
- Instructions:
  - 如果 `7041` 阶段在 `server_id` 仍为 `0` 时就连续下发后续三条 launch `26`，把 lobby 状态推进到 `state=2/game_state=1`，客户端很容易在 `4511` 前立刻发送空 `7035 / AbandonCurrentGame`。
  - 当前更接近官方的修正方向是：`7041` 先只发启动第一条 `26` 触发载入，把其余 launch `26` 和外围 follow-up 挪到 `4511` 同步出 `server_id`、并下发 `037/038` 之后再继续发送。
  - 如果新日志里仍看到 `7035` 早于 `4511/大24`，优先检查是否又在 `7041` 初始窗口里把状态推进到了 `RUN`。

[Dota2 4511 后不能让权威 lobby 状态回退到 1/0]
- Date: 2026-04-30
- Context: Agent 在分析“host loading 一直转圈”的新日志时发现
- Category: 代码模式
- Instructions:
  - 当 `4511` 后已经成功下发 `037/038` 和延后的 `7041` stage2/3/4，若服务端权威 `GBE_local_lobby` 或 shared lobby 状态又被后续逻辑恢复成 `state=1/game_state=0`，客户端会停在 `host loading`，只看到 `DOTA_GAMERULES_STATE_INIT`。
  - 这种情况下，问题不再是 `7035` 早退，而是 `4511` 后的权威运行态没有跟着延后的 launch `26` 一起推进到最终状态。
  - 如果日志里出现延后的 `stage=2/3/4` 已排队，但后续 `restored shared lobby reason=4511_lan_server_available` 仍显示 `state=1 game_state=0`，优先检查 shared lobby 是否被旧状态覆盖。

[Dota2 practice lobby 需要 backing generic Steam lobby]
- Date: 2026-04-30
- Context: Agent 在继续排查 host `start game` 长期停在 `DOTA_GAMERULES_STATE_INIT` 时发现
- Category: 依赖关系
- Instructions:
  - 当前 `7038 / PracticeLobbyCreate` 默认只生成 Dota-specific `lobby_id`，这不足以驱动旧 `steamapi` 依赖的 generic `ISteamMatchmaking` lobby 链路。
  - 如果没有同步创建 backing generic Steam lobby，后续 `SetLobbyGameServer(...)` 就没有真实目标可更新，也不会触发期望的 `LobbyGameCreated_t` / `LobbyDataUpdate_t` 联动。
  - 后续排查应优先确认日志里是否已经出现 `GenericLobbyID=` 和 `synced generic lobby gameserver ... generic_lobby_id=...`，再决定是否回到更底层的 networking/serialized 路径。

[Dota2 invisible generic lobby 不会自动进入 settings->lobby]
- Date: 2026-05-01
- Context: Agent 在继续排查“大厅仍显示主机载入中”并对照 `steam_matchmaking.cpp`、`steam_friends.cpp` 与 `steam_game_coordinator.cpp` 时发现
- Category: 代码模式
- Instructions:
  - `Steam_Matchmaking::on_self_enter_leave_lobby(...)` 对 `k_ELobbyTypeInvisible` 会直接 `return`，因此 `CreateLobbyImmediate(k_ELobbyTypeInvisible, 1)` 虽然能创建 backing generic lobby，但默认不会调用 `settings->set_lobby(...)`。
  - `Steam_Friends::GetFriendGamePlayed(...)` 与 `RunCallbacks()` 对外广播的 `m_steamIDLobby` / `friend.lobby_id` 都读取 `settings->get_lobby()`；如果这里只保留旧值或空值，前台大厅与好友视图就可能继续停留在错误的 host loading 状态。
  - 因此 Dota practice lobby 在 create、shared runtime restore、leave generic lobby 这三类生命周期路径上，都需要显式把当前 `generic_lobby_id` 同步到 `settings->lobby`，而不能只维护 `GBE_local_lobby.generic_lobby_id`。

[Dota2 server coordinator 也必须能发布 shared lobby]
- Date: 2026-04-30
- Context: Agent 在继续修复 practice lobby host startgame 卡在 loading、并追到 `4511` 后状态回退问题时发现
- Category: 代码模式
- Instructions:
  - 当前 `dll/steam_game_coordinator.cpp` 里的 `GBE_PublishSharedDotaLobbyState()` 不能禁止 `is_server` 路径写 shared state；否则 server coordinator 在 `4511` 后推进出的 `state/game_state/server_id/match_id` 不会落到 shared lobby。
  - 一旦 shared lobby 仍停留在旧值，后续 `GBE_SendDotaPracticeLobbyDetailsUpdate()` 开头调用 `GBE_RestoreSharedDotaLobbyState()` 时，就会把 server 侧刚推进到的运行态重新拉回旧状态。
  - 如果日志显示 `updated authoritative lobby state after deferred 7041 launch ... state=2 game_state=1` 紧接着又出现 `restored shared lobby ... state=1 game_state=0`，应优先检查是否又恢复了 `GBE_PublishSharedDotaLobbyState()` 对 server 的早退。

[SteamNetworkingSocketsSerialized 不能返回空证书假成功]
- Date: 2026-04-30
- Context: Agent 在分析新一轮 host startgame 日志、确认 GC lobby 状态已推进到 `state=2/game_state=1` 但控制台仍反复报 `Cert request returned invalid cert` 时发现
- Category: 代码模式
- Instructions:
  - `dll/steam_networking_socketsserialized.cpp` 里的 `GetCertAsync()` 不能再返回 `k_EResultOK` 配合空 `SteamNetworkingSocketsCert_t` 负载；这会把问题从“无证书”变成更糟的“无效证书”。
  - 如果当前并没有真实可用的 SteamDatagram 证书，应优先返回明确失败结果，并把 `m_cbCert/m_cbSignature/m_cbPrivKey` 维持为 `0`，不要伪造空成功。
  - `GetNetworkConfigJSON()` 也不应继续返回裸 `0`；至少要返回可解析的最小 JSON，避免引擎把它当成完全缺失的 network config。

[GitHub 构建触发偏好]
- Date: 2026-04-23
- Context: 用户要求后续触发 GitHub 构建时限定目标任务
- Instructions:
  - 以后如果需要触发 GitHub 构建，只触发 `win / build (api_regular, x64, debug)`，不要再主动触发其他 Windows 或 Linux 构建任务。

[Dota2 7041 排查优先级]
- Date: 2026-04-23
- Context: 用户要求后续继续修复 “开始游戏” 路径时严格按 `/workspace/hoststartgame.zip` 对齐
- Instructions:
  - 后续 `7041 / k_EMsgGCPracticeLobbyLaunch` 的修复要严格按照 `hoststartgame.zip` 的消息路径实现，不要混入 dedicated `startgame.zip` 的分支节奏。
  - 当前优先排查并实现的方向依次是：补齐 `029/030_in_5453`、让本地 lobby 状态随延时异步推进、重新审视 `connect` 地址格式是否被客户端接受、再看 `4007/8793/8727/7427/7534` 是否需要关键响应。

[Dota2 7041 后的 26 优先顺序]
- Date: 2026-04-23
- Context: 用户基于抓包纠正 `7041` 后的实际回复顺序
- Instructions:
  - `7041` 之后应先回复连续的 `26 / PracticeLobbyDetailsUpdate`，不要先去补 `8879/8095/8800` 这类低优先级 direct 请求。
  - 如果现有实现把 `766/5501/5575/779/5429` 等外围消息插到后续 `26` 之前，需要优先重排，让四条启动 `26` 先完整发出。

[Dota2 26 会推动界面状态]
- Date: 2026-04-23
- Context: 用户指出 `26 / PracticeLobbyDetailsUpdate` 与 `24` 一样会推动游戏界面变化
- Instructions:
  - 排查 practice lobby 开始游戏问题时，不能只对齐 `26` 的发送顺序，还要核对 `26` 的消息体是否与抓包 donor 足够一致。
  - 如果 `26` 的内容与官方抓包差异过大，即使顺序正确，客户端界面推进也可能失败。

[Dota2 7041 启动包 patch 红线]
- Date: 2026-04-23
- Context: 用户补充说明 practice lobby 开始游戏阶段的 4 个高优先级实现要求
- Instructions:
  - 启动包里的服务器地址或 `connect` 字符串必须做绝对等长替换；如果 donor 是固定长度字符串，本地替换值也必须保持同字节数，不能破坏 protobuf 长度编码。
  - 重放开始游戏相关 SO Update 或 `26` 时，必须确保 lobby `state` 跃迁到官方抓包里代表开始载入的枚举值，不能只替换地址字段。
  - 开始游戏时需要生成新的 `match_id` 和伪造的 `server_id`，并在所有相关响应包里对 donor 的旧值做等长 patch。
  - 处理 `k_EMsgGCPracticeLobbyLaunch` 及其响应时，要继续注意 wrapped/direct 的扩展头与 job 传递规则，并在开始请求后紧跟必要的 SOUpdate/状态更新以推动客户端 UI 进入载入画面。

[Dota2 7041 connect 地址来源]
- Date: 2026-04-23
- Context: 用户要求把 practice lobby 开始游戏里的固定 `127.x.x.x` connect 地址改成本机实际 IP
- Instructions:
  - `7041` 启动包里的 connect 地址优先取本机实际 IPv4，不要再固定写死为 `127.x.x.x`。
  - 即使改为本机实际 IP，替换后的 connect 字符串也必须严格保持和 donor 相同的总字节数，不允许破坏原始 protobuf 长度。

[Dota2 4007 回包策略]
- Date: 2026-04-23
- Context: 用户要求继续排查 server-side GC 黑屏阶段时明确指定 `4007` 的实现方向
- Instructions:
  - `4007 / k_EMsgGCServerHello` 不要先走纯通用零值回包，应优先基于当前 Dota 客户端实际发出的 `4007` 请求体字段构造更贴近 Dota 的 `4005 / ServerWelcome`。

[Dota2 dedicated startgame 样本差异]
- Date: 2026-04-23
- Context: Agent 在对照 `/workspace/startgame.zip` 与 `/workspace/hoststartgame.zip` 分析 dedicated 服务器开局链路时发现
- Category: 代码模式
- Instructions:
  - `/workspace/startgame.zip` 不是 `hoststartgame` 的简单等价替身；它在 `7041` 之后多出一段 dedicated 专用的前置 GC 时序，包含额外的 `5453` 小包、一次 `822 / ClanState`、以及与 host 样本不同顺序的 `779 / GameConnectTokens` 与 `766 / PersonaState`。
  - dedicated 样本里最值得优先参考的是额外的 `003/004/005/008/009/022/076/079/080` 这些 `5453`，以及 `014/017/032` 这组三条 `779`；大量尾部 `151/147/815` 和批量 `766` 更像进入比赛后或界面刷新噪声，不应先作为当前 `7041` 卡点的首要实现目标。
  - host 样本与 dedicated 样本的 `779 / GameConnectTokens` 节奏不同：host 是 `47 -> 25 -> 47`，dedicated 是 `47 -> 47 -> 25`，不能假设 dedicated 开局仍沿用 host 的 token 顺序。

[Dota2 Practice Lobby 7041 启动链路]
- Date: 2026-04-23
- Context: Agent 在执行 Dota2 Practice Lobby “开始游戏”启动链路实现时发现
- Category: 代码模式
- Instructions:
  - `7041 / k_EMsgGCPracticeLobbyLaunch` 需要同时兼容 direct 与 `5452/5453` wrapped 两种入口。
  - 当前高置信度官方启动顺序是连续 4 条 `26 / PracticeLobbyDetailsUpdate`：先 `state=SERVERSETUP + match_id`，再补 `server_id + game_start_time`，再切到 `state=RUN + connect`，最后补 `game_state=1` 且把 `2015` 缩成空对象。
  - 启动 donor 模板里的旧 `connect` 字符串长度固定为 38，做字节替换时必须保持绝对等长；当前可用的本地 loopback 替换串为 `127.000.000.01:27015 127.000.1.1:27015`。
  - 启动相关模板 patch 至少需要处理 `lobby_id`、`account_id`、`steam_id`、`match_id`、`server_id`、`game_start_time` 和 `connect`，其中 `match_id` 与 `game_start_time` 都必须维持 donor varint 的原始编码长度。

[Dota2 7041 网络降级使用严格 loopback connect]
- Date: 2026-04-28
- Context: Agent 在继续处理 `7041 / PracticeLobbyLaunch` 的 SDR 降级时发现
- Category: 代码模式
- Instructions:
  - `7041` 启动阶段现在优先把 `GBE_local_lobby.server_id` 维持为 `0`，让 donor 模板中的 `server_id` 统一被 patch 成 8 字节零值，而不是本地伪造的 server SteamID。
  - 启动阶段的 `connect` 应优先使用严格的 `127.0.0.1:27015` 双 endpoint，并通过尾部空格补齐到 donor 原始总长度，避免再依赖带零填充 octet 的旧 loopback 字符串。

[Dota2 启动后需用真实 GameServer SteamID 回填 server_id]
- Date: 2026-04-28
- Context: Agent 在分析“服务器无法定位 game session”日志并对照 `4511 / LANServerAvailable` 与 `4508 / GameServerInfo` 上行通知时发现
- Category: 代码模式
- Instructions:
  - `7041` 初始阶段保留 `server_id=0` 是允许的，但这只是启动期占位，不应贯穿整个运行态。
  - 当本地 game server 已经登录 Steam 并开始发送 `4511 / k_EMsgGCLANServerAvailable`、`4508 / k_EMsgGCGameServerInfo` 时，应从 `SteamGameServer::GetSteamID()` 取真实 `GameServer SteamID` 回填到当前 lobby 的 `server_id`。

[Dota2 server-side lobby owner 身份边界]
- Date: 2026-04-28
- Context: Agent 在继续修复 practice lobby 启动后 `NETWORK_DISCONNECT_REJECT_NOLOBBY` 时发现
- Category: 代码模式
- Instructions:
  - server-side `steam_game_coordinator` 的 `settings->get_local_steam_id()` 代表的是 GameServer SteamID，不能直接拿它去填 `CSODOTALobby.all_members[0].id`、`leader_id`、`2016` 成员对象或 `7034` connected players 响应里的玩家身份字段。
  - practice lobby 的 owner/player 身份需要在 client-side 建房时单独保存为共享运行态字段（`owner_steam_id`、`owner_account_id`、`owner_name`），供 server-side lobby SO/cache 和 connected-player 响应复用。

[Dota2 lobby member 默认断线态陷阱]
- Date: 2026-04-29
- Context: Agent 在分析“`NOLOBBY` 已消失但游戏仍卡在 `DOTA_GAMERULES_STATE_INIT`”的新日志时发现
- Category: 代码模式
- Instructions:
  - `CSODOTALobbyMember.leaver_status` 的字段号是 `16`，值 `1` 对应 `DOTA_LEAVER_DISCONNECTED`；如果在 `2004 / CSODOTALobby` 的 `all_members` 或 `2016` member 对象里把它写成 `1`，Dota 会继续把玩家视为断线未就位。
  - 直接构造 practice lobby SO 对象时，owner member 的 `leaver_status(16)` 和 `leaver_actions(28)` 应显式写 `0`。
  - 模板重写路径在改写 lobby member 对象时，也应把 `leaver_status(16)` 和 `leaver_actions(28)` 归零，避免 donor 模板残留 `DOTA_LEAVER_DISCONNECTED`。
  - 回填真实 `server_id` 后，还需要再补发一条 server-side direct `26 / PracticeLobbyDetailsUpdate`，把更新后的 `server_id(6)` 同步进 Dota 自己维护的 GC SOCache；只改 `GBE_local_lobby.server_id` 不足以修复 session 绑定。

[Dota2 7041 后若客户端主动发 7035 要优先转查启动回退]
- Date: 2026-04-29
- Context: Agent 在分析“点击开始游戏后直接闪退”的 `STEAM_LOG_3126989041.log` 与 `gbe_gc_debug.log` 时发现
- Category: 代码模式
- Instructions:
  - 如果 `7041` 后 `gbe_gc_debug.log` 只看到 `4 x 26` 和外围 `766/5501/5575/779/5429`，却完全看不到后续的 `4007/4005`、`7034`、`7450/7451`、`4508/4511`，要先判断客户端是否已经在启动早期主动回发了 `7035 / k_EMsgGCAbandonCurrentGame`。
  - 一旦 Steam 日志在 `7041` 后很快出现 `Send msg 7035`、`Client ... SIGNONSTATE_FULL -> SIGNONSTATE_NONE`、随后再次进入 `SteamInternal_GameServer_Init`，这说明当前问题已经从“卡 INIT / leaver_status”前移成“启动序列触发客户端放弃当前对局”。
  - 这种情况下，应优先复查 `7041` 后的 `26 / PracticeLobbyDetailsUpdate` 与外围消息的时序和内容一致性，不要继续把排查重点放在更后面的 server-side GC 链路或 `leaver_status` 上。

[Dota2 启动期 server-side 24 不能回放固定 donor 座位]
- Date: 2026-04-29
- Context: Agent 在分析“只有天辉一号位能开，其他位置点击开始游戏立刻闪退”的新复现差异时发现
- Category: 代码模式
- Instructions:
  - `ServerWelcome` 之后 server-side 额外补发的 lobby `24 / CacheSubscribed`，如果仍从 donor 模板回放成员对象，就可能把 `CSODOTALobbyMember.team/slot` 带回模板里的固定座位。
  - 这种固定 donor 座位与当前 `7047 / PracticeLobbySetTeamSlot` 选中的实际位置不一致时，Dota 可能会在启动早期直接回发 `7035 / AbandonCurrentGame` 并随后崩溃。
  - 启动期这条 server-side `24` 应优先按当前运行态直接构包，显式带上最新的 `owner_team` 和 `owner_slot`，不要继续依赖 donor 模板中的成员座位信息。

[Dota2 Practice Lobby 7041 外围消息节奏]
- Date: 2026-04-23
- Context: Agent 在对照 `/workspace/hoststartgame.zip` 补齐启动外围消息时发现
- Category: 代码模式
- Instructions:
  - `7041` 启动链路不能只发 `4 x 26`，还应补齐样本里的外围 Steam 消息序列，至少覆盖 `766(init) -> 5501 -> 5575 -> 779 -> 766(serversetup) -> 5575 -> 779 -> 766(run) -> 5429 -> 779 -> 766(server run) -> 766(private lobby) -> 766(run)`。
  - `004_in_766` 的 rich presence 是 `#DOTA_RP_INIT`，`024_in_766` 会把 `status` 和 `steam_display` 切到 `#DOTA_RP_PRIVATE_LOBBY`，这些不是现有 `010/018/023/026` 的简单重复包。
  - `022_in_779` 与前两条 `779` 不同，包含一组新的 connect token 数据；如果只重放前两条 `779`，启动时序仍然比官方样本短一段。

[Dota2 host start-game 抓包主链路边界]
- Date: 2026-04-23
- Context: Agent 在对照 `/workspace/hoststartgame.zip`、`/workspace/hoststartgeme11.zip`、`/workspace/steartgamenew.zip` 审查官方开始游戏时序时发现
- Category: 代码模式
- Instructions:
  - 这 3 份 host start-game 抓包的核心主链路只稳定出现 `7041 -> 多条 26`，并穿插 `766`、`5501`、`5575`、`779`、`5429`，以及邻近的 `7197 -> 7198`、`8673 -> 8674`。
  - 这 3 份抓包里没有看到 `4007`、`7427`、`7534`、`8793`、`8886`、`8727`、`7387`、`7038`、`7009`、`7055`、`24/25/29`，因此这些消息不能被当作官方 start-game 主链路的必经步骤。
  - `8729` 只在其中 2 份抓包里作为请求出现，但都没有看到明确回包，不能继续把它硬当成已证实的严格请求/回包配对。
  - 官方 `7041` 后的节奏更接近：先首条 `26`，再 `766/5501/5575/779/766/5575/779`，然后才进入后续 `26`，不能把 4 条 `26` 过早连续排到外围消息前面。

[外部仓库对 Dota2 direct 响应的补充确认]
- Date: 2026-04-23
- Context: Agent 在阅读 `/workspace/go-dota2` 与 `/workspace/GameTracking-Dota2/Protobufs` 时发现
- Category: 代码模式
- Instructions:
  - `go-dota2` 的生成客户端把 `7197 -> GCMatchmakingStatsResponse`、`7427 -> GCNotificationsResponse`、`7534 -> ClientToGCGetProfileCardResponse`、`8673 -> ClientToGCRequestGuildDataResponse`、`8793 -> ClientToGCGetCurrentPrivateCoachingSessionResponse` 都视为标准 request-response。
  - `7534 / k_EMsgClientToGCGetProfileCardResponse` 的响应体在 `go-dota2` 中直接映射为 `CMsgDOTAProfileCard`，因此最小 `7535` 走 profile card 方向是合理的。
  - `7428` 的结构是 `CMsgGCNotificationsResponse { update = 1 }`；`8794` 的结构是 `CMsgClientToGCGetCurrentPrivateCoachingSessionResponse { result = 1, current_session = 2 }`；`4524` 的结构是 `CMsgGCToClientAggregateMetricsBackoff { upload_rate_modifier = 1 }`。
  - `GameTracking-Dota2/Protobufs` 与 `go-dota2` 都只明确提供了 `4007 = k_EMsgGCServerHello` 的消息号；Dota2 专属 proto 中没有可直接复用的 `CMsgServerHello` 结构，因此 `4007` 仍不能按“已知结构”安全构包。

[Dota2 hoststartgame 后段 direct 映射]
- Date: 2026-04-23
- Context: Agent 在继续对照 `/workspace/hoststartgame.zip` 修复 practice lobby 启动尾段时发现
- Category: 代码模式
- Instructions:
  - `027/030` 这一组在 host 样本里可以明确对应 `7197 -> 7198`，其中 `030_in_5453` 的 outer body 不是直接以内层消息开头，前面还带有 wrapped direct 的 field1/field2，真正的内层 direct replay 模板应从 `1e 1c 00 80` 开始截取。
  - `7041` 的本地 lobby `state/game_state` 不能在排队阶段提前写终态；应把目标状态作为队列元数据附着到 `GC_Message` 上，并在 `push_incoming_now` 或 `RunCallbacks` 真正把消息转入 `incoming_messages` 时再应用。

[字段映射参考顺序]
- Date: 2026-04-21
- Context: 用户要求后续做 Dota2 GC / Lobby 字段对应时优先查工作区内的 SteamKit 定义
- Instructions:
  - 进行消息字段语义对应时，先查看 `/workspace/SteamKit` 里的生成定义，不要先靠抓包现象自行猜字段含义。
  - 只有在 SteamKit 缺失定义或无法解释时，才结合抓包、日志和样本做补充推断。

[SteamKit 参考来源]
- Date: 2026-04-21
- Context: 用户要求后续分析 Dota2 GC / lobby 相关协议时优先利用工作区内的 SteamKit 仓库
- Instructions:
  - 工作区内存在可直接读取的 SteamKit 仓库，可用于查询 Dota2 GC 消息号、protobuf 字段定义和 lobby 相关结构。
  - 相关任务优先先检查 `/workspace/SteamKit` 下的生成代码，再结合抓包和日志做字段映射。
  - 当我对 GC 消息结构、SO Cache 对象或 lobby/start-game 字段没有把握时，必须先系统性阅读 `/workspace/SteamKit` 和 `/workspace/go-dota2` 的相关定义，再动手改 `gbe_fork`。
  - `hoststartgame.zip` 是官方抓包数据，后续分析时应优先按官方样本对齐。
  - 读取官方抓包时，可以参考 `/workspace/SteamKit/Resources/NetHookAnalyzer2` 的源码逻辑解析消息，而不是只凭肉眼或十六进制片段猜测。

[Dota2 GC 登录重放约束]
- Date: 2026-04-21
- Context: 用户要求在 /workspace/gbe_fork 接入 Dota2 登录阶段 GC 抓包回放
- Instructions:
  - 不要引入新的 protobuf 依赖，也不要编译新的 .proto。
  - 只能使用抓包得到的硬编码 uint8_t/uint8 十六进制数组重放。
  - 新增常量、枚举、辅助函数命名必须带 GBE_ 前缀。
  - 不要使用匿名命名空间。
  - 所有手工编辑必须使用 apply_patch。
  - 不要修改 workflow，也不要提交 git。
  - 仅修改与当前任务直接相关的代码，保持最小变更。

[Linux gmake 编译前置依赖]
- Date: 2026-04-21
- Context: Agent 在执行 Dota2 GC 登录回放改动的编译验证时发现
- Category: 构建方法
- Instructions:
  - `build/project/gmake2/linux/api_regular` 下直接编译 `steam_game_coordinator.cpp` 目前会先因缺失 `net.pb.h` 失败。
  - 当前工作区未生成 `net.pb.h`，因此在补齐该生成依赖前，无法用该目标对单文件改动做真实编译校验。

[Dota2 登录后半段 direct GC 配对规律]
- Date: 2026-04-21
- Context: Agent 在分析 `/workspace/login.zip` 的 136-181 段 5452/5453 抓包时发现
- Category: 代码模式
- Instructions:
  - `8137 -> 8136`、`8673 -> 8674`、`8676 -> 8677`、`7387 -> 7388`、`8078 -> 8079`、`8853 -> 8854`、`9023 -> 9024` 都能用请求 `source_job_id` 与回包 `target_job_id` 精确配对，属于高置信度映射。
  - `7197 -> 8678` 在 `login.zip` 里只有时间相邻证据，没有 job id 对应，且消息语义不自然，应视为低置信度临时猜测，不能当成已证实映射。
  - `login.zip` 的这一段里没有看到 `8879`、`8793`、`7427` 的 direct GC 回包；另有 `8729` 请求但未见对应回包，因此缺失请求不能继续靠“最近邻”顺序硬配。

[Dota2 guild member 抓包补充]
- Date: 2026-04-21
- Context: Agent 在分析 `/workspace/member.zip` 的 `8676/8677/8678` 相关抓包时发现
- Category: 代码模式
- Instructions:
  - `176_in_5453_k_EMsgClientFromGC.bin` 的内层 payload 与现有 `8677` 模板一致，进一步确认 `8676 -> 8677` 是稳定 direct 配对。
  - `139_in_5453_k_EMsgClientFromGC.bin` 的内层 payload 与现有 10 字节 `8678` 模板一致，但该消息没有可用于配对请求的 job 语义，更像 guild membership 的主动更新。
  - 优先把 `8678` 作为 `8676` 之后的附加更新重放，不要继续把它当成 `7197` 的可靠 direct 响应。

[Dota2 自定义大厅实现约束]
- Date: 2026-04-21
- Context: 用户要求在 `/workspace/gbe_fork` 实现 Practice Lobby 创建链路
- Instructions:
  - 拦截 `7038 / k_EMsgGCPracticeLobbyCreate` 后，建房首轮应按当前官方样本发送 `24 / CacheSubscribed -> 7055`。
  - `7038` 建房首轮不要主动补发 `26`。
  - `7055` 必须使用 9 字节扩展头，并把请求的 `SourceJobID` 填到扩展头的 `field11`。
  - 需要在 C++ 中随机生成新的 `uint64_t` LobbyID，并把抓包模板中的旧 LobbyID 全部等长替换成新值。
  - 需要把抓包模板中的旧 SteamID 替换为本地玩家真实 ID，确保客户端把本地玩家识别为房主。
  - `AccountID` 需要按消息路径分别处理：能等长 raw 替换就替换；welcome 的 account-bound cache object 改走语义 patch；其余长度不匹配时记录并跳过，不要中断 GC 会话。
  - 需要在模拟器内存中持久化当前本地 lobby 状态，并为每一发建房相关回包打印调试日志。

[Dota2 Practice Lobby 建房抓包规律]
- Date: 2026-04-21
- Context: Agent 在分析 `/workspace/1776260078-create-lobby.zip` 的首轮 `7038` 建房成功抓包时发现
- Category: 代码模式
- Instructions:
  - `7038 / k_EMsgGCPracticeLobbyCreate` 是走外层 `5452/5453` 包裹的 wrapped direct 流程，不是当前登录后常见的裸 direct 请求。
  - 早期样本里看到的 `6146` / SO 更新包不能再直接当作当前首轮顺序依据；当前最终应以 `firstcreateloby.zip` 确认过的 `24 -> 7055` 为准。
  - `7038` 请求的 9 字节 inner header 使用 field10 固定 64 位 job 值，官方 `7055` 响应改为 field11 并复用同一个 job 值。
  - 建房首轮缓存模板里包含旧 LobbyID 的 8 字节 varint 以及旧房主 SteamID 的 fixed64，需要统一替换成本地值。

[Dota2 高位 AccountID 与旧抓包模板兼容性]
- Date: 2026-04-21
- Context: Agent 在分析 `STEAM_LOG_3127950399.zip` 与 `gbe_gc_debug.log` 的 GC 初始化失败问题时发现
- Category: 代码模式
- Instructions:
  - 当前 Dota2 `ClientWelcome` 和部分 direct replay 模板里的旧 `account_id` 使用 4 字节 protobuf varint。
  - 当本地 `GetAccountID()` 编码后超过 4 字节时，不能继续对这些模板做等长替换，否则会在 GC welcome 阶段直接构包失败。
  - 对这类通用模板应改为“能等长替换就替换，长度不匹配时记录日志并跳过”，不要因此中断整个 GC 会话。
  - Lobby 的 `SteamID`、`LobbyID` 和 fixed32/fixed64 替换仍需保持严格等长，不应放宽。

[Dota2 登录阶段通用 24 模板对象组成]
- Date: 2026-04-22
- Context: Agent 在排查“首次登录后掉落物品/物品状态异常”时解析 `GBE_kDotaCacheSubscribedTemplate` 发现
- Category: 代码模式
- Instructions:
  - 登录阶段通用 `24 / CacheSubscribed` 模板当前主要包含三类 SO 对象：`type 1 = CSOEconItem`、`type 7 = CSOEconGameAccountClient`、`type 2010 = CSODOTAPlayerChallenge`。
  - 其中 `type 1 / CSOEconItem` 是 donor 账号库存快照，最可能导致首次登录时把抓包模板账号的物品状态带到当前账号。
  - 当前登录期 `24` 路径本质上还是整包模板重放加 ID 替换，没有像 Practice Lobby `2004` 那样按字段重写 object_data。
  - 如果后续做最小修复，优先处理登录期 `24` 里的 `type 1 / CSOEconItem`，其次再考虑 `type 7 / CSOEconGameAccountClient`；`type 2010` 主要影响挑战/活动进度，不是物品主因。
  - Dota2 登录路径当前不会自动补发一份本地真实 `type 1` 库存，所以最小修复不应直接删掉模板里的 `type 1` bucket，而应在发送登录 `24` 前把该 bucket 的 object_data 重写为 `items.json` 中的本地真实物品列表。

[Dota2 Practice Lobby 入口封装兼容性]
- Date: 2026-04-22
- Context: Agent 在分析 `STEAM_LOG_3783392324.zip` 与 `gbe_gc_debug.log` 的建房失败问题时发现
- Category: 代码模式
- Instructions:
  - `7038 / k_EMsgGCPracticeLobbyCreate` 不能假设只走 `5452/5453` wrapped 路径，真实运行里也会直接以 direct GC 消息发出。
  - 建房回包逻辑需要同时兼容 direct 与 wrapped 两种入口，两者都应复用同一套 `24 -> 7055` 首轮发送顺序与 LobbyID 替换逻辑。

[Dota2 Practice Lobby 设置同步抓包规律]
- Date: 2026-04-21
- Context: Agent 在分析 `/workspace/lobbysettings.zip` 的大厅设置修改抓包时发现
- Category: 代码模式
- Instructions:
  - `/workspace/lobbysettings.zip` 最开始的几条抓包包含点击 slot / 交换位置的样本，不只是房间设置修改样本。
  - 大厅设置修改主请求是 `7046 / k_EMsgGCPracticeLobbySetDetails`，而不是 `7047`。
  - 点击 slot / 交换位置对应 `7047 / k_EMsgGCPracticeLobbySetTeamSlot`，按 SteamKit 定义请求字段为：`field 1 = team`、`field 2 = slot`、`field 3 = bot_difficulty`。
  - `7047` 官方样本会先回一个 `26` 大包，再回一个 `7055` 小 ack；`26` 的 `2004.field120` 内嵌 `CSODOTALobbyMember`，其中 `field 3 = team`、`field 7 = slot`、`field 16 = leaver_status`。
  - `7046` 需要同时兼容 direct 与 `5452/5453` wrapped 两种入口，客户端修改设置后期望收到异步 `26` 更新，而不是 `7055`。
  - `7046` 请求里当前高置信度动态字段为：`field 1 = LobbyID`、`field 2 = room_name`、`field 4 = server_region`、`field 5 = game_mode`、`field 9 = bot_difficulty_radiant`、`field 15 = pass_key`、`field 43 = bot_difficulty_dire`、`field 44 = bot_radiant`、`field 45 = bot_dire`。
  - `26` 的 inner payload 使用 direct proto 外壳且 protobuf 扩展头长度为 0，主体由多段 `field 2` SO/update message 组成，其中 `field1=2004` 的大块承载完整 lobby details。
  - 结合 SteamKit `CSODOTALobby` 定义，当前高置信度响应映射为：`request.field5 -> response.field3`、`request.field4 -> response.field21`、`request.field9 -> response.field36`、`request.field15 -> response.field39`、`request.field43 -> response.field93`、`request.field44 -> response.field94`、`request.field45 -> response.field95`；`response.field128` 是 `lobby_creation_time`，官方样本固定为 `1776809986`。

[Dota2 Practice Lobby 成员名来源]
- Date: 2026-04-22
- Context: Agent 在修复大厅房主名称仍显示抓包用户名的问题时发现
- Category: 代码模式
- Instructions:
  - `26` 的 `field1=2014` update 承载 `CSODOTAStaticLobbyMember` 风格的成员静态信息，其中 `field1` 名字会直接影响大厅里显示的房主名称。
  - 构造本地 practice lobby 更新时，`2014` 里的成员名字应使用 `settings->get_local_name()`，不能继续复用抓包模板中的固定用户名。

[Dota2 Practice Lobby 服务器设置约束]
- Date: 2026-04-22
- Context: 用户说明服务器地址选项当前固定为本地房间，因此抓包中不会体现实际 region 变化
- Instructions:
  - 当前 practice lobby 的服务器设置应固定为本地房间参数，不需要继续尝试从最新 `serverrigon.zip` 样本中推导可变 region 值。
  - 后续处理服务器地址相关逻辑时，可以直接按本地房间固定参数实现。

[Dota2 Practice Lobby 本地房间字段映射]
- Date: 2026-04-22
- Context: Agent 在对照 `/workspace/SteamKit` 与 `gbe_gc_debug.log` 排查“本地房间仍显示 auto region”时发现
- Category: 代码模式
- Instructions:
  - `CMsgPracticeLobbySetDetails` 中，`field25 = lan`，`field48 = lan_host_ping_location`。
  - `CSODOTALobby` 中，`field57 = lan`，`field109 = lan_host_ping_location`。
  - Practice Lobby 的“本地房间”显示不能只依赖 `server_region`，还需要同步 `lan` 与 `lan_host_ping_location` 到 `24/26` 的 `2004` 对象。

[Dota2 Practice Lobby 7046 设置样本补充]
- Date: 2026-04-22
- Context: 用户补充最新 `gbe_gc_debug.log` 中第二次 `7046` 的界面操作含义
- Category: 代码模式
- Instructions:
  - 第二次 `7046` 中 `body_prefix` 从 `20 00` 变为 `20 0c`，对应的是用户切换了游戏模式，不是地区设置。
  - 因此当前把 `field5` 解析为 `game_mode` 的判断与用户实际操作一致，不能再把这条变化误判为 `server_region`。

[Dota2 Practice Lobby 首屏机器人显示问题]
- Date: 2026-04-22
- Context: 用户补充建房时未选机器人，但进入房间首屏显示机器人，点击 slot 后恢复正常
- Category: 代码模式
- Instructions:
  - 如果建房首屏显示了错误的机器人状态，而点击 slot 触发 `7047 -> 26` 后恢复，优先怀疑首轮 `24` 仍残留抓包模板中的 bot 相关字段，而不是本地 `26` 构造逻辑。
  - 排查重点放在 `24 / CacheSubscribed` 的 `2004` 对象是否已经把 bot 相关字段按本地状态重写，而不是只修 `26`。

[Dota2 Practice Lobby 新增字段映射]
- Date: 2026-04-22
- Context: Agent 在分析 `lobbyvsiable.zip`、`cheat.zip`、`bot.zip` 等最小操作抓包时确认
- Category: 代码模式
- Instructions:
  - `7046` 请求里还需处理的高置信度字段包括：`field10 = allow_cheats`、`field11 = fill_with_bots`、`field13 = allow_spectating`、`field33 = visibility`。
  - `26` 的 `2004` 响应对应映射为：`field13 = allow_cheats`、`field14 = fill_with_bots`、`field31 = allow_spectating`、`field75 = visibility`。
  - `watcher.zip` 实际是玩家池样本，不是观战样本；已确认 `7047` 里 `team=4` 对应 `PLAYER_POOL`。

[Dota2 Practice Lobby 广播频道抓包识别]
- Date: 2026-04-22
- Context: Agent 在分析 `/workspace/spector.zip` 时确认
- Category: 代码模式
- Instructions:
  - `/workspace/spector.zip` 不是 `7047` 的 `team=3 / SPECTATOR` 观战位样本，而是 Practice Lobby 广播/解说频道链路样本。
  - 该样本中已确认的关键消息包括：`7149 = k_EMsgGCPracticeLobbyJoinBroadcastChannel`、`7367 = k_EMsgGCLobbyUpdateBroadcastChannelInfo`、`8054 = k_EMsgGCPracticeLobbyCloseBroadcastChannel`。
  - 样本里出现的 `7197 = k_EMsgGCMatchmakingStatsRequest` 与观战位切换无关，不应把它当成 spectator 相关请求。
  - 在拿到真正的最小观战抓包前，不要根据 `spector.zip` 推断 `team=3 / SPECTATOR` 的实现细节。

[Dota2 Practice Lobby 广播频道字段映射]
- Date: 2026-04-22
- Context: Agent 在对照 `/workspace/SteamKit` 与 `/workspace/spector.zip` 的 `26/2004` 更新时确认
- Category: 代码模式
- Instructions:
  - `CSODOTALobby.field58` 是 `broadcast_channel_info`，元素类型是 `CLobbyBroadcastChannelInfo`。
  - `CLobbyBroadcastChannelInfo` 的字段映射为：`field1 = channel_id`、`field2 = country_code`、`field3 = description`、`field4 = language_code`。
  - `7149 / JoinBroadcastChannel` 的请求字段是：`field1 = channel`、`field2 = preferred_description`、`field3 = preferred_country_code`、`field4 = preferred_language_code`。
  - `7367 / LobbyUpdateBroadcastChannelInfo` 的请求字段是：`field1 = channel_id`、`field2 = country_code`、`field3 = description`、`field4 = language_code`。
  - 当前样本里 `7149` 会回 `26` 更新再回 `7055` ack，而 `7367` 和 `8054` 只看到 `26` 更新，没有看到额外 `7055` ack。

[Dota2 Practice Lobby 建房首屏同步]
- Date: 2026-04-22
- Context: Agent 在排查首次建房后房主名、房间名和服务器地区首屏显示异常时确认
- Category: 代码模式
- Instructions:
  - `7038 / CMsgPracticeLobbyCreate` 的实际房间设置在 `field7 = lobby_details`，其类型就是 `CMsgPracticeLobbySetDetails`，可直接复用 `7046` 的字段解析逻辑。
  - 建房首屏的关键同步应优先通过首轮 `24 + 7055` 完成，不要再把“首轮尽快补发 `26`”当成默认策略。

[Dota2 Practice Lobby 首屏落位与 CacheSubscribed]
- Date: 2026-04-22
- Context: Agent 在排查“建房后房主不立即出现在天辉第一个位置”时发现
- Category: 代码模式
- Instructions:
  - 即使最终首轮顺序改回 `24 -> 7055`，如果 `24 / CacheSubscribed` 仍复用错误的 lobby 模板状态，客户端首屏仍可能显示错误的成员落位。
  - 当前已验证更稳的做法是：建房首轮 `24` 继续走模板 patch，并把首屏所需的关键 lobby/member 状态 patch 到模板里，而不是默认要求它与后续 `26` 使用同一份本地重构 object_data。

[Dota2 Practice Lobby 建房专用 24 的空扩展头要求]
- Date: 2026-04-22
- Context: Agent 在继续排查“点击建房没有反应”并对照 `gbe_gc_debug.log` 与旧 `24` 模板前缀时发现
- Category: 代码模式
- Instructions:
  - Practice Lobby 建房专用的 `24 / CacheSubscribed` 外层 `ProtoBufMsgHeader_t` 必须保持空扩展头，即 `m_EMsgFlagged = 24 | protobuf_mask` 且 `m_cubProtoBufExtHdr = 0`。
  - 不要为这条建房专用 `24` 额外追加 `CMsgProtoBufHeader`；此前把 `m_cubProtoBufExtHdr` 改成 `14` 并附带 `client_steam_id / session_id / app_id` 后，客户端会出现“点击建房没有反应”。

[Dota2 Practice Lobby 建房 24 当前回退到模板 patch]
- Date: 2026-04-22
- Context: 用户要求先恢复建房稳定性，不再继续修首屏落位时确认
- Category: 代码模式
- Instructions:
  - `7038` 建房阶段的 `24 / CacheSubscribed` 当前优先使用 `GBE_kDotaPracticeLobbyCacheSubscribedTemplate` + `GBE_PatchDotaLobbyTemplateIdentifiers()` 的旧模板 patch 路径。
  - 在重新验证出稳定收益前，不再让建房专用 `24` 使用本地重构的 `2004/2014/2015/2016` object_data 方案。

[firstcreateloby 抓包确认建房首屏落位来自首轮缓存]
- Date: 2026-04-22
- Context: Agent 在分析 `/workspace/firstcreateloby.zip` 并对照建房首轮 wrapped GC 消息时发现
- Category: 代码模式
- Instructions:
  - `firstcreateloby.zip` 中建房首轮主链路是 `003_out_5452(7038) -> 004_in_5453(24) -> 005_in_5453(7055)`，首轮没有 `26`。
  - 建房完成后紧接着出现的是 `006_out_5452(7009)` 和 `008_out_5452(8673)`，在此之前没有 `7047` 换位请求，因此房主首屏落位不可能来自一次额外的手动换位。
  - 如果该抓包对应的客户端界面已显示房主在天辉 1 号位，则该初始落位信息必须已经包含在首轮 `24 / CacheSubscribed` 所携带的 lobby/member 快照里。

[firstcreateloby 的真实 24 对象结构]
- Date: 2026-04-22
- Context: Agent 直接解析 `/workspace/firstcreateloby.zip` 中 `004_in_5453_k_EMsgClientFromGC.bin` 的 inner `24 / CacheSubscribed` 时发现
- Category: 代码模式
- Instructions:
  - 该官方建房首轮 `24` 的 inner `CMsgSOCacheSubscribed` 实际对象顺序是：`2004(CSODOTALobby)`、`2013(CSODOTALobby)`、`2014(CSODOTAStaticLobby)`、`2015(CSODOTAServerLobby)`、`2016(CSODOTAServerStaticLobby)`。
  - 其中 `2004` 已明确包含房主成员条目：`field120` 内成员 `id=<房主SteamID>`、`field3 team=0`、`field7 slot=1`、`field16 leaver_status=1`；同时 `field121=0`、`field128=<创建时间>`。
  - `2014` 在该样本里包含一个成员静态条目，只有名字 `Svenmax` 和 `field2=0`。
  - `2015` 在该样本里只有一个空成员条目（`field1` 的空 bytes）。
  - `2016` 在该样本里包含一个成员静态条目，首字段是房主 `steam_id`，并带一组附加服务端成员字段。

[建房首轮不要主动发 26]
- Date: 2026-04-22
- Context: Agent 对照 `firstcreateloby.zip` 官方建房首轮链路与当前首屏表现后确认
- Category: 代码模式
- Instructions:
  - `7038` 建房首轮应优先只发送 `24 -> 7055`，不要主动跟一条 `26`。
  - 当前本地构造的 `26` 很可能会在建房首轮覆盖模板 `24` 中更接近官方的初始成员状态，导致首屏仍需手点 slot 才刷新。

[建房模板 24 的房间名/服务器区域/玩家名需要二次 patch]
- Date: 2026-04-22
- Context: 用户反馈首屏房主位置已正确，但房主名和服务器区域仍显示抓包样本值时确认
- Category: 代码模式
- Instructions:
  - 建房首轮 `24` 当前仍使用官方模板重放，但必须在 `LobbyID/SteamID` 之外继续 patch `2004.field16(room_name)`、`2004.field21(server_region)`、`2014.member[0].field1(player_name)`。
  - 这些值应直接来自当前进程内的建房状态：`GBE_local_lobby.room_name`、`GBE_local_lobby.server_region`、`settings->get_local_name()`；不需要也不应该从模拟器系统文件读取。

[建房模板 24 的 2004 需要同步更多大厅状态]
- Date: 2026-04-22
- Context: Agent 在排查“建房首屏误显示机器人，点击 slot 后被 26 修正”时发现
- Category: 代码模式
- Instructions:
  - 建房首轮 `24` 如果只 patch 房间名、地区和玩家名，`2004` 中其余沿用抓包模板的字段仍可能污染首屏显示。
  - 模板 `24` 的 `2004` 至少还要同步本地 `game_mode`、`allow_cheats`、`fill_with_bots`、`allow_spectating`、`visibility`、`pass_key`、`bot_difficulty_radiant`、`bot_difficulty_dire`、`bot_radiant`、`bot_dire`、`lan`、`lan_host_ping_location`，避免首屏状态与后续 `26` 不一致。

[7046 改设置时必须保留房主默认落位]
- Date: 2026-04-22
- Context: Agent 在分析 `gbe_gc_debug.log` 中“建房首屏房主正确，但首次 7046 后房主消失，点击 slot 后才出现”的问题时发现
- Category: 代码模式
- Instructions:
  - 建房首屏房主之所以能直接显示在天辉 1 号位，当前主要依赖首轮模板 `24` 自带的成员状态；但后续 `7046` 会改用 `GBE_local_lobby` 的本地状态构造 `26`。
  - 因此建房初始化 `GBE_local_lobby` 时必须把房主默认状态设为 `owner_team=0`、`owner_slot=1`，否则首次 `7046` 发出的 `26` 会把房主覆盖成 `slot=0`，导致客户端里房主暂时消失。

[Dota2 ClientWelcome 的 account_id 应做语义 patch]
- Date: 2026-04-22
- Context: Agent 在继续修复高位 `AccountID` 与旧 welcome 模板兼容问题时发现
- Category: 代码模式
- Instructions:
  - `ClientWelcome` 模板里旧 `account_id` 的高置信度目标位于 `CMsgClientWelcome.outofdate_subscribed_caches` 中的 `type 2002 = CSODOTAGameAccountClient` 和 `type 2012 = CSODOTAGameAccountPlus`。
  - 这两个对象的 `object_data` 都应重写 `field1 = account_id`，而不是继续依赖旧模板里的 4 字节 varint 等长 raw 替换。
  - 因此 welcome 路径在高位 `AccountID` 场景下，应保留 `version` 与 `steam_id` 的等长替换，但把 `account_id` 改为对 `2002/2012` 做语义级 patch。
  - 当前工程实现上不要直接依赖 Dota 专用 `CMsgClientWelcome` 生成类去访问 `outofdate_subscribed_caches`；更稳的做法是手工遍历 welcome 外层 protobuf 的 `field 3`，再对每个 cache payload 用 `CMsgSOCacheSubscribed` 做局部解析和重写。

[Dota2 lobby SO Cache 的双维度 ID patch]
- Date: 2026-04-28
- Context: 用户要求后续处理大厅 SO Cache 时不要只盯单一 `account_id` 路径
- Instructions:
  - 大厅 SO Cache 里不仅要处理 `members` 列表中的 32 位 `account_id`，还要处理 `leader_id` 这类 64 位 ID 特征码。
  - patch donor 模板时必须同时扫荡并安全替换这两个维度，不能只覆盖 `field1 account_id` 而遗漏 lobby owner / leader 的 `steam_id` 语义字段。
  - 对 protobuf 对象做语义 patch 时，应优先按字段语义重写 `leader_id` 与 `members.account_id`，避免回退到无字段语义的整包盲扫替换。

[Dota2 高位 AccountID 跳过路径的处理策略]
- Date: 2026-04-22
- Context: 用户要求后续不要盲目把所有长度不匹配的 `account_id` 跳过路径都升级成语义 patch
- Instructions:
  - 对高位 `AccountID` 的旧模板兼容问题，优先保留“长度不匹配时记录并跳过”的降级策略，不要默认把所有跳过路径都升级为语义 patch。
  - 跳过日志需要保留，并补充足够的消息上下文，至少应包含模板名或消息作用域；如有条件，再带上对应 `emsg`、selector、body size 等定位信息。
  - 只有当某条跳过路径已经对应到明确的功能异常时，才把该路径升级成有针对性的语义 patch。

[Dota2 Practice Lobby 离房与撤房链路]
- Date: 2026-04-23
- Context: Agent 在分析 `/workspace/create-leave.zip`、`/workspace/create-destroy.zip` 并补齐 Practice Lobby teardown 消息时发现
- Category: 代码模式
- Instructions:
  - 建房成功后的房间聊天链路是 `7009 / JoinChatChannel -> 7010 / JoinChatChannelResponse`，其中聊天频道名形如 `Lobby_<lobbyid>`，`channel_type=3`，并且 `channel_id` 需要与 `lobby_id` 分开维护。
  - 主动离开房间的链路是 `7040 / PracticeLobbyLeave -> 25 / CMsgSOCacheUnsubscribed -> 7272 / LeaveChatChannel -> 7014 / OtherLeftChannel`。
  - 主动撤销房间的链路是 `8246 / DestroyLobbyRequest -> 25 / CMsgSOCacheUnsubscribed -> 8247 / DestroyLobbyResponse -> 7272 / LeaveChatChannel -> 7014 / OtherLeftChannel`。
  - Dota 路径里的 `25 / CMsgSOCacheUnsubscribed` 需要使用 `owner_soid(field2)`，其中 `type=3`、`id=lobby_id`，不是旧的 `owner(field1)` 结构。
  - leave 或 destroy 处理完 lobby 后，不要同步清掉 chat channel 状态；要保留到后续 `7272` 处理完成后再清理。

[Dota2 7034 的 team 语义与本地 owner_team 对齐]
- Date: 2026-04-28
- Context: Agent 在继续实现并细化 `7034 / k_EMsgGCConnectedPlayers` 最小回包时发现
- Category: 代码模式
- Instructions:
  - 当前 `GBE_local_lobby.owner_team` 的取值已经与 Dota proto `DOTA_GC_TEAM` 对齐：`0 = GOOD_GUYS`，`1 = BAD_GUYS`。
  - 因此构造 `CMsgConnectedPlayers.PlayerDraft.team` 时可以直接复用 `owner_team`，只需在异常值场景下兜底回 `0`，不需要再额外做 UI 编号到 proto 枚举的二次映射。
  - 当客户端已经进入 `DOTA_GAME_UI_DOTA_INGAME`，但 server 侧日志仍出现“Need to tell GC player is no longer connected”并发送带 `disconnected_players` 草稿的 `7034` 时，回给 server 的 `7034` 不能只带 `steam_id` 和 `player_draft`；至少还要在 `connected_players[0]` 中补齐 `leaver_state.lobby_state` 和 `leaver_state.game_state`，让 GC 返回的玩家连接态与当前 lobby 运行态一致。
  - 后续继续排查 `7034` 时，优先打开字段级摘要日志，直接比对 request/response 里的 `connected_players`、`disconnected_players`、`send_reason`、`player_draft` 和 `leaver_state`，不要再只凭消息号和长度猜字段缺口。
  - 当 server 发出的 `7034` request 真实形态是 `disconnected_players=1`、`send_reason=GAME_STATE(2)`、并自带 `building_state` 等比赛统计字段时，response 应优先镜像这些外围字段，只最小化把玩家连接态修正为 `connected_players=1`；不要再把 `send_reason`、`building_state` 等字段硬编码成另一个形态。
  - 如果 `7034` request 已经显式带了 `disconnected_players[0].leaver_state` 草稿，response 不要再把 `disconnected_players` 整段直接清零；至少要把该玩家的 `disconnected_players[0].leaver_state` 一并回给 server，避免 server 继续停留在“没有 leaver state 可上报”的状态。
  - `7034.PlayerDraft.team_slot` 目前高置信度更接近队内 0-based 槽位，而不是 lobby UI 的原始 slot 编号；例如 lobby `slot=3` 时，本地服日志会显示 `input slot 2`。

[Dota2 game_session_manifest 的排查边界]
- Date: 2026-04-29
- Context: Agent 在分析“server 卡在 `ss_waitingforgamesessionmanifest`、客户端停在 `DOTA_GAMERULES_STATE_INIT`”的新日志并对照 Source2 网络 proto 时发现
- Category: 代码结构
- Instructions:
  - `game_session_config` 和 `game_session_manifest` 不属于 Dota GC direct 消息，而是 Source2 `CSVCMsg_ServerInfo` 的字段：`field 19 = game_session_config`、`field 20 = game_session_manifest`。
  - `spawngroupmanifest`、`manifestincomplete` 属于 Source2 `CNETMsg_SpawnGroup_Load / ManifestUpdate` 网络消息，也不在当前 `steam_game_coordinator.cpp` 的 GC 处理层。
  - 因此如果 server 卡在 `ss_waitingforgamesessionmanifest`，不能只在 GC direct 消息里补外围响应；需要同时确认 Source2 server->client 网络层是否真的发出了 session config/manifest，并确认客户端是否成功应用了这些消息。
  - 在 `7034` 已经对齐、但客户端仍稳定出现 `8880/8096/7504/8801` 超时的场景下，优先先补最小 direct 响应消除 GC 噪音，再判断 `INIT` 卡住是否仍与 Source2 session/manifest 层有关。
  - 当缺少 `7504/8096/8801/8880` 的 donor 抓包时，优先遵循“解析请求格式而后回复”原则：先按 proto 解析 request，再基于解析结果生成最小合法 response；不要继续完全忽略 request 体盲回固定包。

[gbe_fork 本地 Linux 构建前置依赖]
- Date: 2026-04-28
- Context: Agent 在本地验证 `steam_game_coordinator.cpp` 改动时发现
- Category: 构建方法
- Instructions:
  - 当前仓库执行 `./third-party/common/linux/premake/premake5 --file=premake5.lua --genproto --os=linux gmake2` 前，需要先准备 `build/deps/linux/gmake2/protobuf/install64/bin/protoc`。
  - 如果该 `protoc` 缺失，`premake5.lua` 会在生成工程阶段直接报 `protoc not found`，后续 `make config=debug_x64 api_regular` 无法开始。

[gbe_fork 禁止本机构建]
- Date: 2026-04-28
- Context: 用户要求后续在该仓库中不要再尝试本机构建
- Instructions:
  - 后续处理 `gbe_fork` 时，不要再在当前机器上尝试执行本地构建、生成工程或编译验证。
  - 需要验证时优先通过提交并推送到现有 PR，让远端 CI 负责构建检查。

[Dota2 Lobby SO 的 2015 与 121-124 字段语义]
- Date: 2026-04-29
- Context: Agent 在结合官方 `firstcreateloby.zip` 与“换位置后开始游戏仍闪退”的新日志复查 `24/26` 的大厅 SO 构造时修正
- Category: 代码模式
- Instructions:
  - `2015 = CSODOTAServerLobby` 不能一概保持空对象；在建房后和 `7047` 换位后的直构 `24/26` 路径里，最小安全形态应与官方样本一致，包含一个 `field1 = empty bytes` 的空成员条目，用来和 `2004/2014/2016` 对齐成员基数。
  - 只有 `7041` 启动四阶段里的最后一条 `26` 进入 `game_state=1` 时，才应把 `2015` 缩成空对象。
  - `2004.field121` 是 `member_indices`，单人本地大厅应归一化为单个 `0`。
  - `2004.field122`、`field123`、`field124` 分别是 `left_member_indices`、`free_member_indices`、`requested_hero_ids`，不能再把它们当作槽位占位符批量写 `0`。

[Dota2 Lobby SO 的运行态字段不能在 26 中丢失]
- Date: 2026-04-28
- Context: Agent 在复查 practice lobby 启动后续 `26 / PracticeLobbyDetailsUpdate` 时发现
- Category: 代码模式
- Instructions:
  - 用 scratch builder 重新构造 `2004 / CSODOTALobby` 时，必须同步当前运行态字段：`state(4)`、`connect(5)`、`server_id(6)`、`game_state(22)`、`match_id(30)`、`game_start_time(87)`。
  - 否则一旦启动后的 `7046/7047/...` 再触发新的 `26`，就会用缺字段的 `2004` 覆盖掉已启动 lobby 的运行态信息。

[Dota2 比赛内 7035 4511 4508 的当前处理语义]
- Date: 2026-04-28
- Context: Agent 在分析 `hoststartgame` 之后的 direct 请求日志与 SteamKit/go-dota2 定义，并确认 `7035` 体为空时发现
- Category: 代码模式
- Instructions:
  - 当前日志里的 `7035 / k_EMsgGCAbandonCurrentGame`、`4511 / k_EMsgGCLANServerAvailable`、`4508 / k_EMsgGCGameServerInfo` 都没有 `source_job`，更接近客户端或本地服发往 GC 的上行通知，而不是明确的 request-response。
  - `SteamKit` 里的 `CMsgAbandonCurrentGame` 当前是空消息；如果日志显示 `7035 len=8`，通常只是 direct proto 头，没有额外 body 字段，处理时不要先脑补 abandon 参数语义。
  - 在现有 `gbe_fork` replay 框架里，对这三条消息优先做“消费并记录关键字段”的最小处理，不要先凭猜测伪造 direct reply。

[Dota2 server-side 首轮 connected players 需要更早预热]
- Date: 2026-04-29
- Context: Agent 在分析新一轮 `console.log` 与 `gbe_gc_debug.log` 时发现 `CheckUpdateConnectedPlayers` 的“no leaver state”报错发生在 server 自发 `7034` 之前
- Category: 代码模式
- Instructions:
  - 如果 `console.log` 里 `Need to tell GC player is no longer connected, but we don't have a leaver state?` 出现在第一次 `Send msg 7034` 之前，那么仅修 `7034` reply 形态还不够，因为它只能影响后续状态同步，修不到这次最早的 server 判定。
  - 这类场景下应优先考虑在 server-side `4007 / k_EMsgGCServerHello` 完成 `4005 / ServerWelcome` 与 lobby `24` 之后，主动补一条无 job 的 `7034` 预热 server 侧的 connected players 视图，而不是等 server 自己晚些时候再发第一条 `7034`。

[Dota2 建房仅对齐 a3561972，开始游戏继续推进]
- Date: 2026-04-28
- Context: 用户纠正“只对齐建房，不要把其他后续修复也回退到该提交”
- Instructions:
  - 只让 `7038 / PracticeLobbyCreate` 建房路径对齐 `a3561972e01a81c2c6c37ef9b070c5c284cc6182`。
  - `7041 / PracticeLobbyLaunch` 以及之后为开始游戏链路新增的修复要继续保留并往前推进，不能因为建房对齐而一起回退。

[Dota2 7041 网络降级优先策略]
- Date: 2026-04-28
- Context: 用户要求在 `7041 / PracticeLobbyLaunch` 链路中优先绕过 `SteamNetworkingSocketsSerialized` 限制，强制客户端走 legacy UDP 直连
- Instructions:
  - 启动链路里凡是 donor 模板带有 `server_id`（64 位 SteamID 路由标识）的地方，优先将该 8 字节值整体清零，不要再替换成本地生成的 server SteamID。
  - `connect` 字符串需要继续强制 patch 为纯 IP 直连目标，优先使用 `127.0.0.1:27015`，并保持原 donor 总字节长度不变。
  - 如果启动相关 payload 中存在明显的 SDR ticket、证书或 relay 标识位，应优先做“保持长度不变的最小清零/降级”处理，目标是让客户端放弃 SDR/P2P 握手，退回原始 socket 连接。

[Dota2 进图后优先拦截服务器授权请求]
- Date: 2026-04-28
- Context: 用户要求在客户端已成功直连本地服务器并达到 `SIGNONSTATE_FULL` 后，优先补齐服务器侧向 GC 申请比赛授权/比赛详情的回复
- Instructions:
  - 当进入地图后，如果本地启动的 server 再次通过 `SendMessage_` 向 GC 发起新请求，要优先把这些 server-side 请求与客户端请求区分开来处理。
  - 优先观察并拦截 server 发出的 `k_EMsgGCServerHello` 或比赛详情/比赛授权类请求，再基于官方 donor 样本回放对应的授权包或比赛参数包。
  - 回放给 server 的授权类消息必须动态 patch 当前 `MatchID` 和 `LobbyID`，目标是让本地服务器脱离“等待授权/等待比赛参数”状态并推进到选人界面。

[Dota2 server/client GC 实例的 lobby 状态隔离]
- Date: 2026-04-28
- Context: Agent 在复查 `gbe_gc_debug.log` 中 server-side `4511` 多次出现 `local_lobby_id=0` 时发现
- Category: 代码模式
- Instructions:
  - 当前 `Steam_Game_Coordinator` 的 `GBE_local_lobby` 是实例级状态；client 侧在 `7038/7041` 中维护出的 lobby 运行态，不会自动出现在后起的 server-side GC 实例里。
  - 如果日志里 server-side `4511 / k_EMsgGCLANServerAvailable` 已经上报了正确 `lobby_id`，但同时打印 `local_lobby_id=0 matches_local=0`，应优先排查 client/server GC 实例之间的 lobby 状态同步，而不是先假设缺少某条固定 donor 回包。

[Dota2 server-side 4007 后的 24 区分]
- Date: 2026-04-28
- Context: Agent 在对照 `console.log`、`gbe_gc_debug.log` 与 direct replay 代码路径时发现
- Category: 代码模式
- Instructions:
  - server-side `4007 / k_EMsgGCServerHello` 之后看到的极小 `24 / CacheSubscribed`，不能默认当作 practice lobby SO cache；需要结合日志确认它是否只是一条过瘦的默认 cache。
  - 真正与 practice lobby 运行态对应的 `24` 应包含 `2004 / 2014 / 2015 / 2016` 这四类 SO，并能和日志里的 `owner=<Lobby:...>` 语义对上；排查时不要把它和纯 inventory `type_id=1` cache 混为一谈。

[Dota2 7041 启动 26 的 runtime rewrite 约束]
- Date: 2026-04-29
- Context: Agent 在继续修复 practice lobby 非默认位置启动即 `7035` abandon 的问题时发现
- Category: 代码模式
- Instructions:
  - donor-based `7041` 四条启动 `26 / PracticeLobbyDetailsUpdate` 不能只 patch `account_id`、`steam_id`、`lobby_id`、`match_id`、`server_id`、`game_start_time` 和 `connect`；其中内层 `2004/2016` 的成员 `team/slot` 也必须按当前运行态重写。
  - 对启动 `26` 做 runtime rewrite 时，不能再用空字符串或零值覆盖 `room_name`、`game_mode`、`server_region`、`pass_key`、bot 配置等 lobby 字段；应传入当前 `GBE_local_lobby` 的真实状态。

[Dota2 7041 启动 26 的 2014/2015 阶段语义]
- Date: 2026-04-29
- Context: Agent 在分析“`7041` 后仍立刻 `7035`”的新日志并回看当前 rewrite 逻辑时发现
- Category: 代码模式
- Instructions:
  - `7041` 启动阶段的四条 `26 / PracticeLobbyDetailsUpdate` 不应在前 3 条就把 `2015` 清空；只有最后一条进入 `game_state=1` 的阶段才应把 `2015` 缩成空对象。
  - 启动阶段若重写 `2014`，必须保留当前玩家名，不能把 `player_name` 传成空字符串，否则会把 lobby 静态成员名清空。

[Dota2 7041 启动 26 的 121/122/123/124 处理边界]
- Date: 2026-04-29
- Context: Agent 在复查 `CSODOTALobby` 的 proto 定义与当前 runtime rewrite 实现时发现
- Category: 代码模式
- Instructions:
  - `CSODOTALobby.field 121` 是 `member_indices`，`122` 是 `left_member_indices`，`123` 是 `free_member_indices`，`124` 是 `requested_hero_ids`；不要把 `124` 混成成员索引字段处理。
  - 在 donor-based `7041` 启动 `26` 的 runtime rewrite 中，如果暂时没有完全确认这组字段的运行态重建语义，优先保留 donor 原始 `121/122/123/124`，不要强行压成 `121=0` 或直接删除 `122/123/124`。

[Dota2 直构 2016 不能继续使用 donor 固定成员尾巴]
- Date: 2026-04-29
- Context: Agent 在分析“非默认位置仍在 `7041` 后立刻 `7035`”的新日志并回看 direct builder 时发现
- Category: 代码模式
- Instructions:
  - `2016` 的 SO 类型是 `CSODOTAServerStaticLobby`，不是 `CSODOTALobbyMember`；不要往 `2016` 的成员里写 `team`、`slot`、`leaver_status`、`leaver_actions` 这些仅属于 `CSODOTALobbyMember` 的字段。
  - 对 `2016` 的最小安全重写是：只在其 `all_members` 内更新 `CSODOTAServerStaticLobbyMember.field 1 = steam_id`，其余 server-static 字段沿用 donor 或保持缺省。

[Dota2 7041 connect 不能长期停留在 loopback 占位]
- Date: 2026-04-29
- Context: 用户指出当前 `7041` 启动时虽然有 `GBE_FormatDotaPracticeLobbyConnectFromIp(...)`，但实际仍把 `connect` 无条件写成 loopback，占位后也没有用后续真实地址推进
- Category: 代码模式
- Instructions:
  - `7041` 启动阶段不能长期把 lobby `connect` 固定为 loopback 占位；若能拿到本机实际 IP，应优先生成真实的纯 IP connect 字符串，而不是始终写死 `127.0.0.1:27015`。
  - 当本地 game server 后续通过 `4508 / k_EMsgGCGameServerInfo` 上报真实 `public_ip/private_ip/server_port` 后，应优先用这些运行态地址回写 lobby `connect`，而不是只记录日志不更新状态。

[Dota2 connect 排查的当前用户纠偏]
- Date: 2026-04-29
- Context: 用户纠正当前 `connect` 链路排查方向
- Instructions:
  - Dota2 practice lobby 当前连接端口固定为 `:27015`，不要继续把 `4508` 上报的 `server_port` 泛化进 `connect` 字符串逻辑。
  - 现阶段先不要动 `Steam_Networking_Sockets_*` 相关实现；优先继续沿 `connect` 链路收敛和修复问题。
  - 如果对开源项目或接口用法没有把握，先去查官方文档、GitHub 或公开资料，不要凭主观猜测扩展实现。

[旧 steamapi 的局域网链路参考优先级]
- Date: 2026-04-29
- Context: 用户在当前日志已证明 `7034` 早期缺口被补上后，要求优先从旧 `steamapi` 学习其“不改 `Steam_Networking_Sockets_*` 仍可实现局域网联机”的做法
- Instructions:
  - 后续继续排查当前 Dota2 局域网联机卡点时，优先对照 `/workspace/steamapi/unpacked/steam_api` 的局域网链路实现，先找它是如何在不修改 `Steam_Networking_Sockets_*` 的前提下跑通本地 server/client 联机的。
  - 在完成这轮旧 `steamapi` 对照之前，不要直接跳回 `Steam_Networking_Sockets_*` 改动。

[旧 steamapi 的 generic 玩家接入生命周期]
- Date: 2026-04-29
- Context: Agent 在对照旧 `steamapi` 的局域网联机实现时发现
- Category: 代码模式
- Instructions:
  - 旧 `steamapi` 局域网链路里，`Steam_GameServer::SendUserConnectAndAuthenticate`、`BeginAuthSession`、`CreateUnauthenticatedUserConnection` 在 generic 层面都会尽早落到 `add_player()`，并通过统一的 player lifecycle 维护“玩家已进服”的状态。
  - 当前 `gbe_fork` 虽然也会 `add_player()`，但如果 Dota server-side GC 仍卡在过早的连接态建立时机，可以优先把这条 generic `on_client_connected()` 生命周期桥接到 Dota-specific connected players 预热，而不是先去修改 `Steam_Networking_Sockets_*`。

[旧 steamapi 对照优先于动 serialized]
- Date: 2026-04-30
- Context: 用户要求继续排查 lobby 联机卡点时，先看旧 `steamapi` 的做法，尽量不要先改 `steam_networking_socketsserialized`
- Instructions:
  - 后续优先分析 `/workspace/steamapi/unpacked/steam_api` 里 lobby 联机和本地 server/client 接入路径，先确认旧实现是靠哪些 generic 生命周期和 connect 流程跑通的。
  - 在旧 `steamapi` 对照结论清楚之前，尽量先不要改 `dll/steam_networking_socketsserialized.cpp`。

[Dota host startgame 也要同步通用 lobby gameserver 快照]
- Date: 2026-04-30
- Context: Agent 在对照旧 `steamapi` 的 lobby/LAN 流程并继续排查 `DOTA_GAMERULES_STATE_INIT` 卡点时发现
- Category: 代码模式
- Instructions:
  - 仅推进 Dota-specific 的 `24/26/7034/7041` 还不够；当 host 的 `server_id` 首次就绪后，还需要同步通用 `Steam_Matchmaking::SetLobbyGameServer(...)`，让 generic lobby snapshot 也带上 `gameserver(id/ip/port)`。
  - 这样本地客户端侧才能走到旧 `steamapi` 依赖的 `LobbyGameCreated_t` / `LobbyDataUpdate_t` 联动，而不是只看到 Dota 自己的 lobby 运行态更新。
  - 该通用 gameserver 同步应继续保持 Dota lobby connect 的 `:27015` 约束，不要把 `4508.server_port` 直接推广成 lobby connect 端口语义。

[优先排查老 steamapi 联机路径与 24/26 SO 生命周期]
- Date: 2026-04-30
- Context: 用户在 generic lobby gameserver 同步已打通后，要求继续排查 host startgame 卡点时补充的新偏好
- Instructions:
  - 继续排查时不要改 `dll/steam_networking_socketsserialized.cpp`。
  - 优先学习 `/workspace/steamapi/unpacked/steam_api` 的旧联机路径，确认本地 host/client 是如何通过 generic lobby、snapshot 和 player lifecycle 跑通的。
  - 同时重点检查 `24/26` 对应的 lobby SO 生命周期，解释并修复客户端每次收到 `26` 后打印 `Lobby object destroyed` 的原因。

[Dota2 launch donor 的 prelude 小 24 也必须绑定 Lobby owner_soid]
- Date: 2026-04-30
- Context: Agent 在继续排查 host startgame 的 `Lobby object destroyed` 时发现
- Category: 代码模式
- Instructions:
  - `GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplayFromWrappedTemplate(...)` 生成的 donor-based `24 / CacheSubscribed`，无论是否启用 runtime rewrite，都必须在外层 `CMsgSOCacheSubscribed` 上强制写 `owner_soid = <type=3, id=lobby_id>` 并清掉旧 `owner`。
  - 这条约束不仅适用于大 `038` launch `24`，也适用于前置的 prelude 小 `24`；否则客户端可能在后续 `26` 到来时把 lobby SO 视为已销毁或 owner 失配。

[Dota2 direct 4007 必须按 CMsgServerHello.version 回 4005]
- Date: 2026-04-30
- Context: Agent 在检查 `Version out of date (GC wants 200, we are 6778)!` 时发现
- Category: 代码模式
- Instructions:
  - direct `4007 / k_EMsgGCServerHello` 的 body 在 `base_gcmessages.proto` 里只有 `CMsgServerHello.version = field 1`，不能把它按自定义多字段结构解析。
  - 当前 host launch 路径下，`4005 / k_EMsgGCServerWelcome` 的 `min_allowed_version` 和 `active_version` 应直接镜像 `4007.version`；否则客户端会把 GC 版本误判成 `200` 一类错误值，并打印 `Version out of date`。

[Dota2 host startgame 首段窗口应由 4506/7034/8330 驱动 26]
- Date: 2026-04-30
- Context: Agent 在重新对照 `/workspace/lobbystartgamedota2.zip` 的 `015-032` 首段窗口并修改 `steam_game_coordinator.cpp` 时发现
- Category: 代码模式
- Instructions:
  - 当 `4511` 首次把 lobby `server_id` 从 `0` 同步成真实 gameserver SteamID 后，应优先下发官方 donor 对应的 prelude 小 `24` 和 official 大 `24`，不要立刻排一长串 synthetic `26` 与外围 persona/auth/ticket 消息。
  - 这个窗口里的 `4506`、`7034`、`8330` 更接近客户端的 launch 轮询/推进信号；`4506` 后应先落到官方 `018` 对应的 `state=2/game_state=0`，`8330` 只回 `8331`，不要立刻再跟一条 synthetic `26`。
  - `4005 / ServerWelcome` 之后不要再主动 synthetic 推一条 `7034`；官方节奏是客户端自己发 `7034`，服务端再根据该轮询推进 lobby 状态。

[Dota2 host startgame 后段窗口要按 8870 挂起和官方 game_state 序列推进]
- Date: 2026-04-30
- Context: Agent 在继续对照 `/workspace/lobbystartgamedota2.zip` 的 `021-046` 窗口并接入官方 donor `26/8745` 时发现
- Category: 代码模式
- Instructions:
  - `8870` 本身不是立刻回包的 direct；它更像一个“挂起后续推进”的标记，应该让紧随其后的那次 `7034` 触发官方 `024/025` 双 `26` 窗口，而不是在 `8870` 收到时马上 synthetic 回一条 `26`。
  - 官方后段 `26` 的关键 `game_state` 推进值依次是：`018 -> 0`，`021/024 -> 1`，`025 -> 2`，`030 -> 3`，`032 -> 10`，`043/046 -> 4`；如果本地推进值不按这组序列走，容易再次偏离 `WAIT_FOR_PLAYERS_TO_LOAD/HERO_SELECTION/STRATEGY_TIME/PRE_GAME` 的真实节奏。
  - `039 out 8744` 应回官方 `042 in 8745`，随后 `040/045 out 7034` 再分别驱动官方 `043/046 in 26`，不要把 `8744` 混成普通空请求直接吞掉。

[Dota2 host startgame 的 persona/auth/ticket 外围消息要按阶段去重排队]
- Date: 2026-04-30
- Context: Agent 在继续排查“已进入游戏但 dashboard 仍显示主机载入中”，并对照 `/workspace/hoststartgame_unpacked/` 的 `7501/766/5501/5575/779/5429` 顺序时发现
- Category: 代码模式
- Instructions:
  - official 样本里的 rich presence/persona 不是单条静态消息；它会沿 `#DOTA_RP_INIT -> #DOTA_RP_FINDING_MATCH(SERVERSETUP) -> #DOTA_RP_FINDING_MATCH(RUN) -> #DOTA_RP_PRIVATE_LOBBY(RUN)` 逐步推进，并夹着 `5501/5575/779/5429` 这类外围消息。
  - 这些外围消息不能在 `4511/server_id` 首次同步时一次性全量倾倒；更稳妥的做法是按 launch 阶段去重排队：`7041` 只发 init persona，`server_id` 同步后补 `5501/5575/779 + setup persona`，`4506` 后补 `5575/779 + run persona`，`PRE_GAME 046` 窗口先补 `5429/779 + 766(server-run) + 766(run)`，最终 `766(private-lobby)` 需要再晚一拍，贴近官方 `025 out 7501(PRIVATE_LOBBY) -> 026 in 766(PRIVATE_LOBBY)` 的节奏。
  - 如果后续 dashboard 状态仍异常，优先先核对这些 persona/rich presence 阶段消息是否根本没发、发重了，或发到了错误阶段，而不是先怀疑 `CSODOTALobby.state/game_state` 顶层推进。

[Dota2 host startgame 的 dashboard 状态还依赖客户端主动 rich presence 上传]
- Date: 2026-05-01
- Context: Agent 在对照 `/workspace/hoststartgame_unpacked/` 与新日志时发现官方序列里有 `003/009/017/025 out 7501`，但本地日志完全没有 `7501`
- Category: 代码模式
- Instructions:
  - 官方 launch 后段不是单纯“服务端回几条 `766`”；每个 persona 阶段前，客户端还会先主动上传一条 `7501 k_EMsgClientRichPresenceUpload`。
  - 这 4 条 `7501` 的阶段分别对应：`#DOTA_RP_INIT + SERVERSETUP`、`#DOTA_RP_FINDING_MATCH + SERVERSETUP + party_state: IN_MATCH`、`#DOTA_RP_FINDING_MATCH + RUN + party_state: IN_MATCH`、`#DOTA_RP_PRIVATE_LOBBY + RUN + party_state: IN_MATCH`。

[Dota2 返回 dashboard 时官方 7035 走 wrapped 25 与双 PostGame 7010]
- Date: 2026-05-01
- Context: Agent 在对照 `/workspace/steamhoststartlobbyandleave/061-075` 官方抓包并实现 `7035` 修复时发现
- Category: 代码模式
- Instructions:
  - 返回主界面阶段的官方关键链路不是 replay 新的 `24/26`；而是 `061 out 5452(inner 7035) -> 062 in 5453(inner 25) -> 063 in 5453(inner 7010) -> 064 in 5453(inner 7010)`。
  - 这两个 `7010` 都是同一个 `PostGame_<lobby_id>` channel 响应，字段特征是 `field6=18`、`field8=1`，不能直接复用普通 lobby chat 的 `7009 -> 7010` 构造。
  - 处理这段链路时应优先走 wrapped `5453` 回复，并在 `7035` 窗口把 rich presence 先维持到 `#DOTA_RP_PRIVATE_LOBBY + party_state: IN_MATCH` 的过渡态；不要再猜测需要补新的 `24/26`。
  - 如果日志里只有 synthetic `766` 而完全没有本地 rich presence 更新，dashboard 仍显示“主机载入中”时，应优先补齐本地 `SteamFriends` rich presence 阶段更新，而不是继续只追加更多 inbound `766`。

[Dota2 official donor member 重写不能提前清零 leaver_status]
- Date: 2026-05-01
- Context: Agent 在继续对照 `/workspace/lobbystartgame.log`、`/workspace/console.log` 与 `steam_game_coordinator.cpp` 的 launch cache / official 26 donor 重写路径时发现
- Category: 代码模式
- Instructions:
  - 官方 `4511 -> 24` 的初始 lobby cache 中，`CSODOTALobby.all_members[0].leaver_status` 仍是 `DOTA_LEAVER_DISCONNECTED`；后续早期 `26` 才伴随 `state: SERVERSETUP -> RUN` 进入下一阶段。
  - donor/template 路径里的 `GBE_RewriteDotaLobbyTemplateMemberObject(...)` 不能把 member `field 16 = leaver_status` 与 `field 28 = leaver_actions` 无条件重写成 `0`，也不要在 donor 原本缺失时强行补这两个字段。
  - 否则本地 `24` 会过早显示 `DOTA_LEAVER_NONE`，破坏官方 `DISCONNECTED -> 后续修正` 的状态过渡，影响继续排查 dashboard `host loading` 问题时对关键 `26` 窗口的对照。

[Dota2 新 GC 客户端实例恢复私有房间时只补一次当前 24/26 快照]
- Date: 2026-05-01
- Context: Agent 在继续排查“profile 已到 PRIVATE_LOBBY 但 dashboard 仍显示主机载入中”，并复查 `GBE_RestoreSharedDotaLobbyState(...)` / `initialize_gc()` 与最新 `gbe_gc_debug.log` 时发现
- Category: 代码模式
- Instructions:
  - 当新的客户端 `Steam_Game_Coordinator` 实例启动时，如果 shared lobby 已经处于 `state=2, game_state=4` 的 practice private lobby，单靠 adopt shared runtime 和 rich presence 重放还不够；当前实例还需要补一份当前时刻的 lobby SO 快照给本地缓存。
  - 这次补发应复用现有当前态构建器，只发一次 direct `24 / CacheSubscribed` 加一次 direct `26 / LobbyDetailsUpdate`，顺序保持 `24 -> 26`，不要发明新的包结构。
  - 触发点应尽量收敛到“Dota2 GC 初始化”或“客户端从空本地 lobby 完整 adopt shared lobby”这类新实例恢复场景，避免在同一实例的正常 launch 状态推进中反复追加额外 `24/26`。

[Dota2 的 2016 member 需要区分 donor 重写与 runtime 自建]
- Date: 2026-05-01
- Context: Agent 在继续排查“已到 PRE_GAME 但 dashboard 仍显示主机载入中”，并对照 `gbe_gc_debug.log` 中 `2016.member[0]` 与 `steam_game_coordinator.cpp` 的 object `2016` donor/runtime 路径时发现
- Category: 代码模式
- Instructions:
  - donor `2016` 重写不应继续只改 `hero_id`；它至少要与 `2004.field 120 owner_state` 对齐 owner 的 `steam_id`、`hero_id`、`team`、`slot`，这样 `all_members[0]` 才不会长期停留在 donor 的错误队伍/槽位。
  - 但 donor 路径仍应保留官方样本自带的 `leaver_status/leaver_actions` 过渡，不要在 template rewrite 时无条件清零；这与 `4511 -> 24` 初始 `DISCONNECTED` 过渡有关。
  - runtime 自建 `2016` 则应显式补全 owner 的 `team`、`slot`、`leaver_status=0`、`leaver_actions=0`，避免在没有 donor 成员负载的 `24/26` 快照里再次退化成“只有 steam_id/hero_id”的精简 member 视图。

[Dota2 donor 2016 的 leaver_status 需要在 RUN 后再修正]
- Date: 2026-05-01
- Context: Agent 在顺序读完新一轮 `console.log` 与 `gbe_gc_debug.log` 后发现 `team/slot/hero` 已修正，但 dashboard 仍停留 host loading
- Category: 代码模式
- Instructions:
  - 初始 `4511 -> 24` 的 donor `2016.member[0].leaver_status` 仍应保持官方样本里的 `DOTA_LEAVER_DISCONNECTED`，不要过早在 `SERVERSETUP` 阶段改成 `NONE`。
  - 但当 donor `26` 已进入 `lobby_state=RUN` 后，如果 `2016.member[0]` 还一直保留 `DISCONNECTED` 且客户端后续没有新的 member 修正包，dashboard 可能会持续显示“主机载入中”。
  - 因此 donor `2016` 的最小修复策略是：`SERVERSETUP` 保留原始 leaver 过渡，`RUN` 及之后把 member 的 `leaver_status/leaver_actions` 改写为 `0`，同时继续保留 owner 的 `hero/team/slot` 同步。

[Dota2 donor 2016 的 leaver_status 字段是 fixed32 不是 varint]
- Date: 2026-05-01
- Context: Agent 在复查“RUN 后仍然 host loading”的最新日志并对照 `2016.member[0]` donor 布局时发现前一次修复没有真正覆盖旧值
- Category: 代码模式
- Instructions:
  - donor `2016.member[0].leaver_status` 在模板里对应 `field 16 / wire_type 5`，需要按 fixed32 重写，不能用 varint `field 16 / wire_type 0` 追加一个新字段冒充覆盖。
  - 如果误用 varint 追加 `field 16=0`，debug 看起来会出现额外字段，但客户端仍会继续读取原来的 fixed32 `3758096384`，导致 `CSODOTALobby.all_members[0].leaver_status` 继续显示 `DOTA_LEAVER_DISCONNECTED`。

[Dota2 官方抓包对比必须先解压并逐个按顺序解析]
- Date: 2026-05-01
- Context: 用户要求分析 `steamhoststart-hero.zip` 时明确指定排查方法
- Category: 代码模式
- Instructions:
  - 分析官方抓包压缩包时，先解压，再逐个文件解析，不能只挑个别消息或直接 grep 结论。
  - 对照本地实现时，必须同时核对官方数据包的结构、对象内容和先后顺序，按时间链路逐段比较。
  - 对于“大厅开始游戏到选择英雄”的问题，判断标准以官方抓包对应阶段的界面结果为准，例如进入选英雄后主界面应从“主机连接中”切到“离开/返回游戏”。

[Dota2 进入 PRE_GAME 后不能只凭 game_state=4 就重放 PRIVATE_LOBBY]
- Date: 2026-05-01
- Context: Agent 在逐个顺序对照 `steamhoststart-hero.zip` 的 `016-029` 与 `steam_game_coordinator.cpp` 后发现官方 `PRIVATE_LOBBY` 切换晚于 `046/PRE_GAME`
- Category: 代码模式
- Instructions:
  - 当 practice lobby 已进入 `state=2, game_state=4` 时，本地 rich presence/persona 仍不应立刻切到 `#DOTA_RP_PRIVATE_LOBBY`；官方在这之前还会经过 `5429`、后续 Steam 侧链路，再晚一拍才出现最终 `766(private-lobby)`。
  - 因此恢复 shared lobby 或重放 rich presence 时，不能只看到 `game_state=4` 就默认 `PRIVATE_LOBBY`；应至少等到“private lobby persona 已实际发送”的闩锁成立后，再把状态从 `#DOTA_RP_FINDING_MATCH + RUN` 切到 `#DOTA_RP_PRIVATE_LOBBY + RUN`。

[Dota2 当前真实运行里 PRIVATE_LOBBY 更适合挂在 RUN 后续 game_state 边沿上]
- Date: 2026-05-01
- Context: Agent 在顺序读完用户最新 `console.log` 与 `gbe_gc_debug.log`，并对照 `steamhoststart-hero` 的晚期 persona 时序后发现
- Category: 代码模式
- Instructions:
  - 当前这套 host start 流里，`7197/8673` 并不会在 `game_state=4` 时稳定出现；实际能稳定观察到的晚期 `RUN` 边沿是 `26` 把 lobby 从 `game_state=1 -> 2 -> 3` 推进到英雄选择/策略时间。
  - 因此如果要做最小实现来恢复 dashboard 的“Leave / Return to Game”，优先把 `PRIVATE_LOBBY` persona 闩锁挂在“`046` 已跑过且随后进入 `RUN` 的后续 game_state（当前日志里至少是 `>=2`）”上，而不是继续依赖 `7197/8673 && game_state==4`。
  - `7197/8673` 更适合保留为兜底补发时机；shared lobby rich presence 重放则只应依赖 `PrivateLobbyPersona` 闩锁位，而不是再次直接判断 `game_state==4`。

[Dota2 PRE_GAME 的 rich presence 重放可依赖 PregameRunPersona 闩锁而不必等最终 PrivateLobbyPersona 位]
- Date: 2026-05-01
- Context: Agent 在顺序读完新一轮 `gbe_gc_debug.log` 后发现 `032 -> 5429 -> 24 -> 24 -> 8744 -> 8745 -> 043 -> 046` 已完整出现，但 `GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(...)` 仍在 `state=2, game_state=4` 时反复把状态刷回 `#DOTA_RP_FINDING_MATCH`
- Category: 代码模式
- Instructions:
  - 不能只凭 `game_state=4` 就默认 `#DOTA_RP_PRIVATE_LOBBY`，但一旦 `046` 路径已经跑过并且 `GBE_kDotaLaunchPeripheralStagePregameRunPersona` 已经置位，shared-lobby restore/reapply 就可以把本地 rich presence 呈现为 `#DOTA_RP_PRIVATE_LOBBY + RUN`。
  - 如果仍强制等到 `GBE_kDotaLaunchPeripheralStagePrivateLobbyPersona` 最终置位才允许重放 private 状态，新的 coordinator 客户端实例会在 `PRE_GAME` 之后持续把 dashboard 刷回 host-loading/FINDING_MATCH，甚至可能抑制后续晚期 Steam 链的继续推进。
  - 更精确的条件是：`state=2 && game_state=4 && PregameRunPersona 已置位` 时允许本地 rich presence 呈现为 private；而最终 `PrivateLobbyPersona` 位仍保留给真正补发的晚期 `766(private-lobby)` 作为完成标记。

[Dota2 synthetic 26 应复用官方 donor，避免用极简 2004 覆盖 team_details]
- Date: 2026-05-01
- Context: Agent 在继续排查“profile 已到 PRIVATE_LOBBY 但 dashboard 仍显示主机载入中”，并复查 `GBE_BuildDotaPracticeLobbyDetailsUpdatePayload(...)` 与 `GBE_BuildDotaPracticeLobbySOObjectData(...)` 时发现
- Category: 代码模式
- Instructions:
  - 后续 synthetic `26 / LobbyDetailsUpdate` 不宜再从零构造精简 `2004/2014/2015/2016` 组合；极简 builder 当前会把 `2004.field 17` 退化成两个空 message，覆盖掉官方 donor 里已有的 team details / 队伍完成度视图。
  - 更稳妥的最小实现是复用现成的官方 `26` donor（当前可直接复用 `046` 模板）作为骨架，再通过 `GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(...)` 只重写运行时字段。
  - runtime 自建 member/owner_state 若仍需保留，`leaver_status(field 16)` 必须继续按 fixed32 编码，不能写成 varint。

[Dota2 launch donor 的 2004.field 16 与 046 后 persona 模板都必须显式改写]
- Date: 2026-05-01
- Context: Agent 在顺序读完新的 `gbe_gc_debug.log` 并对照 `GBE_RewriteDotaLobbyTemplateObject2004(...)`、`GBE_HandleDotaDirectPostLoginRequest(...)` 后发现
- Category: 代码模式
- Instructions:
  - `7046` 把 `GBE_local_lobby.room_name` 更新成新房间名后，如果 launch/official donor 的 `2004.field 16` 没有在模板重写阶段显式覆盖，客户端会继续 adopt donor 自带的旧 `game_name`，表现为建房后名称错乱、点槽位或 launch 后又跳回旧名字。
  - 因此 `GBE_RewriteDotaLobbyTemplateObject2004(...)` 不能只改 `lobby_id/state/connect/server_id/...` 这些运行时字段；也要把 `field 16 / room_name` 重写为当前 `GBE_local_lobby.room_name`，并在 donor 缺失该字段时补回去。
  - 当 `046` 已经把 lobby 推到 `state=2, game_state=4` 并且本地 rich presence 已切成 `#DOTA_RP_PRIVATE_LOBBY` 后，紧随其后的两条 `766` 不能继续排队 `...ServerRunHex` / `...RunHex` 这类 `FINDING_MATCH` 模板；必须改用现成的 `...ServerPrivateLobbyHex` 与 `...PrivateLobbyHex`，否则客户端会被后续 persona 包重新刷回 host-loading 视图。

[Dota2 2004 的 hero-select 相关数组要与索引字段分开观察]
- Date: 2026-05-01
- Context: Agent 在继续排查“hero selection 把 Dire 3 暂时显示成 Dire 1”并复查 `CSODOTALobby` proto 字段时发现
- Category: 代码模式
- Instructions:
  - `CSODOTALobby.field 124` 是 `requested_hero_ids`，`field 132` 是 `requested_hero_teams`；它们属于 hero-select 语义，不应继续和 `121/122/123` 这些成员索引字段混在同一含义里理解。
  - 后续如果 hero-select 槽位仍异常，要优先核对 donor `2004` 在 `124/132` 上是否残留旧请求数组，再决定是否清理或重建，而不是只盯 `all_members.team/slot`。
