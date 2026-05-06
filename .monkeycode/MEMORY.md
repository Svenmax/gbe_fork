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

### 用户指令与执行约束

[抓包分析任务保持只读]
- Date: 2026-05-01
- Context: 用户要求分析 `/workspace/steamhoststart-hero/` 抓包文件并明确禁止修改文件
- Instructions:
  - 在抓包分析类任务中默认保持只读，不修改业务文件。
  - 输出结论时按文件顺序逐个分析，不跳号。

[未知 GC 消息排查优先级]
- Date: 2026-04-28
- Context: 用户要求在分析未知 GC 消息时约束排查方式
- Instructions:
  - 遇到未知的 GC 消息时，优先参考 SteamKit 和 go-dota2。
  - 也可以直接去 GitHub 搜索相关消息定义或实现。
  - 不要在缺少依据时自行猜想消息含义或处理方式。

[GC 抓包解析工具偏好]
- Date: 2026-05-03
- Context: 用户要求后续优先使用 SteamKit 自带的 nethook2 与 nethookanalyzer2 精确解析 GC 消息结构和内容
- Instructions:
  - 后续分析 Steam / Dota GC 抓包时，优先使用 `SteamKit/resources` 中的 `nethook2` 抓包工具与 `nethookanalyzer2` 解析工具。
  - 在需要精确展开 GC 消息结构、字段和值时，优先以 nethookanalyzer2 的解析结果为准。

[gbe_fork 构建方式]
- Date: 2026-05-03
- Context: Agent 在执行 `gbe_fork` 的 Start Game 对齐修正时发现
- Category: 构建方法
- Instructions:
  - `gbe_fork` 使用 `premake5.lua` / `premake5-deps.lua` 生成构建文件，而不是根目录 `CMakeLists.txt`。
  - Linux 下依赖构建入口参考 `README.md` 中的 `./third-party/common/linux/premake/premake5 --file=premake5-deps.lua ... gmake2`。

[gbe_fork 主页状态排查约束]
- Date: 2026-05-03
- Context: 用户要求排查 `gbe_fork` 中“已进游戏但主页/资料页状态不对”时的对比口径
- Instructions:
  - 排查 `gbe_fork` 的主页/资料页状态问题时，不能只看 `26`、`CSODOTALobby.game_state`、`7034`。
  - 必须同时对比 `7501` 从 `FINDING_MATCH` 切到 `PRIVATE_LOBBY` 的时间点。
  - 必须同时对比 `766` persona 回显的时间点。
  - 必须核对该切换点发生在 `lobby.state=RUN` 之后且处于 `GAME_IN_PROGRESS` 之前还是之后。

[官方 Start Game rich presence 顺序]
- Date: 2026-05-03
- Context: Agent 在对照 `/workspace/steamhoststartgame/` 的 `7501` 与 `766` 抓包时发现
- Category: 代码模式
- Instructions:
  - 官方 practice lobby `Start Game` 的 Steam rich presence 顺序是 `INIT -> FINDING_MATCH(SERVERSETUP) -> FINDING_MATCH(RUN) -> PRIVATE_LOBBY(RUN)`。
  - `025_out_7501` 时 lobby 已经是 `RUN`，但状态仍是 `#DOTA_RP_FINDING_MATCH`。
  - `030_out_7501` 才切到 `#DOTA_RP_PRIVATE_LOBBY`，并且紧跟着 `031_in_766` persona 回显同样的 `PRIVATE_LOBBY`。
  - `025_out_7501` 与 `030_out_7501` 之间的关键窗口是 `026_in_5453(inner 7014)`、`027/028_in_766(FINDING_MATCH,RUN)`、`029_in_5453(inner 26)`。

[主页状态分析排除手动切页]
- Date: 2026-05-03
- Context: 用户纠正 dashboard 切出是手动操作，只是为了观察主页状态
- Instructions:
  - 分析主页从“主机载入中”变为“断开、返回游戏”时，不能把 `ChangeGameUIState` 当成自动状态变化证据。
  - `ChangeGameUIState` 在这类日志里可能只是用户手动切到 dashboard 查看主页状态的结果。
  - 后续只要日志里的 `ChangeGameUIState -> DOTA_GAME_UI_STATE_DASHBOARD` 是用户手动切页，就默认忽略，不再把它当作异常切回或 lifecycle 断点证据。

