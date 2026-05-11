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

[Dota practice lobby 成员同步规则]
- Date: 2026-05-10
- Context: Agent 在修复 Dota2 LAN practice lobby 加入后成员互相不可见、加入者默认占位和加入首包闪退时发现
- Category: 代码模式
- Instructions:
  - practice lobby 的 `24/26` 中 `2004 CSODOTALobby` 必须包含所有房间成员，并同步 `member_indices`。
  - `2016 CSODOTAServerStaticLobby` 必须为房主和加入者都写入 static member，否则 UI 可能看不到对方。
  - 加入者初始状态应为 `DOTA_GC_TEAM_PLAYER_POOL slot=0`，不能默认复用房主的天辉 slot。
  - generic Steam lobby 成员列表是跨实例同步 Dota practice lobby 成员的来源，成员变化后需要推送新的 `26`。
  - 成员换槽位的 `team/slot/hero/connected` 不能只写进本进程 shared state；跨机器同步必须通过 generic lobby member data 传播，并且读取 generic 快照时不能用本地旧远端成员状态覆盖 member data。
  - owner 自己换槽位时也要读取 owner 的 generic lobby member data 并同步到 `owner_team/owner_slot`，否则非房主客户端会继续用默认 owner 天辉一号位覆盖房主的实际槽位。
  - 官方 `7044 -> 24` 加入首包的 SO 顺序是 `2004, 2015, 2013, 2014, 2016`；不能只按双人样本写死 `2`，应按实际成员数 `N` 同步生成 `2004.field120`、`2004.field121`、`2015.field1`、`2014.field1`、`2016.field1`。
  - 多成员 join 首包必须保持各 SO 对象成员基数一致；只 patch `2004/2016` 而不更新 `2015/2014` 会造成客户端在读取 `24` 时闪退风险。

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

[Dota2 闪退修复必须全面审计]
- Date: 2026-05-08
- Context: 用户反馈多次单点补丁后仍闪退，要求全面审计代码
- Instructions:
  - 排查 Dota2 practice lobby 闪退时，不要继续只根据日志末端打一处补丁。
  - 必须先系统审计 GC 生命周期、client/server coordinator 初始化释放顺序、abandon teardown 状态机、shared/local lobby 写点和历史提交差异，再做成组修改。
  - 输出或提交前要明确说明审计覆盖面、证据和仍存在的风险。

[Dota2 abandon persona 构建与最终收束]
- Date: 2026-05-08
- Context: Agent 在修复 `bf2c89a0` 后第一局断开仍闪退时发现
- Category: 代码模式
- Instructions:
  - `bf2c89a0` 后日志已不再出现 `GC_POLL` re-init，但 `7035 -> 25 -> 7010 -> 7010 -> 7272 -> 7014` 后仍缺少 final persona/rich presence/state teardown。
  - abandon persona 模板使用的 donor fixed64 SteamID 与 launch persona 不同；若只 patch `GBE_kOldDotaSteamIdFixed64`，`766` persona 构建会失败且不会入队。
  - stale pre-postgame `7272` 分支不能只回复 `7014` 后直接 return；它还必须执行最终 INIT/no-lobby persona、rich presence 和 shared/local lobby 收束。

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

[AP Start Game HERO_SELECTION 后续推进]
- Date: 2026-05-09
- Context: Agent 在修复 Dota2 AP Start Game 第二局英雄选择 UI 卡住时发现
- Category: 代码模式
- Instructions:
  - AP practice lobby 在 `RUN/HERO_SELECTION(game_state=2)` 后，不能只等待后续 `7034 game_state=3` 或 `send_reason=10` 才推进 `STRATEGY_TIME(game_state=3)`。
  - 当前日志显示客户端可在 `game_state=2` 后长时间停留于 `WAIT_FOR_PLAYERS_TO_LOAD`，直到后续交互或延迟请求才收到 `game_state=3`。
  - 修正这类问题时应优先保持 `7034` connected players 正常回复，同时主动排官方后续 `26` 状态链，避免改 game mode 或 UI 层。

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

[完整 abandon teardown 不能在 25 后立即 reset]
- Date: 2026-05-07
- Context: Agent 在排查 dashboard 主页点击“断开连接”后游戏闪退时发现
- Category: 代码模式
- Instructions:
  - 完整 `7035` abandon teardown 路径已经排入 `25 + 7010 + 7010` 并切换到 postgame chat 后，不能设置 `GBE_pending_reset_after_cache_unsubscribed` 在客户端取走 `25` 后立刻 `ResetGCMemory`。
  - 过早 reset 会在客户端继续发送 `7272` 前清掉 postgame/pre-postgame chat 状态，导致无法稳定完成官方 `7272 -> 7014` 收尾链，可能引发点击断开后的客户端闪退。
  - `GBE_pending_reset_after_cache_unsubscribed` 仅适合保留给未进入完整 postgame teardown 的 current-game disconnect 兜底路径。

