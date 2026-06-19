# 用户指令记忆

本文件记录了用户的指令、偏好和教导，用于在未来的交互中提供参考。

## 条目

[gbe_fork GC 重构验证方法]
- Date: 2026-06-17
- Context: Agent 在执行 gbe_fork 的 GC/proto wire 重构与验证时发现
- Category: 构建方法
- Instructions:
  - `gbe_proto_wire_test` 可用下面的命令独立编译并运行：`g++ -std=c++17 -I. -Idll tools/gbe_proto_wire_test/gbe_proto_wire_test.cpp dll/gbe_proto_wire.cpp dll/gbe_dota_gc_wire.cpp -o /tmp/opencode/gbe_proto_wire_test && /tmp/opencode/gbe_proto_wire_test`
  - `gc_message_utils_test` 可用下面的命令独立编译并运行：`g++ -std=c++17 -I. -Idll tools/gc_message_utils_test/gc_message_utils_test.cpp dll/gbe_gc_message_utils.cpp dll/gbe_proto_wire.cpp -o /tmp/opencode/gc_message_utils_test && /tmp/opencode/gc_message_utils_test`
  - `gbe_dota_custom_game_test` 可用下面的命令独立编译并运行：`g++ -std=c++17 -I. -Idll -Ilibs tools/gbe_dota_custom_game_test/gbe_dota_custom_game_test.cpp dll/gbe_dota_custom_game.cpp dll/gbe_proto_wire.cpp -o /tmp/opencode/gbe_dota_custom_game_test && /tmp/opencode/gbe_dota_custom_game_test`
  - `gbe_dota_lobby_flow_test` 可用下面的命令独立编译并运行：`g++ -std=c++17 -I. -Idll tools/gbe_dota_lobby_flow_test/gbe_dota_lobby_flow_test.cpp dll/gbe_dota_lobby_flow.cpp dll/gbe_dota_lobby_publish.cpp dll/gbe_dota_lobby_snapshot.cpp -o /tmp/opencode/gbe_dota_lobby_flow_test && /tmp/opencode/gbe_dota_lobby_flow_test`
  - `dll/steam_game_coordinator.cpp` 的直接语法检查会受工作区缺失生成头和第三方 include 影响，当前环境里可见的缺口包括 `json/json.hpp` 与生成的 protobuf 头。

[gbe_fork GC 重构执行规则]
- Date: 2026-06-17
- Context: 用户要求后续继续 gbe_fork 的 GC 重构推进
- Instructions:
  - 后续重构要优先做好测试覆盖，确保重构后的行为和原实现一致。
  - 每次拆分或迁移 GC 逻辑后，要补充回归验证，优先使用可重复的字节级或 fixture 级对比。
  - 发现行为差异时，先修正测试预期和实现差异，再继续扩大重构范围。
  - `gbe_proto_wire` 承载可复用的纯字节、格式化、解析和模板补丁 helper，后续抽离优先选择测试覆盖清晰、调用边界简单的函数。

[gbe_fork GC 重构边界规划]
- Date: 2026-06-18
- Context: 用户要求评估重构必要性后继续推进，并要求先确定边界和规划
- Category: 工作流协作
- Instructions:
  - 后续 GC 重构按三层边界推进：`gbe_proto_wire` 只放通用 protobuf wire/byte helper；Dota 专属纯字节规则放入 `gbe_dota_gc_wire`；`steam_game_coordinator.cpp` 保留 GC 路由、状态编排、Steam client 交互、日志和需要生成 protobuf 类型的逻辑。
  - 每次迁移优先选择输入输出完全由 bytes/string/标量组成的函数，迁移前补字节级测试，迁移后运行 `gbe_proto_wire_test` 和 `gc_message_utils_test`。
  - 暂缓迁移依赖 `Steam_Game_Coordinator` 状态、Steam client、rich presence、消息队列、日志策略或生成 protobuf 类型的函数。
  - 当前优先顺序为：先整理测试结构，再迁移 `GBE_RewriteDotaLobbyTemplateMemberObject`，再迁移 `GBE_RewriteDotaServerStaticLobbyMemberObject`，最后评估 `GBE_RewriteDotaLobbyTemplateObject2004` 是否用 options struct 拆分。

[gbe_fork Dota GC 文件拆分规则]
- Date: 2026-06-18
- Context: 用户说明自己不熟悉代码结构，要求后续拆分方案务必考虑全面后执行
- Category: 工作流协作
- Instructions:
  - Dota GC 后续拆分采用“三个 Dota 文件起步”的折中方案：保留 `gbe_dota_gc_wire.cpp/.h`，新增 `gbe_dota_custom_game.cpp/.h`，再按需要新增 `gbe_dota_lobby_flow.cpp/.h`。
  - 不要把所有 Dota 逻辑集中到一个新的大 cpp，避免形成新的 `steam_game_coordinator.cpp`。
  - 不要一开始拆太多 Dota 文件；等 `gbe_dota_lobby_flow.cpp` 变大后，再按业务增长拆出 `lobby_state`、`practice_lobby_flow`、`launch_flow`、`chat_flow`。
  - 归档规则：通用 protobuf 字节工具放 `gbe_proto_wire`；Dota 纯字节模板重写放 `gbe_dota_gc_wire`；自定义游戏、workshop mod、display name 和 metadata 放 `gbe_dota_custom_game`；lobby 创建、加入、踢人、换位置和 details 更新放 `gbe_dota_lobby_flow`；发消息、队列、Steam client、rich presence 和顶层分发留在 `steam_game_coordinator`。

[gbe_fork 提交流程限制]
- Date: 2026-06-18
- Context: 用户要求继续重构时调整后续协作方式
- Category: 工作流协作
- Instructions:
  - 后续继续开发和验证时先保留本地未提交状态，只有用户明确要求时再执行 git commit 或 git push。

[gbe_fork 重构拆分粒度]
- Date: 2026-06-18
- Context: 用户纠正后续 GC/Dota lobby 重构推进方式
- Category: 工作流协作
- Instructions:
  - 后续重构按业务块推进和说明，例如 member state update、owner adoption、generic lobby snapshot、chat channel sync 等块。
  - 避免只按零散 helper 连续抽离；每次选择一个清晰业务块，完成该块内必要的纯逻辑迁移、测试和回归。
