# GC 重构后续深化任务清单

## 执行规则

- 每次只处理一个 stage 的一个必要任务或一个小型增强任务。
- 生产逻辑、测试补强、文档治理可以在同一 stage 内完成，但提交前必须能用清单说明边界。
- 每个任务完成后记录实际验证命令。
- 涉及 header 声明迁移、registry 迁移、seam 扩展或审计规则时运行 `tools/run_gc_verification.sh`。
- 当前环境缺少 Premake 时记录环境限制，具备工具环境后补 `premake5 --with-gc-tests gmake2`。

## 推荐启动顺序

1. 先执行 Stage 0，建立下一轮基线和外部验证缺口。
2. 再执行 Stage 4.1，封装 host showcase flag；该任务范围小、调用点少、适合作为下一轮代码变更起点。
3. 然后执行 Stage 3，推进 `EquipItemsPlan` executor 化；该任务架构收益高，必须依赖现有顺序测试保护。
4. Stage 1、2、5、6 可按审查容量穿插执行，每次只迁一个声明组、一个 seam 或一小批 registry entries。

## Stage 0：生产构建和运行时验证基线

- [ ] 0.1 记录当前验证基线
  - 运行 `tools/run_gc_verification.sh`
  - 记录 handler smoke、payload helper、audit 和 style gate 结果
  - 检查 `git status --short --branch`
- [ ] 0.2 补 Premake gate 验证记录
  - 在具备工具环境运行 `premake5 --with-gc-tests gmake2`
  - 验证 GC test target 可生成
  - 当前环境无工具时记录限制和后续执行命令
- [ ] 0.3 建立真实路径验证清单
  - 覆盖 `7035 abandon current game`
  - 覆盖 `7004 signout`
  - 覆盖 `8052/8053 custom game loading`
  - 覆盖 `2569 equip items full forward`
  - 覆盖 reconnect/direct-connect flow

## Stage 1：Internal header 继续收缩

- [ ] 1.1 盘点 `gbe_dota_gc_internal.h` 剩余声明分组
  - 分类 wire payload、lobby payload、template/replay、logging、stateful orchestration
  - 输出保留、迁移、下沉清单
- [ ] 1.2 迁移 wire payload declarations
  - 新建或复用窄 header 承载 DTO parser、proto patch、template patch 声明
  - 更新生产和测试 include
  - 验证 `tools/run_gc_verification.sh`
- [ ] 1.3 迁移 lobby payload declarations
  - 新建或复用窄 header 承载 lobby cache/details/launch/chat payload 声明
  - 避免同时移动 template/replay owner
  - 验证 `tools/run_gc_verification.sh`
- [ ] 1.4 评估 template/replay declarations
  - 保持大型 canned payload owner 不变
  - 仅在调用边界明确时新增窄 header
  - 验证 audit template ownership 仍通过

## Stage 2：Dependency seam 扩展

- [ ] 2.1 封装 server GC forward seam
  - 首选 inventory equip full forward 路径
  - 保持 full item cache 在 create/update 前
  - 增加或更新顺序测试
- [ ] 2.2 封装 network broadcast seam
  - 保留 business reason
  - 保持 local response、save、server GC forward 之后的当前顺序
  - 增加或更新顺序测试
- [ ] 2.3 封装 lobby publish/details update seam
  - 覆盖 lobby publish 或 practice lobby details update 中一个低风险路径
  - 保留 wrapped/session/reason 语义
- [ ] 2.4 封装 snapshot refresh seam
  - 优先处理 equip snapshot refresh
  - 保持 snapshot refresh 在网络广播后的当前顺序

## Stage 3：Equip action plan executor

- [ ] 3.1 定义 execution context
  - 收集 source job、local steam id、server GC availability、snapshot availability 等 executor 输入
  - context 不持有可跨 action 失效的引用
- [ ] 3.2 提取 `execute_equip_items_plan`
  - 放在 `gbe_dota_inventory_handlers.cpp` anonymous namespace
  - executor 负责 SO update、local response、save、server GC forward、network broadcast、snapshot refresh
  - planner 保持 pure helper 边界
- [ ] 3.3 扩展 equip executor focused tests
  - 覆盖 save、server GC forward、network broadcast、snapshot refresh 顺序
  - 覆盖 missing item 和 empty body 不执行 executor 副作用
- [ ] 3.4 评估推广条件
  - 记录 unlock/set style 是否适合 action plan 模式
  - 不满足复用条件时保持局部 executor

## Stage 4：Runtime/global state 继续封装

- [ ] 4.1 封装 host showcase flag
  - 新增 get/set/clear accessor
  - 保持 `7034` showcase repush guard 行为
  - 增加 focused test 或 audit coverage
- [ ] 4.2 封装 server hello cache
  - 新增 get/set/clear accessor
  - 保持 direct server hello parse 和 welcome replay 行为
  - 验证 payload helper/welcome coordinator 仍通过
- [ ] 4.3 封装 launch/sync flags
  - 覆盖 login sync sent、private lobby snapshot replayed、launch state pushed game state、persona signature
  - 每次只迁移一个小状态组
- [ ] 4.4 评估小型 runtime storage struct
  - 仅承载已通过 accessor 稳定的状态组
  - 保持 accessor API 不变
  - 不迁移 `GBE_local_lobby` 和 `GBE_shared_dota_lobby_state`

## Stage 5：Post-login registry 继续收口

- [ ] 5.1 选择下一批 simple direct-only handlers
  - 候选 profile/card、emoticon、conduct、coaching summary
  - 排除 template replay、launch chain 和复杂 fallback 路径
- [ ] 5.2 迁移少量 registry entries
  - 保留 direct/wrapped path guard
  - 保留 source job、target job、session 语义
- [ ] 5.3 扩展 dispatch audit
  - 更新 `_audit_gc_refactor.py` expected mapping
  - 覆盖新 adapter 的 path guard
- [ ] 5.4 运行 full verification
  - `tools/run_gc_verification.sh`
  - 记录迁移范围和保留显式路径原因

## Stage 6：Audit 和验证规则强化

- [ ] 6.1 增加 source-list inclusion audit
  - 检查新增 GC `.cpp` 是否进入 shell test source list 或记录豁免
  - 检查可选 Premake test target source list 是否同步
- [ ] 6.2 增加 side-effect seam audit
  - 对普通 handler 文件中直接调用高风险副作用 API 做提示或失败
  - 允许明确 owner 文件和 executor seam
- [ ] 6.3 增加 reason inventory audit
  - 检查高风险 reason string 是否出现在治理文档或测试中
  - 避免新增临时 reason 漂移
- [ ] 6.4 修正 payload helper test 计数显示
  - 当前输出存在 `[1/25]` 和实际 `26/26` 小不一致
  - 修正文案并保持测试语义不变

## Stage 7：交付检查点

- [ ] 7.1 运行完整验证
  - `tools/run_gc_verification.sh`
  - `bash -n tools/run_gc_verification.sh`
  - `bash -n tools/run_gc_offline_tests.sh`
- [ ] 7.2 检查文档索引
  - 确认 `.monkeycode/docs/INDEX.md` 引用的规格文件存在
  - 确认无断链目录
- [ ] 7.3 提交前审查
  - 查看 `git diff --stat`
  - 查看 `git status --short --branch --untracked-files=all`
  - 记录无法执行的 Premake 或真实客户端验证限制
