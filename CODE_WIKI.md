# Code Wiki — Goldberg Emulator Fork (gbe_fork)

> 本文档是对本仓库（`gbe_fork`，Goldberg Emulator 的一个 Fork）的结构化代码 Wiki，覆盖项目整体架构、模块职责、关键类与函数、依赖关系以及构建运行方式等关键信息。

---

## 目录

1. [项目概述](#1-项目概述)
2. [整体架构](#2-整体架构)
3. [目录结构与模块职责](#3-目录结构与模块职责)
4. [核心模块详解](#4-核心模块详解)
   - 4.1 [入口与 Steam API 导出层](#41-入口与-steam-api-导出层)
   - 4.2 [Steam_Client 中枢](#42-steam_client-中枢)
   - 4.3 [Steam 接口实现族](#43-steam-接口实现族)
   - 4.4 [Settings 配置模型](#44-settings-配置模型)
   - 4.5 [本地存储 Local_Storage](#45-本地存储-local_storage)
   - 4.6 [网络层 Networking](#46-网络层-networking)
   - 4.7 [回调系统 CallSystem](#47-回调系统-callsystem)
   - 4.8 [GBE / Dota2 Game Coordinator 模块群](#48-gbe--dota2-game-coordinator-模块群)
   - 4.9 [Overlay 实验性覆盖层](#49-overlay-实验性覆盖层)
   - 4.10 [辅助库与工具链](#410-辅助库与工具链)
5. [依赖关系](#5-依赖关系)
6. [构建与运行方式](#6-构建与运行方式)
7. [关键设计约定](#7-关键设计约定)

---

## 1. 项目概述

**项目性质**：本项目是 [Goldberg Emulator](https://gitlab.com/Mr_Goldberg/goldberg_emulator) 的一个 Fork（位于 `https://github.com/Detanup01/gbe_fork`，本仓库为其再 Fork）。

**核心目标**：实现一个**无需 Steam 客户端即可运行的 Steamworks SDK 模拟器**，让游戏通过替换 `steam_api(64).dll` / `libsteam_api.so`，在没有真实 Steam 后端的情况下完成 Steamworks API 调用（成就、存档、好友、大厅、网络、UGC、控制器等），并支持局域网（LAN）多人联机。

**Fork 的主要增量**：在本 Fork 中额外加入了一套**完整的 Dota 2 Game Coordinator（GC）离线/局域网模拟层**，使 Dota 2 可以在没有真实 GC 的情况下完成练习大厅的创建/加入/启动、聊天频道、自定义游戏（街机）、SourceTV 等流程。该层由 `dll/gbe_*.cpp/.h` 一组纯函数式模块构成，并辅以对 `steamnetworkingsockets.dll` 的运行时内存补丁（见 [DOTA2_LAN_PATCH.md](file:///workspace/DOTA2_LAN_PATCH.md)）。

**许可证**：LGPL v3（见 [LICENSE](file:///workspace/LICENSE)）。

**目标平台**：Windows（x86 / x64）与 Linux（x86 / x64）。Windows 上支持 MSVC（VS 2022/2026）与实验性 MSYS2/MinGW；Linux 上使用 GCC/Clang。

**构建系统**：[Premake 5](https://premake.github.io/)，主脚本 [premake5.lua](file:///workspace/premake5.lua)，依赖构建脚本 [premake5-deps.lua](file:///workspace/premake5-deps.lua)。

---

## 2. 整体架构

### 2.1 分层概览

模拟器在游戏进程中以一个或多个动态库的形式存在，整体分为以下层次（自上而下）：

```
┌──────────────────────────────────────────────────────────────────────┐
│  游戏 (.exe)                                                          │
│   ├─ 直接调用 SteamAPI_*（steam_api.h）                                │
│   ├─ 调用 SteamGameServer_*（steam_gameserver.h）                      │
│   └─ 通过 CreateInterface("SteamClientXXX") 等显式获取接口              │
└───────────────┬──────────────────────────────────────────────────────┘
                │ 动态链接 / 注入
┌───────────────▼──────────────────────────────────────────────────────┐
│  模拟器 DLL（steam_api(64).dll / libsteam_api.so）                     │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ 导出层  dll.cpp / flat.cpp / wrap.cpp                           │  │
│  │  · SteamAPI_Init / SteamAPI_Shutdown / RunCallbacks             │  │
│  │  · SteamInternal_CreateInterface / CreateInterface              │  │
│  │  · 旧接口分发（create_client_interface）                          │  │
│  │  · flat C API（SteamAPI_ISteamClient_*）                         │  │
│  └───────────────┬────────────────────────────────────────────────┘  │
│  ┌───────────────▼────────────────────────────────────────────────┐  │
│  │ 中枢  Steam_Client（steam_client.h/.cpp）                        │  │
│  │  · 持有所有 ISteam* 接口实现实例                                  │  │
│  │  · 持有 Settings / Local_Storage / Networking / Overlay          │  │
│  │  · 后台线程驱动 network->Run() 与 RunEveryRunCB                  │  │
│  │  · RunCallbacks() 回调派发                                        │  │
│  └───────────────┬────────────────────────────────────────────────┘  │
│  ┌───────────────▼────────────────────────────────────────────────┐  │
│  │ 接口实现  steam_user / steam_friends / steam_apps / ...         │  │
│  │  · 每个实现多继承 ISteamXXX001 ~ ISteamXXX 当前版本              │  │
│  │  · 通过 Settings 读写配置、Local_Storage 落盘                    │  │
│  │  · 通过 Networking 收发 Common_Message                           │  │
│  │  · 通过 SteamCallBacks/SteamCallResults 投递回调                  │  │
│  └───────────────┬────────────────────────────────────────────────┘  │
│  ┌───────────────▼────────────────────────────────────────────────┐  │
│  │ GBE Dota2 GC 层  gbe_*.cpp/.h（纯函数 + DTO）                     │  │
│  │  · 手写 protobuf wire 格式（gbe_proto_wire）                      │  │
│  │  · 大厅状态机 / 启动流程 / 自定义游戏                             │  │
│  │  · GC 消息构造（gbe_gc_message_utils）                            │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
                │ UDP/TCP 广播 + 协议
┌───────────────▼──────────────────────────────────────────────────────┐
│  局域网内其它同样使用本模拟器的游戏实例                                 │
└──────────────────────────────────────────────────────────────────────┘
```

### 2.2 核心运行模型

1. **加载**：游戏加载 `steam_api(64).dll`（或经 ColdClientLoader 注入 `steamclient(64).dll`）。
2. **初始化**：游戏调用 `SteamAPI_Init()` → [dll.cpp](file:///workspace/dll/dll.cpp) 中通过 `get_steam_client()` 懒构造唯一的 `Steam_Client` 单例，创建 `user_steam_pipe` 并 `ConnectToGlobalUser`。
3. **接口获取**：游戏通过 `SteamFriends()`、`SteamUser()` 等 accessor，或 `SteamInternal_CreateInterface("SteamClientXXX")` 取得接口指针。所有版本（001~当前）由同一个 `Steam_Client*` 通过 `static_cast` 多重继承分发。
4. **后台线程**：`Steam_Client` 构造时启动 `KillableWorker` 后台线程，每隔最多 300ms（`max_stall_ms`）执行一次 `network->Run()` 与 `run_every_runcb->run()`，以驱动网络收发与周期回调。
5. **回调派发**：游戏调用 `SteamAPI_RunCallbacks()` → `Steam_Client::RunCallbacks(true,true)` → 各接口注册的 `SteamCallBacks`/`SteamCallResults` 派发。
6. **关闭**：`SteamAPI_Shutdown()` → 释放 user/pipe → `BShutdownIfAllPipesClosed()` → 当 init 计数归零时 `destroy_client()` 销毁 `Steam_Client`。

---

## 3. 目录结构与模块职责

```
workspace/
├── dll/                      # ★ 模拟器核心：所有 ISteam* 接口实现 + 导出层 + GBE/Dota2 模块
│   ├── dll/                  #   头文件 (.h)
│   ├── *.cpp                 #   实现
│   ├── gc_steam/             #   Steam GC 的 .proto 定义
│   ├── gc_tf2/               #   TF2 GC 的 .proto 定义
│   └── net.proto             #   模拟器自有的 LAN 网络协议定义
├── sdk/steam/                # Valve Steamworks SDK 头文件（实现契约）
├── libs/                     # 第三方库（detours, imgui, json, simpleini, stb, utfcpp, ...）
├── helpers/                  # 通用辅助（common_helpers, dbg_log, pe_helpers）
├── crash_printer/            # 崩溃捕获（win / linux）
├── overlay_experimental/     # 实验性游戏内覆盖层（基于 ImGui）
├── game_overlay_renderer_lib/# GameOverlayRenderer 占位库（兼容性）
├── networking_sockets_lib/   # steamnetworkingsockets 独立库转发
├── steam_old_lib/            # 旧版 Steam.dll 兼容库
├── steamclient/              # steamclient.dll 转发 shim
├── tools/                    # 工具集（generate_interfaces, lobby_connect, ColdClientLoader, migrate_gse, gc 测试）
├── resources/                # Windows 资源文件 (.rc)
├── post_build/               # 部署示例与说明文档
├── tests/                    # 测试
├── dev.notes/                # 开发笔记
├── z_original_repo_files/    # 上游原始仓库文件存档
├── .github/workflows/        # CI 构建
├── premake5.lua              # 主构建脚本
├── premake5-deps.lua         # 第三方依赖构建脚本
├── build_*.sh / build_*.bat  # 便捷构建脚本
├── package_*.sh / package_*.bat # 打包脚本
└── README.md / CHANGELOG.md / CREDITS.md / DEBUG_GUIDE.md / *.md  # 文档
```

各顶层目录职责见 [第 4 节](#4-核心模块详解)。

---

## 4. 核心模块详解

### 4.1 入口与 Steam API 导出层

文件：[dll/dll.cpp](file:///workspace/dll/dll.cpp)、[dll/flat.cpp](file:///workspace/dll/flat.cpp)、[dll/wrap.cpp](file:///workspace/dll/wrap.cpp)、[dll/dll/dll.h](file:///workspace/dll/dll/dll.h)。

`dll.cpp` 是模拟器的 C 导出入口，集中实现 `steam_api.h` 中所有 `STEAMAPI_API` 函数：

| 关键函数 | 职责 |
|---|---|
| `SteamAPI_Init()` | 创建 `Steam_Client`（懒构造）、建 pipe、`ConnectToGlobalUser`，递增生命周期计数 |
| `SteamAPI_Shutdown()` | 释放 user/pipe，计数归零时 `destroy_client()` |
| `SteamAPI_RunCallbacks()` | 调用 `get_steam_client()->RunCallbacks(true,true)` |
| `SteamGameServer_Init*()` | 初始化服务端 pipe/user |
| `SteamInternal_CreateInterface(ver)` | 核心分发：`create_client_interface(ver)` 按版本字符串返回对应 `ISteamXXX` 接口指针 |
| `SteamInternal_ContextInit` | 维护 `CSteamAPIContext` 的失效/重建（支持游戏反复 Init/Shutdown） |
| `SteamFriends()/SteamUser()/...` | 一组 accessor，返回 `Steam_Client` 持有的对应实例 |

**接口版本分发**：`create_client_interface(ver)`（[dll.cpp](file:///workspace/dll/dll.cpp)）对 `"SteamClient001"~"SteamClient022"` 等做字符串匹配，返回 `static_cast<ISteamClient0XX*>(client_ptr)`。由于 `Steam_Client` 多重继承了**所有历史版本**接口，单一实例即可服务任意 SDK 版本。其它接口（User/Friends/...）的分发在 [dll/steam_client_interface_getter.cpp](file:///workspace/dll/steam_client_interface_getter.cpp) 中，每个 `GetISteamXxx` 按版本字符串返回对应实现。

**flat.cpp**：实现 `steam_api_flat.h` 的扁平 C API（`SteamAPI_ISteamClient_CreateSteamPipe` 等），供非 C++ 调用方使用；仅在非 `STEAMCLIENT_DLL` 构建时编译。

**wrap.cpp**：用于 `steamclient` 模式的包装（见 [4.10](#410-辅助库与工具链)）。

**旧接口兼容**：`load_old_steam_interfaces()` 从 `steam_settings/steam_interfaces.txt` 读取游戏实际期望的接口版本字符串，写入一组 `old_*` 全局变量，使 `SteamClient017` 等旧式 accessor 返回正确版本。

**生命周期计数**：`steam_lifetime_counters` 跟踪 init/deinit，并通过 `context_counter` 触发 `CSteamAPIContext` 重建，解决部分游戏（如 appid 1449110）反复 Init/Shutdown 的问题。

### 4.2 Steam_Client 中枢

文件：[dll/dll/steam_client.h](file:///workspace/dll/dll/steam_client.h)、[dll/steam_client.cpp](file:///workspace/dll/steam_client.cpp)。

`Steam_Client` 是模拟器的**单例中枢**，多重继承自 `ISteamClient001 ~ ISteamClient022` 与 `ISteamClient`。其职责：

- **持有所有子系统**：构造时（[steam_client.cpp](file:///workspace/dll/steam_client.cpp#L63)）依次 `new` 出 `Settings`(client/server)、`Local_Storage`、`Networking`、`SteamCallResults`(client/server)、`SteamCallBacks`(client/server)、`RunEveryRunCB`、`Steam_Overlay`，以及所有 `Steam_User/Friends/Utils/Matchmaking/...` 客户端与服务端接口实现。
- **接口获取**：每个 `GetISteamXxx(hSteamUser,hSteamPipe,ver)` 验证 pipe 与 user 后，按 `ver` 返回对应实例（服务端 pipe 返回 `steam_gameserver_*` 实例）。
- **Pipe/User 管理**：`Reusable_Numbers<uint32>` 复用 pipe/user 句柄号；`steam_pipes` 映射 pipe→`Steam_Pipe{type}`；`old_user_refs` 支持旧版 SDK 的 `CreateGlobalInstance` 等接口。
- **后台线程**：`background_thread`（`KillableWorker`）以 `max_stall_ms=300ms` 轮询，若距上次 `RunCallbacks` 超过该阈值则触发 `network->Run()` + `run_every_runcb->run()`，并更新 `last_cb_run`。这保证即使游戏不调用 `SteamAPI_RunCallbacks()`，网络与周期任务也能推进。
- **登录/初始化状态**：`userLogIn()/serverInit()/clientShutdown()/serverShutdown()` 维护 `user_logged_in`/`server_init` 标志，控制 `SteamInternal_CreateInterface` 是否放行。
- **回调注册**：`RegisterCallback/UnregisterCallback/RegisterCallResult/UnregisterCallResult` 转发到 `callbacks_client/server` 与 `callback_results_client/server`，并维护 `old_callbacks_map` 以兼容 `using_old_callbacks`（Friends ≤ 004 的旧式回调）。
- **Overlay 生命周期**：在首个客户端 user 连接时 `steam_overlay->SetupOverlay()`，关闭时 `UnSetupOverlay()`。
- **缺失接口报告**：`report_missing_impl(itf,caller)` / `report_missing_impl_and_exit(...)` 在遇到未实现的接口版本时打印警告或中止。

关键公开成员指针（[steam_client.h](file:///workspace/dll/dll/steam_client.h#L128-L196)）：`network`、`settings_client`/`settings_server`、`local_storage`、`run_every_runcb`、`ugc_bridge`、所有 `steam_*` 接口实例、`steam_overlay`、`playtime_counter`。

### 4.3 Steam 接口实现族

目录：[dll/](file:///workspace/dll)，每个 `steam_*.cpp/.h` 对应一个 Steamworks 接口。

**统一模式**：每个实现类多重继承该接口的所有历史版本（如 `Steam_User` 继承 `ISteamUser001..ISteamUser022, ISteamUser`），构造函数接收一组核心依赖指针：

```cpp
Steam_User(Settings *settings, Local_Storage *local_storage,
           Networking *network, SteamCallResults *callback_results,
           SteamCallBacks *callbacks, bool is_server);
```

依赖注入使各接口能访问配置、存储、网络与回调，而无需全局状态。`is_server` 区分客户端实例与游戏服务端实例。

**代表性实现**（共 50+ 个）：

| 文件 | 类 | 接口 | 职责 |
|---|---|---|---|
| [steam_user.cpp](file:///workspace/dll/steam_user.cpp) | `Steam_User` | `ISteamUser001~022` | 登录态、SteamID、AppTicket、Auth、语音、注册表 |
| [steam_friends.cpp](file:///workspace/dll/steam_friends.cpp) | `Steam_Friends` | `ISteamFriends001~017` | 好友列表、富状态、邀请、overlay 联动 |
| [steam_user_stats.cpp](file:///workspace/dll/steam_user_stats.cpp) | `Steam_User_Stats` | `ISteamUserStats001~012` | 成就/统计/排行榜，含 achievements/leaderboard/stats 子模块 |
| [steam_matchmaking.cpp](file:///workspace/dll/steam_matchmaking.cpp) | `Steam_Matchmaking` | `ISteamMatchmaking001~008` | 大厅创建/加入/搜索/数据 |
| [steam_networking_sockets.cpp](file:///workspace/dll/steam_networking_sockets.cpp) | `Steam_Networking_Sockets` | `ISteamNetworkingSockets001~011` | P2P/虚电路 socket |
| [steam_game_coordinator.cpp](file:///workspace/dll/steam_game_coordinator.cpp) | `Steam_Game_Coordinator` | `ISteamGameCoordinator` | GC 消息收发，集成 GBE Dota2 层（见 [4.8](#48-gbe--dota2-game-coordinator-模块群)） |
| [steam_apps.cpp](file:///workspace/dll/steam_apps.cpp) | `Steam_Apps` | `ISteamApps001~008` | DLC/应用安装/语言 |
| [steam_remote_storage.cpp](file:///workspace/dll/steam_remote_storage.cpp) | `Steam_Remote_Storage` | `ISteamRemoteStorage001~015` | 云存档、UGC |
| [steam_controller.cpp](file:///workspace/dll/steam_controller.cpp) | `Steam_Controller` | `ISteamController001~007`/`ISteamInput` | 控制器/Steam Input |
| [steam_ugc.cpp](file:///workspace/dll/steam_ugc.cpp) | `Steam_UGC` | `ISteamUGC001~020` | 创意工坊 |
| [steam_inventory.cpp](file:///workspace/dll/steam_inventory.cpp) | `Steam_Inventory` | `ISteamInventory001~002` | 物品库存 |
| [steam_http.cpp](file:///workspace/dll/steam_http.cpp) | `Steam_HTTP` | `ISteamHTTP001~002` | HTTP 请求（可选真实下载） |
| [steam_gameserver.cpp](file:///workspace/dll/steam_gameserver.cpp) | `Steam_GameServer` | `ISteamGameServer001~014` | 游戏服务端 |

完整列表见 [dll/dll/](file:///workspace/dll/dll) 目录下 `steam_*.h`。`Steam_Client` 同时持有客户端实例（`steam_*`）与服务端实例（`steam_gameserver_*`），二者共享 `settings_server` 与 `network`。

### 4.4 Settings 配置模型

文件：[dll/dll/settings.h](file:///workspace/dll/dll/settings.h)、[dll/settings.cpp](file:///workspace/dll/settings.cpp)、[dll/dll/settings_parser.h](file:///workspace/dll/dll/settings_parser.h)、[dll/settings_parser.cpp](file:///workspace/dll/settings_parser.cpp)、[dll/settings_parser_ufs.cpp](file:///workspace/dll/settings_parser_ufs.cpp)。

`Settings` 类承载**单次运行的所有可配置状态**：用户身份（`steam_id`、`game_id`、`name`、`language`）、网络（`port`、`disable_networking`、`custom_broadcasts`）、DLC、Mods、排行榜、统计、成就行为开关、Overlay 外观与开关、分支信息、控制器映射等。

**两套实例**：`Steam_Client` 维护 `settings_client`（用户侧）与 `settings_server`（游戏服务端侧），二者 SteamID 不同但配置同源。

**配置加载流程**（`create_localstorage_settings`，[settings_parser.cpp](file:///workspace/dll/settings_parser.cpp)）：
1. 解析 `steam_appid.txt`（或环境变量 `SteamAppId`）得到 appid。
2. 创建 `Local_Storage`，从全局 `GSE Saves` 与本地 `steam_settings` 读取配置。
3. 读取 `.ini`（`configs.main/user/app/overlay.ini`，经 SimpleIni）、`achievements.json`、`stats.txt`、`items.json`、`leaderboards.txt`、`mods.json`、`dlc.txt`、`depots.txt`、`steam_interfaces.txt` 等。
4. 返回 appid 并输出 `settings_client`/`settings_server`/`local_storage`。

**关键数据结构**（[settings.h](file:///workspace/dll/dll/settings.h)）：`DLC_entry`、`Mod_entry`、`Leaderboard_config`、`Stat_config`、`Image_Data`、`Controller_Settings`、`Overlay_Appearance`、`Branch_Info`。

`Overlay_Appearance` 提供丰富的 Overlay 视觉参数（颜色、圆角、动画时长、通知位置枚举 `NotificationPosition`）。

### 4.5 本地存储 Local_Storage

文件：[dll/dll/local_storage.h](file:///workspace/dll/dll/local_storage.h)、[dll/local_storage.cpp](file:///workspace/dll/local_storage.cpp)。

`Local_Storage` 负责**所有持久化**：云存档、统计、排行榜、截图、库存、设置。它定义了一组命名子目录（`remote`、`settings`、`stats`、`leaderboard`、`local`、`screenshots`、`steam_settings`、`inventory`），并提供：

- 数据读写：`store_data/get_data(folder,file,...)`、`file_exists/file_size/file_delete/file_timestamp`、`iterate_file`。
- JSON 读写：`load_json/load_json_file/write_json_file`（基于 `nlohmann::json`）。
- 图像：`load_image`、`load_image_resized`（基于 stb_image）、`save_screenshot`。
- 路径解析：`get_user_appdata_path`（`%appdata%\GSE Saves\` 或 `$XDG_DATA_HOME/GSE Saves/`）、`get_game_settings_path`、`set_saves_folder_name`（可重命名基础目录以实现按游戏隔离）。
- 文件名净化：`sanitize_string`/`desanitize_string`。

### 4.6 网络层 Networking

文件：[dll/dll/network.h](file:///workspace/dll/dll/network.h)、[dll/network.cpp](file:///workspace/dll/network.cpp)、协议定义 [dll/net.proto](file:///workspace/dll/net.proto)。

`Networking` 类实现模拟器自己的 **LAN P2P 协议**，使多个模拟器实例相互发现并交换 `Common_Message`。协议基于 UDP 广播 + TCP，默认端口 `47584`。

**消息信封** `Common_Message`（[net.proto](file:///workspace/dll/net.proto#L403)）：含 `source_id`/`dest_id`（SteamID64）与一个 `oneof messages`，覆盖所有需要跨实例同步的子系统：

| 子消息 | 用途 |
|---|---|
| `Announce` | 节点发现/心跳（PING/PONG + 携带已知 peer 列表） |
| `Low_Level` | HEARTBEAT/CONNECT/DISCONNECT |
| `Lobby` / `Lobby_Messages` | 大厅状态与成员事件 |
| `Network_pb` / `Network_Old` / `Networking_Sockets` / `Networking_Messages` | 各代网络接口的数据帧 |
| `Gameserver` | 游戏服务端广播（供服务器浏览器） |
| `Friend` / `Friend_Messages` / `Steam_Messages` | 好友状态与聊天/邀请 |
| `Auth_Ticket` | 鉴权票据同步 |
| `GameServerStats_Messages` | 服务端↔用户统计同步 |
| `Leaderboards_Messages` | 排行榜分数共享 |
| `Steam_User_Stats_Messages` | 用户统计共享 |
| `GameServer_Items_Messages` | 库存物品同步 |

**回调分类** `Callback_Ids`（[network.h](file:///workspace/dll/dll/network.h#L57)）：`CALLBACK_ID_USER_STATUS/LOBBY/NETWORKING/GAMESERVER/FRIEND/...`，各接口通过 `setCallback(id, steam_id, fn, object)` 订阅。

**关键 API**：`Run()`（收发循环）、`sendTo/sendToAllIndividuals/sendToAllGameservers/sendToAll`、`addListenId`、`setAppID`、`startQuery`（Source 协议查询，供 `Steam_Matchmaking_Servers`）、`getOwnIP/getIP/getPort`。

`disable_networking` 为 true 时 `Networking` 不创建 socket，纯本地运行。

### 4.7 回调系统 CallSystem

文件：[dll/dll/callsystem.h](file:///workspace/dll/dll/callsystem.h)、[dll/callsystem.cpp](file:///workspace/dll/callsystem.cpp)。

模拟器复刻了 Steamworks 的回调/CallResult 机制：

- `CCallbackMgr`：标记 `CCallbackBase` 的注册/注销与服务端标志。
- `Steam_Call_Result`：单条 CallResult（`SteamAPICall_t`、回调对象列表、结果数据、超时、`iCallback`）。
- `SteamCallResults`：管理所有 CallResult 的队列，`addCallResult/addCallBack/callback_result/rmCallBack/runCallResults`，超时常量 `STEAM_CALLRESULT_TIMEOUT=120s`。
- `SteamCallBacks`：按 `iCallback` 分组的普通回调队列，`addCallBack/addCBResult/runCallBacks`，关联到 `SteamCallResults`。
- `RunEveryRunCB`：周期回调注册表（`add/remove/run`），供需要每帧执行的子系统（如 overlay、成就图标加载、playtime）注册。

`Steam_Client` 持有 `callback_results_client/server` 与 `callbacks_client/server`，分别服务客户端/服务端 pipe。

### 4.8 GBE / Dota2 Game Coordinator 模块群

这是本 Fork 的**核心增量**，位于 [dll/gbe_*.cpp/.h](file:///workspace/dll)。这是一组**纯函数 + 值类型 DTO** 的库，用于在不依赖真实 Dota 2 GC 的情况下，合成、解析、改写 Dota 2 GC 的 protobuf 消息，使练习大厅创建/加入/启动、聊天频道、自定义游戏、SourceTV 等流程得以离线/局域网运行。

#### 4.8.1 两层机制

1. **网络补丁层**（[dll/dll/networking_patch.h](file:///workspace/dll/dll/networking_patch.h)，见 [DOTA2_LAN_PATCH.md](file:///workspace/DOTA2_LAN_PATCH.md)）：在 `DllMain(DLL_PROCESS_ATTACH)` 调用 `NetworkingPatch::ApplyAll()`，后台线程等待 `steamnetworkingsockets.dll` 加载后，搜索特征字节 `84 DB 75 11 48 8B 40 20` 并将 `JNZ(0x75)` 改为 `JZ(0x74)`，使 `IP_AllowWithoutAuth` 等 dev 配置在 release 可见。同时在 `Steam_Networking_Utils` 构造函数中设置 `g_gbe_ip_allow_without_auth=1` 等，允许无认证连接。
2. **GC 模拟层**（本节）：手写 protobuf wire 格式，构造/改写 GC 消息。

#### 4.8.2 模块依赖图

```
gbe_proto_wire  (基础：protobuf wire 格式 + Dota 请求解析 + 模板补丁)
   ^   ^   ^   ^   ^
   |   |   |   |   +-- gbe_gc_message_utils   (构造所有出站 Dota GC 消息)
   |   |   |   +------ gbe_dota_gc_wire        (改写大厅模板对象)
   |   |   +---------- gbe_dota_custom_game    (街机/自定义游戏辅助)
   |   +-------------- gbe_dota_lobby_state    (中心状态机 + 计划)
   |                     |   |
   |                     |   +-- gbe_dota_lobby_snapshot
   |                     +-- gbe_dota_lobby_flow  <-- gbe_dota_lobby_publish
   |
gbe_dota_types  (共享 DTO)
gbe_dota_gc_router  --> gbe_gc_message_utils, gbe_proto_wire   (包装消息路由)
gbe_gc_config       (独立 JSON profile 解析)
gbe_dota_custom_lobby_http  --> gbe_dota_types, json           (HTTP JSON 适配)
```

#### 4.8.3 各模块职责

| 模块 | 命名空间 | 职责 |
|---|---|---|
| [gbe_proto_wire](file:///workspace/dll/gbe_proto_wire.h) | `gbe::proto_wire` | **基础层**：手写 protobuf wire 读写（`read_varuint`/`parse_fields`/`append_*`）、模板补丁（`patch_dota_lobby_template_identifiers`、`patch_dota_practice_lobby_launch_template`）、递归 varint 改写、FNV-1a 64 哈希、连接串规整（`normalize_dota_practice_lobby_connect*`，默认端口 27015）、Dota 请求解析（`parse_dota_practice_lobby_set_details_body` 等）、诊断格式化 |
| [gbe_gc_message_utils](file:///workspace/dll/gbe_gc_message_utils.h) | `gbe::gc_message` | **最大模块**：Dota GC 消息构造套件。常量 `kProtoMask=0x80000000`、`kGcInvitationCreated=4502`、`kDotaPracticeLobbyDetailsUpdate=26` 等。提供 `build_dota_lobby_object_2004`、`build_dota_practice_lobby_cache_subscribed_payload_from_objects`、`build_dota_chat_message_payload`(7273)、`build_dota_lobby_list_response_payload`(8012) 等 ~80 个构造函数；以及 ID 生成器 `generate_dota_so_change_version`、`generate_dota_lobby_invite_gid` |
| [gbe_dota_gc_wire](file:///workspace/dll/gbe_dota_gc_wire.h) | `gbe::dota_gc_wire` | protobuf "改写器"：逐字段重写已序列化的大厅模板子对象（2004/2014/2015/2016 及成员子对象），用于采纳其它 peer 的大厅快照。`rewrite_dota_lobby_template_object_2004` 是核心，覆盖大厅对象 2004 的完整字段映射 |
| [gbe_dota_gc_router](file:///workspace/dll/gbe_dota_gc_router.h) | `gbe::dota_gc_router` | 路由登录后"包装"式 GC 请求：`extract_wrapped_post_login_request` 剥两层信封取出内层 emsg/body/job-id；`build_outbound_message` 重建出站包装/非包装消息。包装 emsg 白名单由 `gc_message::is_supported_dota_wrapped_post_login_request` 守门 |
| [gbe_dota_lobby_state](file:///workspace/dll/gbe_dota_lobby_state.h) | `gbe::dota_lobby_state` | **中心状态模型**：定义 `GBE_LocalLobby`/`GBE_SharedDotaLobbyState` 及一族"计划"结构（`CreateLobbyPlan`/`JoinLobbyMergePlan`/`LaunchInitPlan`/`LaunchRunPlan`/`QueuedLobbyStateApplyPlan` 等）。纯函数 `compose_*_plan` 由 GC handler 调用计算计划，再 apply。`publish_local_lobby_to_shared`/`adopt_shared_lobby_to_local` 处理本地↔共享状态同步，`build_reconnect_context` 构建重连上下文 |
| [gbe_dota_lobby_flow](file:///workspace/dll/gbe_dota_lobby_flow.h) | `gbe::dota_lobby_flow` | 大厅成员与流程逻辑：成员增删查改、远程成员计数与 LAN 启动门控（`should_hold_lan_launch_for_remote_members`）、聊天显示名解析、owner 转移、街机槽位规整（`normalize_arcade_lobby_member_slots`，owner 槽 1、其余 2..10） |
| [gbe_dota_lobby_publish](file:///workspace/dll/gbe_dota_lobby_publish.h) | `gbe::dota_lobby_flow` | 将强类型大厅状态转为**字符串化发布 DTO**（`compose_lobby_member_publish_data` 等），供广播给 peer |
| [gbe_dota_lobby_snapshot](file:///workspace/dll/gbe_dota_lobby_snapshot.h) | `gbe::dota_lobby_flow` | 从 GC 消息构建**强类型快照 DTO**（`compose_generic_lobby_snapshot_scalar_data` 强制 `lan=true`）并 apply 成员快照 |
| [gbe_dota_custom_game](file:///workspace/dll/gbe_dota_custom_game.h) | `gbe::dota_custom_game` | 街机/自定义游戏辅助：详情比较、显示名、从 IPv4 派生匿名游戏服务器 SteamID64、识别 guide-only mod、构造可加入大厅条目 |
| [gbe_dota_custom_lobby_http](file:///workspace/dll/gbe_dota_custom_lobby_http.h) | `gbe::dota_custom_lobby_http` | 单函数适配器：将可加入自定义大厅数据转为 `nlohmann::json`（64 位 id 以十进制字符串输出） |
| [gbe_gc_config](file:///workspace/dll/gbe_gc_config.h) | `gbe::gc_config` | 解析 `gc.json` 选择 GC profile（`enum Profile{Tf2,Dota2}`），含 `DotaFallbackReason` 与 appid 兜底（appid==570 强制 Dota2） |
| [gbe_dota_types](file:///workspace/dll/gbe_dota_types.h) | 全局命名空间 | 头文件 DTO 集合：`GBE_DotaCustomGameDetails`、`GBE_DotaLobbyMemberState`、各 `*PublishData`/`*SnapshotData`/`*SnapshotInput` 等 |

#### 4.8.4 关键协议约定

- GC 消息帧：`[u32 emsg][u32 header_len][header bytes][body bytes]`，`emsg` 带 `kProtoMask=0x80000000` proto 位。
- SOID owner type：`3`=lobby、`4`=user。
- 大厅对象类型 ID：`2002`(账号行为)、`2004`(大厅)、`2011`(大厅邀请)、`2012`(Dota Plus)、`2013`(服务端大厅占位)、`2014`(静态大厅名)、`2015`(服务端大厅启动)、`2016`(服务端静态大厅)。
- 大厅状态值：`1`=预运行/READYUP、`2`=RUN、`3`=POSTGAME、`4`=自定义游戏 READYUP。
- 默认 `server_region=15`、`game_mode=2`、`bot_difficulty_dire=4`、owner 槽 `1`、默认端口 `27015`、loopback `127.0.0.1:27015`。
- 快照中 `lan` 强制为 true（本 Fork 总按 LAN 处理）。

#### 4.8.5 集成点

`Steam_Game_Coordinator`（[dll/dll/steam_game_coordinator.h](file:///workspace/dll/dll/steam_game_coordinator.h)）持有 `pending_messages`/`incoming_messages` 队列与 `GBE_LocalLobby` 状态，调用上述纯函数计算计划并 apply，再通过 `gbe_gc_message_utils` 构造出站 GC 消息回给游戏。GC profile 由 `gbe_gc_config::parse_gc_config` 在初始化时从 `steam_settings/gc.json` 决定。

### 4.9 Overlay 实验性覆盖层

目录：[overlay_experimental/](file:///workspace/overlay_experimental)。仅当 `EMU_OVERLAY` 宏定义时编译真实实现，否则 `Steam_Overlay` 退化为同名空方法 stub。

- [steam_overlay.h](file:///workspace/overlay_experimental/overlay/steam_overlay.h)：`Steam_Overlay` 类，基于 `InGameOverlay`（ImGui + 渲染器 hook）子模块。持有到 `Settings/Local_Storage/SteamCallResults/SteamCallBacks/RunEveryRunCB/Networking` 的反向引用，以及 `Steam_Overlay_Stats`。
- 职责：渲染器检测/hook 生命周期、主窗口 + 好友列表/聊天窗口、通知队列与动画/音效（内置 WAV，[notification.h](file:///workspace/overlay_experimental/overlay/notification.h)）、成就图标异步加载、风格设置。
- 公共 API：`Ready/NeedPresent/ShowOverlay/SetupOverlay/UnSetupOverlay/SetNotificationPosition/SetNotificationInset/OpenOverlayInvite/OpenOverlay/SetLobbyInvite/SetRichInvite/FriendConnect/FriendDisconnect/AddAchievementNotification`。
- [steam_overlay_stats.h](file:///workspace/overlay_experimental/overlay/steam_overlay_stats.h)：`Steam_Overlay_Stats` 实时 FPS/帧时间/游戏时长。
- [steam_overlay_translations.h](file:///workspace/overlay_experimental/overlay/steam_overlay_translations.h)：31 种语言本地化表。
- **集成**：`Steam_Client` 构造 `steam_overlay` 并注入到 `Steam_Friends/Steam_Utils/Steam_User_Stats`，使这些接口的 overlay 相关调用（如 `ActivateGameOverlay`、`BOverlayNeedsPresent`、成就通知）委托给它。

### 4.10 辅助库与工具链

#### Helpers（[helpers/](file:///workspace/helpers)）
- [common_helpers](file:///workspace/helpers/common_helpers/common_helpers.hpp)：跨平台工具（字符串、文件系统、`KillableWorker` 后台线程、`ForgettableMemory` TTL 容器）。
- [dbg_log](file:///workspace/helpers/dbg_log/dbg_log.hpp)：线程安全文件日志，`PRINT_DEBUG` 宏写入全局 `STEAM_LOG.txt`。
- [pe_helpers](file:///workspace/helpers/pe_helpers/pe_helpers.hpp)（Windows）：PE 头/内存扫描/远程 DLL 注入，供 ColdClientLoader 与 extra-protection 使用。
- [os_detector.h](file:///workspace/helpers/common_helpers/os_detector.h)：编译期 OS/架构检测宏。

#### Crash Printer（[crash_printer/](file:///workspace/crash_printer)）
`crash_printer::init(log_file)/deinit()` 跨平台安装崩溃处理器，写带时间戳的栈回溯。Windows 用 `SetUnhandledExceptionFilter` + DbgHelp；Linux 用 `sigaction` 捕获 SIGILL/SIGSEGV/SIGBUS + `backtrace`。

#### 第三方库（[libs/](file:///workspace/libs)）
每个子目录含 `SOURCE.txt` 记录上游与版本：`detours`(API hook)、`fifo_map`(有序 map)、`gamepad`(手柄抽象)、`imgui`(即时 GUI，源码经 InGameOverlay 子模块)、`json`(nlohmann/json v3.12)、`sha`(SHA-1)、`simpleini`(INI 解析)、`stb`(图像加载/缩放/写入)、`utfcpp`(UTF-8 转换)。

#### SDK（[sdk/steam/](file:///workspace/sdk/steam)）
Valve Steamworks SDK 头文件（v1.60），含所有 `isteam*.h` 接口及其历史版本（如 `isteamclient001~022.h`）。这是模拟器实现的**契约**，也是 `generate_interfaces` 工具正则匹配的目标。

#### 兼容性 shim
- [game_overlay_renderer_lib/](file:///workspace/game_overlay_renderer_lib)：`GameOverlayRenderer(64).dll/.so` 占位，满足部分游戏（如 appid 410900）对内存中存在该库的检查，导出 `BOverlayNeedsPresent`（返回 false）等。
- [networking_sockets_lib/](file:///workspace/networking_sockets_lib/steamnetworkingsockets.cpp)：独立 `steamnetworkingsockets.dll/.so`，重新导出 `ISteamNetworkingSockets/Utils` 与 SteamDatagram 系列，使按符号动态加载该模块的游戏解析到模拟器。
- [steam_old_lib/](file:///workspace/steam_old_lib)：旧版 `Steam.dll` 兼容（x86-only），用 Detours 钩子重定向到模拟器，供老游戏使用。
- [steamclient/steamclient.cpp](file:///workspace/steamclient/steamclient.cpp)：`steamclient(64).dll` 转发 shim，导出 `CreateInterface` 委托给 `steam_api` 的 `SteamInternal_CreateInterface`。

#### 工具（[tools/](file:///workspace/tools)）
| 工具 | 产物 | 用途 |
|---|---|---|
| [lobby_connect](file:///workspace/tools/lobby_connect/lobby_connect.cpp) | `lobby_connect_<arch>` | 控制台大厅浏览器，列出好友大厅供加入（用 sentinel appid `(uint32)-2`） |
| [generate_interfaces](file:///workspace/tools/generate_interfaces/generate_interfaces.cpp) | `generate_interfaces_<arch>` | 扫描游戏二进制，正则提取接口版本字符串生成 `steam_interfaces.txt` |
| [steamclient_loader](file:///workspace/tools/steamclient_loader)（win: `ColdClientLoader`，linux: `steamclient_loader.sh`） | `steamclient_loader_<arch>` | Cold Client Loader：在非 Steam 进程中伪造 Steam 环境并注入模拟器 DLL |
| [migrate_gse](file:///workspace/tools/migrate_gse) | Python 脚本 | 将旧格式 `steam_settings` 迁移到新 `.ini` 格式（`GSE Saves`） |
| [gc_replay_test](file:///workspace/tools/gc_replay_test) | `gc_replay_test_<arch>` | GC 消息夹具回放验证工具，输出稳定摘要作 golden file |
| `gc_message_utils_test`/`gbe_gc_config_test`/`gbe_proto_wire_test`/`gbe_dota_custom_game_test`/`gbe_dota_lobby_flow_test` | `<name>_<arch>` | 各 GBE 子模块的离线单元测试，由 [run_gc_offline_tests.sh](file:///workspace/tools/run_gc_offline_tests.sh) 统一驱动 |

---

## 5. 依赖关系

### 5.1 内部模块依赖

- **`Steam_Client`** 依赖所有 `Steam_*` 接口实现、`Settings`、`Local_Storage`、`Networking`、`SteamCallResults`、`SteamCallBacks`、`RunEveryRunCB`、`Steam_Overlay`、`PlaytimeCounter`、`Ugc_Remote_Storage_Bridge`。
- **`Steam_*` 接口实现** 依赖 `Settings`、`Local_Storage`、`Networking`、`SteamCallResults`、`SteamCallBacks`、`RunEveryRunCB`（按需）；部分依赖 `Steam_Overlay`（Friends/Utils/User_Stats）。
- **`Steam_Game_Coordinator`** 依赖 GBE Dota2 模块群（`gbe_dota_lobby_state` → `gbe_dota_gc_wire`/`gbe_dota_lobby_flow`/`gbe_dota_custom_game` → `gbe_proto_wire`，以及 `gbe_gc_message_utils`/`gbe_dota_gc_router`/`gbe_gc_config`）。
- **`Networking`** 依赖 protobuf（`Common_Message` 等）与 curl（用于 `Steam_HTTP` 真实下载）。
- **`Settings`** 由 `settings_parser` 从 `Local_Storage` 与 `.ini`/`.json`/`.txt` 构建。

### 5.2 第三方依赖（[premake5-deps.lua](file:///workspace/premake5-deps.lua)）

| 依赖 | 用途 |
|---|---|
| `curl` | HTTP 请求（`Steam_HTTP` 可选真实下载） |
| `protobuf` + `absl` | 编译 [net.proto](file:///workspace/dll/net.proto) 与 GC `.proto`（注意 GBE 手写 wire 层**不**依赖 protobuf 运行时，但 `Networking` 的 `Common_Message` 依赖） |
| `zlib` | curl 依赖 |
| `mbedtls` | TLS（curl HTTPS）与加密 |
| `opus` + `portaudio` | 语音聊天（`Steam_User` 的 `VoiceChat`） |
| `SDL3` | 控制器/手柄输入（`Steam_Controller`） |
| `libssq` | Source 服务器查询（`source_query.cpp`，供服务器浏览器） |
| `ingame_overlay` + `system` + `mini_detour` | Overlay 渲染器 hook（仅 experimental 构建） |
| `detours`（vendored [libs/detours](file:///workspace/libs/detours)） | API hook（experimental 与 steam_old_lib） |
| `imgui/json/simpleini/stb/utfcpp/fifo_map/sha`（vendored [libs/](file:///workspace/libs)） | GUI/配置/图像/编码/哈希 |

### 5.3 系统库
- Windows：`Ws2_32, Iphlpapi, Wldap32, Winmm, Bcrypt, Dbghelp, ntdll, Xinput, Gdi32, Dwmapi, OpenGL32`。
- Linux：`pthread, dl, X11`（仅 experimental overlay）。

---

## 6. 构建与运行方式

### 6.1 一次性环境准备

详见 [README.md](file:///workspace/README.md)。

- **克隆**：`git clone --recurse-submodules -j8 <repo>`，并定期 `git submodule update --init --recursive --remote`。
- **Windows**：VS 2022 Community + "Desktop development with C++" + 最新 Windows SDK；Python 3.10+。
- **Linux**：Ubuntu 22.04，安装 `build-essential gcc-multilib g++-multilib libglx-dev libgl-dev` + Python 3.10+。

### 6.2 构建第三方依赖（少见更新）

```
# Windows (VS)
set "CMAKE_GENERATOR=Visual Studio 18 2026"
third-party\common\win\premake\premake5.exe --file=premake5-deps.lua --64-build --32-build --all-ext --all-build --verbose --os=windows vs2026

# Linux
export CMAKE_GENERATOR="Unix Makefiles"
./third-party/common/linux/premake/premake5 --file=premake5-deps.lua --64-build --32-build --all-ext --all-build --verbose --os=linux gmake2
```

产物落到 `build/deps/<os>/`。

### 6.3 构建模拟器

```
# Windows (VS)
third-party\common\win\premake\premake5.exe --file=premake5.lua --genproto --os=windows vs2026
# 然后 msbuild build\project\vs2026\win\gbe.slnx /p:Configuration=release,Platform=Win32|x64

# Linux
./third-party/common/linux/premake/premake5 --file=premake5.lua --genproto --os=linux gmake2
cd build/project/gmake2/linux
make config=release_x32 -j 8 all
make config=release_x64 -j 8 all
```

`--genproto` 触发用 `protoc` 从 `.proto` 生成 `proto_gen/<os>/` 下的 C++ 源码。便捷脚本：[build_win_premake.bat](file:///workspace/build_win_premake.bat)、[build_linux_premake.sh](file:///workspace/build_linux_premake.sh)。

产物目录：`build/<os>/<toolchain>/<config>/`，含 `regular/`、`experimental/`、`steamclient_experimental/`、`tools/`、`steamnetworkingsockets/`、`gameoverlayrenderer/` 等子目录。

### 6.4 构建产物（[premake5.lua](file:///workspace/premake5.lua) 定义的项目）

| 项目 | 类型 | 产物名 | 说明 |
|---|---|---|---|
| `api_regular` | SharedLib | `steam_api`/`steam_api64`/`libsteam_api` | 标准模拟器 |
| `api_experimental` | SharedLib | 同上 | 带 Overlay（`EMU_OVERLAY`/`EMU_EXPERIMENTAL_BUILD`） |
| `steamclient_experimental` | SharedLib | `steamclient`/`steamclient64` | 实验性 steamclient |
| `steamclient_experimental_stub`（Win） | SharedLib | `steamclient(64)` | 转发 stub |
| `steamclient_experimental_extra`（Win） | SharedLib | `steamclient_extra_<arch>` | extra-protection DLL |
| `lib_steam_old`（Win, x86） | SharedLib | `Steam.dll` | 旧版兼容 |
| `lib_game_overlay_renderer` | SharedLib | `GameOverlayRenderer(64)`/`gameoverlayrenderer` | 占位 |
| `lib_steamnetworkingsockets` | SharedLib | `libsteamnetworkingsockets` | 转发 |
| `steamclient_regular`（Linux） | SharedLib | `steamclient` | Linux 标准 steamclient |
| `tool_lobby_connect` | ConsoleApp | `lobby_connect_<arch>` | 大厅浏览器 |
| `tool_generate_interfaces` | ConsoleApp | `generate_interfaces_<arch>` | 接口提取 |
| `tool_gc_replay_test` + `gc_message_utils_test` + `gbe_gc_config_test` + `gbe_proto_wire_test` + `gbe_dota_custom_game_test` + `gbe_dota_lobby_flow_test` | ConsoleApp | `<name>_<arch>` | GBE 子模块测试 |
| `steamclient_experimental_loader`（Win） | WindowedApp | `steamclient_loader_<arch>` | ColdClientLoader |
| `tool_file_dos_stub_changer`（Win） | ConsoleApp | `file_dos_stub_<arch>` | DOS stub 修改 |
| `test_crash_printer*` / `test_gamepad_linux` | ConsoleApp | `test_*_<arch>` | 平台测试 |

### 6.5 打包

```
package_win.bat <build_folder>     # 产出 build/package/win/*.7z
package_linux.sh <build_folder>    # 产出 build/package/linux/*.tar
```

### 6.6 使用方式（面向最终用户）

详见 [post_build/README.release.md](file:///workspace/post_build/README.release.md)：

1. 用游戏对应位宽的 `steam_api(64).dll` / `libsteam_api.so` 替换游戏自带文件。
2. 运行 `generate_interfaces` 生成 `steam_interfaces.txt`，放入 `steam_settings/`。
3. 参考 [post_build/steam_settings.EXAMPLE/](file:///workspace/post_build/steam_settings.EXAMPLE) 编辑配置（`configs.main/user/app/overlay.ini`、`achievements.json`、`stats.txt`、`dlc.txt` 等）。
4. 将 `steam_settings/` 放到模拟器 DLL 同级。
5. （可选）若游戏不通过 `steam_api` 加载，使用 `ColdClientLoader` 注入 `steamclient(64).dll`。

**存档位置**：Windows `%appdata%\GSE Saves\`；Linux `$XDG_DATA_HOME/GSE Saves/`（或 `$HOME/.local/share/GSE Saves/`）。可在 `configs.user.ini` 用 `local_save_path` 改为便携存档，用 `saves_folder_name` 重命名基础目录。

### 6.7 Dota 2 局域网快速开始

见 [DOTA2_LAN_QUICKSTART.md](file:///workspace/DOTA2_LAN_QUICKSTART.md)：编译 experimental x64 版本，替换 Dota 2 的 `steam_api64.dll`，配置 `steam_settings`（含 `steam_appid.txt=570`、`gc.json` 指定 `gc_profile=dota2`），启动即可。内存补丁与 `IP_AllowWithoutAuth` 自动应用。

### 6.8 CI

[.github/workflows/](file:///workspace/.github/workflows) 提供 `emu-deps-linux/win.yml`（依赖）、`emu-build-all-linux/win.yml`（全量构建）、`emu-pull-request.yml`、`release.yml`、`migrate_gse-build-*.yml` 等。Fork 后在 Actions 手动触发即可作为远程构建器（详见 [README.md](file:///workspace/README.md) "Using Github CI as a builder"）。

---

## 7. 关键设计约定

1. **单实例多版本**：`Steam_Client` 与各 `Steam_*` 实现多重继承所有历史接口版本，单实例服务任意 SDK 版本，避免版本分支。
2. **依赖注入**：各接口实现通过构造函数接收核心依赖指针，无全局可变状态（除 `global_mutex`、`startup_counter`、`dbg_logger` 等基础设施）。
3. **后台线程驱动**：`Steam_Client` 的 `KillableWorker` 在游戏不主动 `RunCallbacks` 时仍推进网络与周期任务（`max_stall_ms=300ms`）。
4. **LAN P2P 协议**：`Networking` 自带基于 `Common_Message` 的 UDP/TCP 协议，所有跨实例同步（大厅/好友/统计/库存等）共用同一信封。
5. **GBE 纯函数化**：Dota2 GC 模块群刻意无全局可变状态、无直接 I/O，返回值类型"计划"结构，由 GC handler apply。这使其可独立单元测试（`gbe_*_test` 工具）。
6. **手写 protobuf wire**：`gbe_proto_wire` 不依赖 protobuf 库，自行实现 varint/fixed32/fixed64/length-delimited 读写与递归改写，降低耦合与分配。
7. **快照 vs 发布分离**：强类型 `*SnapshotData`（从 wire 解析）与字符串化 `*PublishData`（广播给 peer）分离，兼顾类型安全与 Steam/JSON 字符串约定。
8. **Overlay 可选**：通过 `EMU_OVERLAY` 宏，`Steam_Overlay` 在 regular 构建中退化为 stub，保持 ABI 一致。
9. **平台隔离**：OS 相关代码用 `__WINDOWS__`/`__LINUX__` 宏隔离；`crash_printer`、`pe_helpers`、`steam_old_lib`、Dota2 内存补丁等平台相关模块仅在对应平台编译。
10. **配置优先级**：本地 `steam_settings/` 总覆盖全局 `GSE Saves/`；`.ini` 仅需保留必要项。

---

> 本 Wiki 基于仓库当前状态分析生成。如需深入了解某模块，可点击文中文件链接直达源码；变更历史见 [CHANGELOG.md](file:///workspace/CHANGELOG.md)，第三方致谢见 [CREDITS.md](file:///workspace/CREDITS.md)，调试指南见 [DEBUG_GUIDE.md](file:///workspace/DEBUG_GUIDE.md)。
