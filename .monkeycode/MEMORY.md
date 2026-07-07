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
  - Dota GC 相关改动完成后运行 `tools/run_gc_verification.sh` 作为完整验证闸口。
  - 该命令覆盖 GC offline tests、replay fixtures、header declaration 审计、source-list inclusion 审计和 handler side-effect seam 审计。
