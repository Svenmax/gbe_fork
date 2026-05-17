# 平台支持说明

## 概述

Dota 2 离线局域网补丁包含两个独立的功能层，它们的平台支持情况不同。

## 功能层次

### 1. 配置自动设置（跨平台）

**支持平台**：
- ✅ Windows
- ✅ Linux
- ✅ macOS

**实现位置**：`dll/steam_networking_utils.cpp`

**功能**：
在 `Steam_Networking_Utils` 构造函数中自动设置以下配置：
```cpp
g_gbe_ip_allow_without_auth = 1;
g_gbe_ip_localhost_allow_without_auth = 1;
g_gbe_unencrypted = 1;
```

**特点**：
- 纯 C++ 代码，无平台依赖
- 在所有平台上都会执行
- 无需用户手动输入控制台命令
- 直接修改 GBE 内部的全局变量

### 2. 内存补丁（仅 Windows）

**支持平台**：
- ✅ Windows x64
- ❌ Linux（需要适配 .so 文件）
- ❌ macOS（需要适配 .dylib 文件）

**实现位置**：`dll/dll/networking_patch.h`

**功能**：
运行时修改 `steamnetworkingsockets.dll` 的内存，使 `IP_AllowWithoutAuth` 配置项在 release 模式下可见。

**特点**：
- 使用 Windows API（`VirtualProtect`、`GetModuleHandle` 等）
- 使用 `#ifdef __WINDOWS__` 保护
- 仅在 Windows 平台编译和执行
- 在后台线程中异步执行

## 平台支持对比表

| 平台 | 配置自动设置 | 内存补丁 | 整体状态 | 说明 |
|------|-------------|----------|----------|------|
| **Windows x64** | ✅ | ✅ | **完全支持** | 开箱即用，无需任何手动操作 |
| **Linux** | ✅ | ❌ | **部分支持** | 配置自动设置，可能需要手动操作* |
| **macOS** | ✅ | ❌ | **部分支持** | 配置自动设置，可能需要手动操作* |

\* 详见下文"Linux/macOS 使用说明"

## 为什么 Linux/macOS 可能也能工作？

内存补丁的目的是让 `IP_AllowWithoutAuth` 配置项在 release 模式下可见。但是：

1. **如果游戏的 steamnetworkingsockets 库在 Linux/macOS 上没有隐藏这个配置项**
   - 那么仅靠配置自动设置就足够了
   - 不需要内存补丁
   - 功能完全正常

2. **只有当配置项被标记为 dev-only 时**
   - 才需要内存补丁使其可见
   - 这种情况下 Linux/macOS 用户需要手动操作

## Linux/macOS 使用说明

### 场景 1：配置项已可见（推荐测试）

如果 `IP_AllowWithoutAuth` 在 Linux/macOS 上本身就是可见的：

1. 编译 GBE Fork
2. 替换 `libsteam_api.so`（Linux）或 `libsteam_api.dylib`（macOS）
3. 启动 Dota 2
4. 控制台输入：`net_option IP_AllowWithoutAuth`
5. 如果显示 `IP_AllowWithoutAuth = 1`，说明功能正常！
6. 直接创建局域网游戏

### 场景 2：配置项不可见（需要手动操作）

如果 `IP_AllowWithoutAuth` 在 Linux/macOS 上被隐藏：

1. 编译 GBE Fork
2. 替换库文件
3. 启动 Dota 2
4. 控制台输入：`net_option IP_AllowWithoutAuth`
5. 如果显示 "Unknown config"，说明配置项被隐藏
6. **手动设置**：
   ```
   net_option IP_AllowWithoutAuth 1
   net_option IPLocalHost_AllowWithoutAuth 1
   net_option Unencrypted 1
   ```
7. 创建局域网游戏

**注意**：手动设置的值在游戏重启后会丢失，需要每次启动时重新设置。

## 如何添加 Linux/macOS 内存补丁支持？

如果需要在 Linux/macOS 上实现完全自动化，需要：

### Linux 支持

1. **适配 ELF 格式**
   - 使用 `dlopen`/`dlsym` 获取库句柄
   - 使用 `mprotect` 修改内存保护
   - 搜索 `.text` 段中的字节特征

2. **示例代码框架**：
   ```cpp
   #ifdef __linux__
   #include <dlfcn.h>
   #include <sys/mman.h>
   
   void* handle = dlopen("steamnetworkingsockets.so", RTLD_LAZY);
   // ... 搜索和修改内存
   #endif
   ```

### macOS 支持

1. **适配 Mach-O 格式**
   - 使用 `dlopen`/`dlsym` 获取库句柄
   - 使用 `mprotect` 修改内存保护
   - 搜索 `__TEXT` 段中的字节特征

2. **示例代码框架**：
   ```cpp
   #ifdef __APPLE__
   #include <dlfcn.h>
   #include <sys/mman.h>
   
   void* handle = dlopen("steamnetworkingsockets.dylib", RTLD_LAZY);
   // ... 搜索和修改内存
   #endif
   ```

### 挑战

1. **字节特征可能不同**
   - Linux/macOS 使用不同的编译器和优化选项
   - 汇编代码可能不同
   - 需要使用 Ghidra/IDA 重新定位

2. **库文件名可能不同**
   - Windows: `steamnetworkingsockets.dll`
   - Linux: `libsteamnetworkingsockets.so`
   - macOS: `libsteamnetworkingsockets.dylib`

3. **内存布局可能不同**
   - 不同平台的内存对齐和布局可能不同
   - 需要分别测试和验证

## 测试建议

### Windows 测试
1. ✅ 编译通过
2. ✅ 内存补丁应用成功
3. ✅ 配置自动设置成功
4. ✅ 局域网游戏正常

### Linux 测试
1. ⏳ 编译通过
2. ⏳ 配置自动设置成功
3. ⏳ 检查配置项是否可见
4. ⏳ 局域网游戏是否正常
5. ⏳ 如果不正常，是否需要手动设置

### macOS 测试
1. ⏳ 编译通过
2. ⏳ 配置自动设置成功
3. ⏳ 检查配置项是否可见
4. ⏳ 局域网游戏是否正常
5. ⏳ 如果不正常，是否需要手动设置

## 总结

### 当前状态

- **Windows**：完全支持，开箱即用 ✅
- **Linux/macOS**：部分支持，可能需要手动操作 ⚠️

### 优势

- 配置自动设置是跨平台的
- 即使没有内存补丁，也能提供部分功能
- 代码结构清晰，易于扩展

### 未来改进

1. 在 Linux/macOS 上测试，确认是否需要内存补丁
2. 如果需要，实现 Linux/macOS 的内存补丁支持
3. 统一三个平台的实现，提供一致的用户体验

## 贡献

如果你在 Linux/macOS 上测试了这个功能，欢迎反馈：
- 配置项是否可见？
- 是否需要手动设置？
- 局域网游戏是否正常？

这些信息将帮助我们决定是否需要为 Linux/macOS 实现内存补丁。
