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
- Date: 2026-06-15
- Context: Agent 在执行 gbe_fork 的 direct proto 重构与验证时发现
- Category: 构建方法
- Instructions:
  - `gbe_proto_wire_test` 可用下面的命令独立编译并运行：`g++ -std=c++17 -I. -Idll tools/gbe_proto_wire_test/gbe_proto_wire_test.cpp dll/gbe_proto_wire.cpp -o /tmp/opencode/gbe_proto_wire_test && /tmp/opencode/gbe_proto_wire_test`
  - `gc_message_utils_test` 可用下面的命令独立编译并运行：`g++ -std=c++17 -I. -Idll tools/gc_message_utils_test/gc_message_utils_test.cpp dll/gbe_gc_message_utils.cpp dll/gbe_proto_wire.cpp -o /tmp/opencode/gc_message_utils_test && /tmp/opencode/gc_message_utils_test`
  - `dll/steam_game_coordinator.cpp` 的直接语法检查会受工作区缺失生成头和第三方 include 影响，当前环境里可见的缺口包括 `json/json.hpp` 与生成的 protobuf 头。

## 条目

[项目知识摘要]
- Date: 2026-06-15
- Context: Agent 在执行 gbe_fork 的 direct proto 重构与验证时发现
- Category: 构建方法
- Instructions:
  - `gbe_proto_wire_test` 可用下面的命令独立编译并运行：`g++ -std=c++17 -I. -Idll tools/gbe_proto_wire_test/gbe_proto_wire_test.cpp dll/gbe_proto_wire.cpp -o /tmp/opencode/gbe_proto_wire_test && /tmp/opencode/gbe_proto_wire_test`
  - `gc_message_utils_test` 可用下面的命令独立编译并运行：`g++ -std=c++17 -I. -Idll tools/gc_message_utils_test/gc_message_utils_test.cpp dll/gbe_gc_message_utils.cpp dll/gbe_proto_wire.cpp -o /tmp/opencode/gc_message_utils_test && /tmp/opencode/gc_message_utils_test`
  - `dll/steam_game_coordinator.cpp` 的直接语法检查会受工作区缺失生成头和第三方 include 影响，当前环境里可见的缺口包括 `json/json.hpp` 与生成的 protobuf 头。