[完整 abandon teardown 不能由 polling 重新初始化 GC]
- Date: 2026-05-08
- Context: Agent 在排查第一局断开连接后 `7272 -> 7014` 已返回但随后 GC re-init 闪退时发现
- Category: 代码模式
- Instructions:
  - 完整 `7035` abandon teardown 不能在 `25` 后 reset，也不能在 pre-postgame `7272 -> 7014` 被取走后立即 `ResetGCMemory("7035_abandon_after_7014", ...)`。
  - `7014` 后客户端/服务器侧可能马上执行 gameserver shutdown；如果此时由 `RunCallbacks()` 或 `IsMessageAvailable()` 因 `gc_initialized=false` 自动 `initialize_gc()`，会在 teardown 末尾重新拉起 Dota GC 并触发闪退。
  - polling 入口不应自动初始化 Dota2 GC；只保留真实 `SendMessage_()` 或 gameserver 显式初始化路径拉起 GC，完整 abandon 的 `7014` 仅确认离开 pre-postgame channel，不再作为 reset 触发点。
  - `GBE_pending_reset_after_cache_unsubscribed` 仅保留给未进入完整 postgame teardown 的 current-game `25` 消费后兜底 reset。

[第二局 GAMESTATE_TIMEOUT 需窄条件推进英雄选择]
- Date: 2026-05-07
- Context: Agent 在排查第一局断开后第二次开始游戏无英雄可选、dashboard 仍显示建房设置时发现
- Category: 代码模式
- Instructions:
  - 第二局 Dota server 可能发送 `7034`，其中 `send_reason=10(GAMESTATE_TIMEOUT)` 但 `game_state=0`，此时本地 lobby 仍停在 `RUN / WAIT_FOR_PLAYERS_TO_LOAD`。
  - 若 launch server setup 已完整同步且本地正处于 `state=RUN, game_state=WAIT_FOR_PLAYERS_TO_LOAD`，应把该 timeout 当作进入 `HERO_SELECTION` 的信号，回发带 `drafts=1` 的 connected players 视图。
  - 该推进必须保持窄条件，不能把 prelaunch 或非 wait-for-players 阶段的 `send_reason=10` 直接当作 hero selection，且同一个 timeout 包不应从 wait-for-players 连跳到 strategy time。

[官方 Dota chat channel id 区间]
- Date: 2026-05-09
- Context: Agent 在排查 `43c8f8b7` 仍在 `7014` 后闪退时对比官方 `steamhoststart-abandon` 抓包发现
- Category: 代码模式
- Instructions:
  - 官方 abandon 抓包中普通 lobby chat leave 使用的 channel id 位于 `0x62e000` 附近，例如 `6481464` 和 `6481871`。
  - 官方 postgame `7010` 使用的 channel id 位于 `0x62f000` 附近，例如 `6487736`。
  - 本地构造 Dota lobby/postgame chat channel 时应保持该区间形状，避免生成过低的 `0x1xxxx` 或 `0x10xxxx` channel id。

[Dota2 CM 断开稳定字段对齐]
- Date: 2026-05-09
- Context: 用户反馈 `fc95ea2a fix(gc): align Dota CM lobby object fields` 后 CM 模式断开连接不会闪退，并上传实测日志
- Category: 代码模式
- Instructions:
  - CM practice lobby 的稳定修复点包括：`CSODOTAServerStaticLobbyMember` 在 `game_mode=2` 时不写 `disabled_random_hero_bits` field `16`，仍保留四个 `banned_hero_ids=0` field `19`。
  - `CSODOTALobby` 需要补齐官方 AP/CM 都存在的 field `103=0`、`104=0`，并在 `RUN` 且 `WAIT_FOR_PLAYERS_TO_LOAD` 之后补 field `65=0`。
  - 最新 CM 实测日志显示断开链路完成 `7035 -> 25 -> 7010 -> 7272 -> 7014`，并在 `7014_pre_postgame_retrieved` 后 reset，未再出现断开闪退。

[Dota2 玩家交换英雄界面账号名查询]
- Date: 2026-05-09
- Context: Agent 在解析用户上传的 `steamplayerswaphero.zip` 英雄选择界面玩家交换英雄抓包时发现
- Category: 代码模式
- Instructions:
  - 玩家交换英雄界面会触发资料/公会/账号查询链，已知包含 `8729 -> 8730`、`8886 -> 8887`、`7534 -> 7535`、`8673 -> 8674`、`7197 -> 7198`。
  - 抓包新增确认 `2581 k_EMsgClientToGCLookupAccountName -> 2582 k_EMsgClientToGCLookupAccountNameResponse`，请求 field `1` 是 account_id，响应 body 包含 field `1` account_id 与 field `2` account_name。
  - 当前实现应对 `2581` 返回本地账号名，避免交换英雄界面查询其他玩家名称时缺响应。

