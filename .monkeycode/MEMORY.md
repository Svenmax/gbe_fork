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
  - 拦截 `7038 / k_EMsgGCPracticeLobbyCreate` 后，不只回 `7055`，还要按顺序补发官方抓包里的 SO Cache lobby 更新包。
  - `7055` 必须使用 9 字节扩展头，并把请求的 `SourceJobID` 填到扩展头的 `field11`。
  - 需要在 C++ 中随机生成新的 `uint64_t` LobbyID，并把抓包模板中的旧 LobbyID 全部等长替换成新值。
  - 需要把抓包模板中的旧 SteamID/AccountID 全部替换为本地玩家真实 ID，确保客户端把本地玩家识别为房主。
  - 需要在模拟器内存中持久化当前本地 lobby 状态，并为每一发建房相关回包打印调试日志。

[Dota2 Practice Lobby 建房抓包规律]
- Date: 2026-04-21
- Context: Agent 在分析 `/workspace/1776260078-create-lobby.zip` 的首轮 `7038` 建房成功抓包时发现
- Category: 代码模式
- Instructions:
  - `7038 / k_EMsgGCPracticeLobbyCreate` 是走外层 `5452/5453` 包裹的 wrapped direct 流程，不是当前登录后常见的裸 direct 请求。
  - 官方建房成功首轮顺序是先回 `7055` 成功响应，再回 `6146` 的 lobby SO 更新包；`7055` 内层包体只有 `08 01`。
  - `7038` 请求的 9 字节 inner header 使用 field10 固定 64 位 job 值，官方 `7055` 响应改为 field11 并复用同一个 job 值。
  - 首轮 `6146` 模板里包含旧 LobbyID 的 8 字节 varint 以及旧房主 SteamID 的 fixed64，需要统一替换成本地值。

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
  - 建房回包逻辑需要同时兼容 direct 与 wrapped 两种入口，两者都应复用同一套 `7055 -> 6146` 发送顺序与 LobbyID 替换逻辑。

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
  - 如果建房后只发 `24 + 7055`，客户端首屏可能继续显示缓存模板中的旧房主名、旧房间名或旧服务器地区；建房成功后应尽快补发一条基于本地状态的 `26` 大厅详情更新。

[Dota2 Practice Lobby 首屏落位与 CacheSubscribed]
- Date: 2026-04-22
- Context: Agent 在排查“建房后房主不立即出现在天辉第一个位置”时发现
- Category: 代码模式
- Instructions:
  - 即使 `7038` 的回包顺序已调整为 `7055 -> 24 -> 26`，如果 `24 / CacheSubscribed` 仍复用旧 lobby 模板，客户端首屏仍可能显示错误的成员落位。
  - Practice Lobby 的 `24` 应与 `26` 共用同一份本地构造的 SO 对象数据，至少保持 `2004`、`2014`、`2015`、`2016` 的 object_data 一致，避免首屏成员状态与后续 `26` 更新不一致。

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
- Context: 用户验证改成 `24 -> 7055 -> 26` 后仍然首屏不落位，结合 `firstcreateloby.zip` 官方建房首轮无 `26` 的事实得出
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
