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