[Dota2 正常结束比赛 GC 收尾链]
- Date: 2026-05-10
- Context: Agent 在解析用户上传的 `dota2hostfinishgame.zip` 与 `steamplayerfinishgame.zip` 正常结束抓包时发现
- Category: 代码模式
- Instructions:
  - 正常结束比赛会出现 `7381 k_EMsgGCGameMatchSignOutPermissionRequest -> 7382 k_EMsgGCGameMatchSignOutPermissionResponse`，响应至少需要 `permission_granted=1`。
  - Dota2 server 侧随后发送 `7004 k_EMsgGCGameMatchSignOut`，官方链路会回 `7005`，并推进 `2004` 到 `RUN/POST_GAME` 后再到 `POSTGAME/POST_GAME`，其中 POSTGAME 形状包含 field `70=2` 与 field `111=duration`。
  - 正常结束不应完全复用 abandon 的立即 postgame `7010` 行为；应先完成 `7005`、POSTGAME `26`、`25`，后续离开旧 chat channel 的 `7272` 再补 postgame `7010` 并回 `7014`。
  - Steam 侧正常结束后可能发送 `7082 k_EMsgGCSubmitPlayerReportV2`，应回 `7083` 且 `enum_result=1` 表示成功。

[Dota2 正常结束后主页状态收束]
- Date: 2026-05-10
- Context: Agent 在分析用户上传的正常结束后主页仍显示游戏中的 `gbe_gc_debug.log` 与 `console.log` 时发现
- Category: 代码模式
- Instructions:
  - 正常结束成功进入总结页后，日志显示 `7004 -> 7005 -> POSTGAME 26 -> 25` 完成，但 Dota server 不一定会继续发送 `7272`，因此不能只依赖 `7272/7014` 来恢复主页 INIT 状态。
  - 正常结束路径应在 `25 k_ESOMsg_CacheUnsubscribed` 被取走后执行最终收束：清理 shared/local lobby、清空 settings lobby，并向 Steam/client GC 侧推 INIT persona/rich presence。
  - Steam/client GC 侧必须保留 postgame chat channel 状态，直到后续旧频道 `7272` 到来后才能按官方链路补 `7010` 并回 `7014`；不能在 normal signout finalizer 中直接清空 client 侧 `GBE_local_lobby`。
  - 正常结束的 `7272` 会落在 Steam/client coordinator 上，而 postgame channel 是 Dota2/server coordinator 在 `7004` 后生成的；finalizer 需要把 server 侧 postgame lobby snapshot 同步给 client 侧，否则 client 只知道旧普通频道，只会回 `7014` 而不会补 `7010`。
  - postgame chat channel 应使用 `GBE_GenerateDotaPostGameChatChannelId()` 的 `0x62f...` 区间；普通 lobby chat 才使用 `GBE_GenerateDotaChatChannelId()` 的 `0x62e...` 区间。
  - abandon 路径仍应保持等待 `7272 -> 7014` 的收束方式，避免破坏已稳定的断开链路。

[Dota2 practice lobby 搜索加入离开链]
- Date: 2026-05-10
- Context: Agent 在解析用户上传的 `searchlobby.zip`、`joinlobby.zip`、`playerleavelobby.zip` 官方抓包时发现
- Category: 代码模式
- Instructions:
  - 搜索 practice lobby 的主线是 `8011 k_EMsgGCLobbyList -> 8012 k_EMsgGCLobbyListResponse`，同时客户端会发送 `7111 k_EMsgGCFriendPracticeLobbyListRequest`，官方可回空 body 的 `7112`。
  - `8012` 顶层包含 field `11` fixed64 `UINT64_MAX`，lobby entries 位于 repeated field `1`；friend practice lobby list 的 `7112` response 可为空。
  - 加入 practice lobby 的主线是 `7044 k_EMsgGCPracticeLobbyJoin -> 24 k_ESOMsg_CacheSubscribed -> 7113 k_EMsgGCPracticeLobbyJoinResponse`，`7113` 成功结果 field `1=0`，并镜像请求 job 到 target job。
  - 离开 lobby 的主线不是立即 `25`；官方在 `7040` 后先推一次 `26`，客户端随后发送 `8011/7111` 刷新列表，GC 再回 `25 + 8012 + 7112`，之后才处理旧 chat channel 的 `7272 -> 7014`。
