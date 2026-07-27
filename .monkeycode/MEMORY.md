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
- Category: [运维部署|构建方法|测试方法|排错调试|工作流协作|环境配置]
- Instructions:
  - [具体的知识点，逐行描述]

## 去重策略

- 添加新条目前，检查是否存在相似或相同的指令
- 若发现重复，跳过新条目或与已有条目合并
- 合并时，更新上下文或日期信息
- 这有助于避免冗余条目，保持记忆文件整洁

## 条目

[维护性优先的重构准则]
- Date: 2026-07-06
- Context: 用户评价 Dota GC 后续重构计划时提出
- Instructions:
  - 重构应以便于更新维护、降低真实维护风险为目标。
  - 不为了美观、表面整洁或单纯移动代码而重构。

[提交闸口自动提交]
- Date: 2026-07-07
- Context: 用户在 Dota GC 小步开发流程中补充协作偏好
- Instructions:
  - 当一个小边界改动已完成、验证通过且到达提交闸口时，直接提交该小步，不再等待额外提交授权。

[Dota GC 验证闸口]
- Date: 2026-07-07
- Context: Agent 在执行 Dota GC 架构收口和测试护栏补强时发现
- Category: 测试方法
- Instructions:
  - Dota GC 相关改动完成后运行 `tools/run_gc_verification.sh --full` 作为完整验证闸口。
  - 该命令覆盖 GC offline tests、审计与 diff --check；通过不等于真协议验收。

[Dota GC 文档入口与真相表]
- Date: 2026-07-13
- Context: 用户要求生成防多 Agent 遗忘的重构记忆文档
- Category: 工作流协作
- Instructions:
  - 唯一状态入口：`docs/gc/CURRENT.md`；任务队列：`docs/gc/ACTIVE_QUEUE.md`。
  - 改 emsg 路由必须同步 `docs/gc/MESSAGE_ROUTING_INVENTORY.md`；新消息只进 production registry。
  - 改 hero/wearable/showcase 必须同步 `docs/gc/HOST_AUTHORITY.md`，并跑 dual_gc H1–H5 与相关 GOLDEN_PATHS。
  - 行为改动在 PR/说明中点名 `docs/gc/GOLDEN_PATHS.md` 的 PathID；协作流程见 `docs/gc/AGENT_PLAYBOOK.md`。
   - `follow-up-task-list.md` / `next-agent-task-list.md` / 长篇 delivery 仅作历史档案，不作权威入口。

[默认工作目录]
- Date: 2026-07-22
- Context: 用户在当前会话中指定项目操作目录
- Category: 工作流协作
- Instructions:
  - 后续项目操作默认以 `/workspace/gbe_fork` 作为工作目录。
