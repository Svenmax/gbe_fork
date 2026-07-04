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

[GC 重构必须包含逻辑重构]
- Date: 2026-07-04
- Context: 用户要求后续 agent 继续推进 `gbe_fork` GC 重构时遵循
- Instructions:
  - 在 GC 代码完成机械移动后，必须继续进行逻辑重构。
  - 任务清单应明确覆盖每个 GC 领域的逻辑重构任务，避免只做文件拆分或代码搬运。
  - 每个领域的逻辑重构应包含请求解析、状态变更、消息构造、副作用隔离和 focused tests。

[GC 逻辑重构必须保护副作用顺序]
- Date: 2026-07-04
- Context: 用户询问如何避免 GC 逻辑重构改变 Dota 客户端敏感副作用顺序时确认
- Instructions:
  - 每个 GC handler 逻辑重构前，先写出当前副作用顺序清单。
  - 高风险 handler 应先补顺序测试，记录 action type、emsg、job id 和关键 payload 字段。
  - 纯逻辑 helper 应产出有序 action list，描述要执行的副作用和顺序。
  - 纯逻辑 helper 不直接调用 `push_incoming_now`、`save_items_to_file`、server GC、network broadcast 或 lobby snapshot replay。
  - Coordinator 方法负责按 action list 串行执行真实副作用。
  - 明确保护协议敏感顺序，例如 `CacheSubscribed` 在 `emsg21/26` 前、SO update 在 response 前、full item cache 在 create/update 前。
  - 每次只重构一个 handler 或一个小领域，并在变更后运行审计和离线 GC 测试。

[GC 拆分边界应保持领域内聚]
- Date: 2026-07-04
- Context: 用户询问当前拆分是否会因过细文件增加维护难度时确认
- Instructions:
  - GC 拆分目标是领域内聚和逻辑清晰，避免为了降低单文件行数继续制造过多细小文件。
  - 默认先在同一个 domain `.cpp` 内完成逻辑重构，只有出现跨领域复用、测试需要纯函数、或文件超过健康范围时才新建 helper 文件。
  - handler bucket 文件保持一个稳定领域，例如 inventory、chat、lobby、match、misc。
  - pure helper 文件保持一个明确责任，例如 wire parse/build、item serialization、lobby payload composition、state transition。
  - handler bucket 约 `300-1200` 行可接受，pure helper 约 `200-1000` 行可接受，超过健康范围后再考虑继续拆分。
  - 跨文件共享 helper 至少应有两个真实调用点，避免为了单个调用点提前抽公共 helper。
  - 单个 handler 的局部步骤优先放在对应 domain `.cpp` 的 anonymous namespace，等复用或测试边界明确后再移动。
  - 每次新增文件都要说明职责边界和迁移理由，并在完成后运行审计脚本和离线 GC 测试。

[减少可选进度汇报]
- Date: 2026-07-04
- Context: 用户要求后续交互投入更多思考并减少 commentary 汇报
- Instructions:
  - 在执行任务时减少可选进度更新和可选 commentary。
  - 仅在需要用户决策、出现阻塞或必须说明风险时发送中间说明。