[主页状态优先级]
- Date: 2026-05-03
- Context: 用户要求后续修正只关注主页状态转变，不再看档案页状态显示
- Instructions:
  - 后续只关注 dashboard 主页从“主机载入中”到“断开、返回游戏”的切换，不再以档案页状态显示作为目标。
  - 目标是进入英雄选择时，dashboard 主页就应同步变为“断开、返回游戏”。

[有下一步就直接继续]
- Date: 2026-05-04
- Context: 用户要求在已有明确下一步时直接继续推进，只有在不确定时才停下来澄清
- Instructions:
  - 如果我已经有明确的下一步，应直接继续执行，不要停在征求许可。
  - 只有在存在关键不确定性或缺少必要信息时，才停下来向用户请求澄清。

[Dota GC 按官方抓包重构]
- Date: 2026-05-06
- Context: 用户要求放弃现有补丁式代码，直接按官方抓包数据重构 Dota Start Game 与 abandon lifecycle
- Instructions:
  - 处理 Dota GC lifecycle 问题时，不要继续围绕现有补丁局部修补，应优先按官方抓包数据重构状态机。
  - 重构时必须严格核对消息时序、字段形状、对象要素、对象顺序和本地化账号字段。
  - 在已有官方抓包和日志足够支撑时，不要停下来反复询问用户，直接改到逻辑自洽后再汇报。

### 官方抓包与稳定机制

[官方 abandon 收尾链]
- Date: 2026-05-03
- Context: Agent 在对照 `/workspace/dota2hoststart-abandon/` 与 `/workspace/steamhoststart-abandon/` 官方同步抓包时发现
- Category: 代码模式
- Instructions:
  - 官方房主放弃游戏的 Dota GC 收尾链是 `7035 -> 25 -> 7010 -> 7010`，随后客户端再发 `7272`，GC 回 `7014`。
  - `7035` 之后 Steam rich presence 仍短暂保持 `#DOTA_RP_PRIVATE_LOBBY`，但会去掉 `lobby` 字段，只保留 `party_state: IN_MATCH`。
  - 离开 postgame chat 后，Steam rich presence 会回到 `#DOTA_RP_INIT`，因此本地 lobby/match 状态不能在 `7272` 之后继续保留。
  - 官方 persona 收尾顺序是 `085_in_766 PRIVATE_LOBBY(with lobby) -> 086_in_766 PRIVATE_LOBBY(no lobby) -> 092_in_766 INIT`，需要和 `7501` 的 rich presence 切换一起对齐。

[ConnectedPlayers send_reason 官方枚举]
- Date: 2026-05-03
- Context: Agent 在继续排查 `gbe_fork` 第二次启动卡英雄选择时查阅 `SteamKit` 与 `go-dota2` 的 `CMsgConnectedPlayers` proto 定义后发现
- Category: 代码模式
- Instructions:
  - `CMsgConnectedPlayers.SendReason` 的官方枚举里，`2` 是 `GAME_STATE`，`4` 是 `PLAYER_CONNECTED`，`10` 是 `GAMESTATE_TIMEOUT`。
  - 分析或门控 prelaunch `7034` 时，不能把 `send_reason=4` 误判为 game-state 推进信号。

[开始游戏问题按流程排查]
- Date: 2026-05-03
- Context: 用户要求分析 `second.zip` 时纠正排查视角
- Instructions:
  - 排查“没英雄可选”时，应优先把问题视为 `Start Game` 流程本身仍不完整，而不是围绕“第一局/第二局”做局部修补。
  - 即使日志来自断开后重新建房，也要先检查 lobby 生命周期和开始游戏主流程是否自洽，再判断是否与前一局残留有关。

