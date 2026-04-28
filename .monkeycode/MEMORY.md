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
- Context: Agent 在继续实现 `7034 / k_EMsgGCConnectedPlayers` 最小回包时发现
- Category: 代码模式
- Instructions:
  - 当前 `GBE_local_lobby.owner_team` 的取值已经与 Dota proto `DOTA_GC_TEAM` 对齐：`0 = GOOD_GUYS`，`1 = BAD_GUYS`。
  - 因此构造 `CMsgConnectedPlayers.PlayerDraft.team` 时可以直接复用 `owner_team`，只需在异常值场景下兜底回 `0`，不需要再额外做 UI 编号到 proto 枚举的二次映射。

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
- Date: 2026-04-28
- Context: Agent 在复查 `24/26` 的大厅 SO 构造与模板 patch 时发现
- Category: 代码模式
- Instructions:
  - `2015 = CSODOTAServerLobby` 在当前本地大厅路径里应保持空对象；不要把它错误地构造成 `field1 = empty bytes` 的“带一个空 member 的对象”。
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
- Context: Agent 在分析 `hoststartgame` 之后的 direct 请求日志与 SteamKit/go-dota2 定义时发现
- Category: 代码模式
- Instructions:
  - 当前日志里的 `7035 / k_EMsgGCAbandonCurrentGame`、`4511 / k_EMsgGCLANServerAvailable`、`4508 / k_EMsgGCGameServerInfo` 都没有 `source_job`，更接近客户端或本地服发往 GC 的上行通知，而不是明确的 request-response。
  - 在现有 `gbe_fork` replay 框架里，对这三条消息优先做“消费并记录关键字段”的最小处理，不要先凭猜测伪造 direct reply。

[Dota2 建房初始 24 不要零化运行态字段]
- Date: 2026-04-28
- Context: Agent 在回归建房闪退时发现
- Category: 代码模式
- Instructions:
  - `7038` 之后的初始 `24 / CacheSubscribed` 仍处于未启动 lobby，`2004` 里的 `connect(5)`、`server_id(6)`、`game_state(22)`、`match_id(30)`、`game_start_time(87)` 不应被强行改写成空字符串或 `0`。
  - 对这些运行态字段，只有在本地确实已经生成有效运行态值时才覆盖 donor；否则保留 donor 原字段，避免客户端在建房阶段因异常 lobby SO 直接闪退。
