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