[second.zip 官方 Start Game 主线]
- Date: 2026-05-03
- Context: Agent 在解析 `/workspace/second_zip` 的官方参考样本时发现
- Category: 代码模式
- Instructions:
  - 官方 Start Game 主线是 `7041 -> 26 SERVERSETUP(match_id) -> 7501/766 FINDING_MATCH SERVERSETUP -> 26 SERVERSETUP(server_id) -> 5429 -> 26 RUN(connect) -> 26 RUN(WAIT_FOR_PLAYERS_TO_LOAD) -> 7501/766 PRIVATE_LOBBY RUN`。
  - `5429 TicketAuthComplete` 位于 `RUN/connect` 之前，而不是只在更晚的 pregame/private-lobby 阶段出现。
  - `7501/766` 是跟随 `24/26` 状态骨架变化的外围回显，核心对齐对象仍应是 `24/26` 的 lobby state、server_id、connect、game_state 链。

[按官方抓包一次性收敛]
- Date: 2026-05-04
- Context: 用户对继续依赖“补发”或“门控”修窗口表示不满，要求直接按官方抓包主线与数据结构修正
- Instructions:
  - 不要继续通过额外补发消息或增加门控条件来维持窗口时序。
  - 优先按官方抓包的消息主线、对象集合、对象顺序和关键字段形状一次性收敛实现。
  - runtime builder 和条件补偿逻辑只能作为失败兜底，不能继续作为默认主路径。

[second.zip 关键26对象差异]
- Date: 2026-05-03
- Context: Agent 在对比 `032/038/040/049` 四条官方 `26` 与当前实现时发现
- Category: 代码模式
- Instructions:
  - 官方 `26 RUN(connect)` 仍然保留 `2015.extra_startup_messages[8869]`，直到 `26 RUN(WAIT_FOR_PLAYERS_TO_LOAD)` 才缩回只含空 member 的 `2015`。
  - `2016` 不能只同步 `steam_id`；其中与账号绑定的嵌套 `account_id` 数据也需要本地化，否则容易形成“外层是本地玩家，内层仍是 donor 账号”的假状态。

[second.zip 官方2016对象稳定字段]
- Date: 2026-05-03
- Context: Agent 在用 `/workspace/inspect_dota_gc_capture.go` 解析 `/workspace/second_zip/032/038/040/049` 四条官方 `26` 时发现
- Category: 代码模式
- Instructions:
  - 官方 `2016 / CSODOTAServerStaticLobby` 在 `032/038/040/049` 四个关键包里内容基本稳定，不会随着 `SERVERSETUP -> RUN(connect) -> WAIT_FOR_PLAYERS_TO_LOAD` 而被清空。
  - 该对象至少稳定包含 `all_members[].disabled_random_hero_bits`、`all_members[].banned_hero_ids`，以及 `lobby_event_points[].account_points[].account_id` 这类账号绑定嵌套数据。
  - `lobby_event_points[].account_points[].periodic_resources` 也会一起出现，因此修正 donor 账号残留时不能只看顶层 member 字段。

[Start Game 关键26必须全本地化]
- Date: 2026-05-03
- Context: 用户要求继续修正 `gbe_fork` 的 Start Game 主流程时补齐官方关键 `26` 缺失结构与 donor 残留字段
- Instructions:
  - 修正 Start Game 时，不能只继续分析；要把和官方关键 `26` 对比后缺失的结构一并补齐。
  - 生成的关键 `26` 要尽量与官方 `032/038/040/049` 保持相同对象要素和顺序。
  - `26` 中各对象内容必须使用本地数据，不能继续夹带 donor 或抓包模板中的旧账号数据。

[Steam侧与Dota2进程侧24/26不可混淆]
- Date: 2026-05-03
- Context: 用户说明 `dota2hoststartgame` 是开始游戏后 Dota2 启动出的另一个服务器进程与 GC 的会话
- Instructions:
  - 分析 `24/26` 时必须先区分会话来源：Steam 侧会话与 Dota2 进程侧会话不能直接混用。
  - `dota2hoststartgame` 中的 `24/26` 只有在确认语义和时序与 Steam 侧一致后，才能作为同类对照样本使用。
  - 如果两侧 `24/26` 的结构、时序或用途不同，后续分析和修正时必须分别处理，避免混淆。

