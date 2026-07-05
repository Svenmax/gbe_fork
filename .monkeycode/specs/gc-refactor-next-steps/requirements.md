# GC 重构后续需求

## 目标

GC 后续重构应在保持 Dota 客户端协议行为稳定的前提下，继续把已完成的机械拆分推进为清晰的逻辑边界。重点是拆分高风险 handler 的请求解析、状态决策、消息构造和副作用执行，并通过离线测试保护可观察行为。

## 功能需求

1. 当重构任一高风险 GC handler 时，系统应保持 handler 对外入口和现有协议语义稳定。
2. 当 handler 包含多个顺序敏感副作用时，系统应先通过测试或清单明确当前副作用顺序。
3. 当提取 pure planner、decision helper 或 DTO parser 时，该逻辑应避免调用 coordinator、network、file save、server GC forward、lobby publish 和全局 setter。
4. 当 coordinator 执行 plan 或 decision 时，系统应按显式顺序执行真实副作用，并保留 job、session、wrapped、reason 和关键 payload 字段语义。
5. 当新增或调整测试时，测试应优先验证 action 顺序、emsg、source job、target job、outer session、payload 关键字段、state mutation 时机和 external forward 参数。
6. 当新增源文件、测试文件或声明迁移时，构建入口、测试入口和审计入口应同步更新。

## 阶段需求

### Stage 0：基线保护和清理

1. 清理已知构建警告和配置不一致点。
2. 保持 `tools/run_gc_offline_tests.sh`、`tools/run_gc_offline_tests.sh --full` 和 `git diff --check` 可重复通过。
3. 在具备工具环境验证 `premake5 --with-gc-tests gmake2`。

### Stage 1：Inventory action-list pilot

1. `GBE_HandleDotaEquipItemsRequest` 应形成 pure planner 和 explicit executor 的试点结构。
2. planner 应输出 item mutation 意图、response body、cache version、server forward 参数、broadcast/snapshot reason 和有序 action list。
3. executor 应保持 response、save、server GC forward、network broadcast 和 lobby snapshot refresh 的当前顺序。

### Stage 2：测试覆盖加深和 harness 收缩

1. Match、lobby、chat 的关键路径应具备更真实的 payload 和分支覆盖。
2. `tools/gbe_dota_handler_test/stubs.h` 应保持清晰领域分区，新增测试应避免 stub 膨胀。

### Stage 3 及以后：逻辑边界收口

1. Lobby、launch、reconnect 和 abandon 相关状态判断应逐步进入无副作用 decision helper。
2. Protocol DTO 和 routing adapter 应逐步减少裸 wire 字段读取和重复 direct/wrapped routing。
3. Dependency seam、persistence 和 global state 访问应逐步收敛为显式函数级边界。

## 验收标准

1. 每轮变更集中在一个 stage、一个 handler 或一个小型共享边界。
2. 本轮对应任务在 `tasklist.md` 中勾选，并记录实际验证命令。
3. `tools/run_gc_offline_tests.sh --full` 通过。
4. `git diff --check` 通过。
5. 涉及声明迁移、新增 helper 或边界重构时，`python3 tools/_audit_gc_refactor.py` 通过。
6. 涉及 Premake 配置时，具备工具环境下 `premake5 --with-gc-tests gmake2` 通过，或在任务清单中记录当前环境限制。

## 非目标

1. 一次性重写全部 GC handler。
2. 引入大型 command framework、通用 executor 框架或完整 state pattern。
3. 在缺少 focused tests 的情况下替换 canned payload、template replay bytes 或 protobuf patch 行为。
4. 为单个调用点提前抽取跨领域公共 helper。