[避免本机构建并允许恢复外部子模块]
- Date: 2026-05-04
- Context: 用户要求恢复 `third-party/common/linux` 并约束后续执行方式
- Instructions:
  - 将 `third-party/common/linux` 保持为仓库记录的原始状态，不要把本地改动带入提交。
  - 以后不要在当前机器上执行本地编译或构建验证；构建真值以用户实测日志或远端 CI 为准。

[运行期 7034 不能只回 26]
- Date: 2026-05-04
- Context: Agent 在分析用户上传的“英雄选择界面有了但没有英雄可选”新日志时发现
- Category: 代码模式
- Instructions:
  - 运行期 `7034` 请求即使会触发 `26` 的 lobby state 推进，也仍然需要继续返回真正的 `7034 connected players` 回复。
  - 如果只发送 `26` 而不回 `7034`，Dota server 会在英雄选择阶段持续缺少 connected player 和 draft seat 视图，容易表现为英雄列表为空。

[官方 Start Game 的 slot=4 并非异常]
- Date: 2026-05-04
- Context: Agent 在解析 `/workspace/second_zip` 与 `/workspace/steamhoststartgame` 的官方 `7047/26` 启动样本并对照当前失败日志时发现
- Category: 代码模式
- Instructions:
  - `/workspace/second_zip` 的官方成功链包含 `7047(team=0, slot=4)`，后续 `028/032/038/040/049` 多条官方 `26` 也持续保持 `2004.all_members[0].slot = 4`，因此 `slot=4` 本身不能再当作启动失败根因。
  - `/workspace/steamhoststartgame` 这套官方样本没有 `7047`，说明启动期是否出现 `7047` 取决于前序客户端操作链，不能把它当成必经主线。
  - 当前更应优先防止 owner 身份在 server 实例里退回到 `settings->get_local_steam_id()` 并污染成 gameserver steam id，同时避免 server-side synthetic `24` 的 `2016` 因缺少真实 `account_id` 而过瘦。

[ServerWelcome 后的 24 应优先复用 launch cache donor]
- Date: 2026-05-04
- Context: Agent 在复查 `gbe_fork/.monkeycode/MEMORY.md` 的旧结论并回看当前 `handle_dota_client_message()` 的 `ServerWelcome -> CacheSubscribed` 实现时发现
- Category: 代码模式
- Instructions:
  - `4511/ServerWelcome` 后的关键 `24 / CacheSubscribed` 在官方链路里不应长期停留在 scratch builder 路径；launch 已开始时应优先复用官方 launch cache donor 模板，再按当前运行态重写字段。
  - 若 `match_id != 0` 但 `server_id == 0`，优先尝试 launch cache prelude donor；若 `server_id != 0`，优先尝试完整 official launch cache donor。
  - 只有 donor 路径构造失败时，才回退到当前态直构 `24`，避免继续用结构过瘦的 synthetic cache 去撞官方状态机。

[Start Game 启动链避免额外旁路补偿]
- Date: 2026-05-05
- Context: Agent 在按 `/workspace/steamhoststart-abandon/` 与 `/workspace/dota2hoststart-abandon/` 继续收敛 `gbe_fork` 的 Start Game GC 流程时发现
- Category: 代码模式
- Instructions:
  - `7041` 之后的主线应围绕当前权威 lobby `24/26` 对象推进，而不是继续回放 donor `stage1..4` 启动模板。
  - 启动期不要再通过 `on_client_connected` synthetic `7034`、`4506` 直接推 RUN、hero-selection 额外补 `26`、late steam chain 强塞 private-lobby persona 这类旁路补偿维持时序。
  - `RUN(connect)` 的推进点应优先贴近官方 `5429 -> 26 RUN(connect)` 骨架，再由后续真实 `7034` 继续驱动 `WAIT_FOR_PLAYERS_TO_LOAD` 与更晚状态。

[Start Game 剩余 peripheral builder 仅服务 abandon persona]
- Date: 2026-05-05
- Context: Agent 在清理 `gbe_fork/dll/steam_game_coordinator.cpp` 的失效启动模板常量后发现
- Category: 代码模式
- Instructions:
  - `GBE_BuildDotaPracticeLobbyLaunchPeripheralMessage()` 与 `GBE_BuildDotaPersonaStatePeripheralMessage()` 目前保留的有效用途是 abandon/postgame persona 构造，不再承担 Start Game 默认主路径。
  - `5501/5575/779` 对应的 Start Game 启动模板常量和 `stage1..4` donor `26` 模板已经全部停用并移除；后续若再出现 Start Game 时序问题，应优先检查权威 `24/26` 与真实 `7034` 主线，而不是恢复这批外围模板。

[官方 PRIVATE_LOBBY rich presence 切换早于英雄选择]
- Date: 2026-05-05
- Context: Agent 在继续对照官方 Start Game 样本与 `GBE_ReapplyDotaPracticeLobbyLaunchRichPresence()` 时发现
- Category: 代码模式
- Instructions:
  - 官方 `7501/766` 从 `FINDING_MATCH RUN` 切到 `PRIVATE_LOBBY RUN` 的时间点早于英雄选择，不应等到 `game_state >= 2` 才切换。
  - 本地 rich presence 在 launch 运行期应从 `WAIT_FOR_PLAYERS_TO_LOAD` 起就允许呈现 `PRIVATE_LOBBY RUN`，避免把 `PRIVATE_LOBBY` 延后到 hero selection 之后。

[SetRichPresence 不会自动生成 766 persona]
- Date: 2026-05-05
- Context: Agent 在继续排查 `gbe_fork` Start Game 期间 `7501/766` 外显链时查看 `dll/steam_friends.cpp` 与官方样本后发现
- Category: 代码模式
- Instructions:
  - `Steam_Friends::SetRichPresence()` 只会更新本地 rich presence 数据并触发 `FriendRichPresenceUpdate_t` / `PersonaStateChange_t` callback，不会自动向 Dota 的 GC 消息队列注入 `766` persona 回显。
  - 如果要对齐官方 Start Game 的 `7501 -> 766` 外显链，需要在状态主线切换点显式排入匹配官方形状的 `766`，不能只依赖本地 `SetRichPresence()`。

[SteamKit 确认 ServerStaticLobbyMember 英雄限制字段]
- Date: 2026-05-06
- Context: Agent 在执行 GitHub SteamKit 搜索以修复第二局 hero selection 无英雄时发现
- Category: 代码模式
- Instructions:
  - SteamKit `MsgGCCommonLobby.cs` 确认 `CSODOTAServerStaticLobbyMember.disabled_random_hero_bits` 是 field `16`，wire type 为 fixed32 repeated。
  - `disabled_hero_id` 是 field `17`，`enabled_hero_id` 是 field `18`，`banned_hero_ids` 是 field `19`。
  - 不要用 `banned_hero_ids=0` 伪造空禁用列表；空列表应直接不写 field `19`，否则可能污染 hero selection 英雄池。

[嵌入式 LAN server 可能不发送 5429]
- Date: 2026-05-06
- Context: Agent 在分析用户上传的“开始游戏后主机一直载入中、没有英雄选择”日志时发现
- Category: 代码模式
- Instructions:
  - 当前测试日志里 `7041 -> 26 SERVERSETUP -> 4511/server_id sync -> 26 SERVERSETUP(server_id)` 后没有出现 `5429 TicketAuthComplete`，但 server-side 会继续发送 `7034 GAME_STATE` 和 `4506 ServerAvailable`。
  - `5429` 仍是官方优先锚点，但不能作为唯一 `RUN(connect)` 推进条件；server_id 已同步且 connect/game_start_time 完整时，可用真实 server-side `7034` 或 `4506` 作为受限 fallback。
  - fallback 不应恢复旧的 `4511/4508` 无条件旁路，也不能跳过 `7034` connected players 回复。
