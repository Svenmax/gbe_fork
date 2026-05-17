# Dota 2 离线局域网补丁 - 实现总结

## 概述

本次实现为 GBE Fork 添加了 Dota 2 离线局域网支持，允许玩家在没有 Steam 认证的情况下进行局域网游戏。

## 修改的文件

### 1. 新增文件

#### `dll/dll/networking_patch.h`
- **功能**：运行时内存补丁，修改 `steamnetworkingsockets.dll`
- **实现**：
  - 在后台线程中等待 DLL 加载
  - 搜索特征字节序列 `84 DB 75 11 48 8B 40 20`
  - 将 `JNZ`(0x75) 修改为 `JZ`(0x74)
  - 使 `IP_AllowWithoutAuth` 配置项在 release 模式下可见
- **平台**：仅 Windows（使用 `#ifdef __WINDOWS__` 保护）

#### `DOTA2_LAN_PATCH.md`
- **功能**：完整的技术文档
- **内容**：
  - 实现原理详解
  - 技术细节说明
  - 使用方法
  - 故障排除指南
  - 兼容性说明

#### `DOTA2_LAN_QUICKSTART.md`
- **功能**：快速使用指南
- **内容**：
  - 编译和部署步骤
  - 快速开始教程
  - 常见问题解答
  - 故障排除

#### `verify_dota2_patch.bat`
- **功能**：Windows 批处理脚本，验证补丁是否正确应用
- **功能**：
  - 检查 DLL 文件是否存在
  - 显示调试日志
  - 提供验证步骤说明

### 2. 修改的文件

#### `dll/base.cpp`
- **修改位置**：第 18-19 行，第 691 行
- **修改内容**：
  ```cpp
  // 第 18-19 行：添加头文件包含
  #include "dll/base.h"
  #include "dll/networking_patch.h"
  
  // 第 691 行：在 DllMain 中调用补丁
  case DLL_PROCESS_ATTACH:
      GBE_LogDllBootstrap("DLL LOADED SUCCESSFULLY");
      PRINT_DEBUG("experimental DLL_PROCESS_ATTACH");
      
      // Apply Dota 2 LAN patch for steamnetworkingsockets.dll
      NetworkingPatch::ApplyAll();
  ```

#### `dll/steam_networking_utils.cpp`
- **修改位置**：第 117-121 行
- **修改内容**：
  ```cpp
  Steam_Networking_Utils::Steam_Networking_Utils(...)
  {
      // ... 原有代码 ...
      
      // Automatically enable offline LAN mode for Dota 2 and other games
      // Set IP_AllowWithoutAuth = 1 to allow connections without Steam authentication
      g_gbe_ip_allow_without_auth = 1;
      g_gbe_ip_localhost_allow_without_auth = 1;
      g_gbe_unencrypted = 1;
  }
  ```

## 实现原理

### 两层机制

1. **内存补丁层**（`networking_patch.h`）
   - 修改 `steamnetworkingsockets.dll` 的枚举函数
   - 使 dev 类型的配置项在 release 模式下可见
   - 在后台线程中异步执行，不阻塞 DLL 加载

2. **配置自动设置层**（`steam_networking_utils.cpp`）
   - 在 GBE 初始化时自动设置配置值
   - 无需用户手动输入控制台命令
   - 直接修改全局变量 `g_gbe_ip_allow_without_auth` 等

### 工作流程

```
DLL 加载
  ↓
DllMain(DLL_PROCESS_ATTACH)
  ↓
NetworkingPatch::ApplyAll() [后台线程]
  ↓
等待 steamnetworkingsockets.dll 加载
  ↓
搜索特征字节
  ↓
修改内存（JNZ → JZ）
  ↓
配置项变为可见
  
同时：
Steam_Networking_Utils 构造函数
  ↓
设置 g_gbe_ip_allow_without_auth = 1
  ↓
允许无认证连接
```

## 技术亮点

### 1. 非侵入式设计
- 不修改磁盘上的 DLL 文件
- 仅在内存中应用补丁
- 重启游戏后恢复原状

### 2. 异步执行
- 补丁在后台线程中执行
- 不阻塞 DLL 加载过程
- 最多等待 30 秒

### 3. 平台兼容性
- 使用 `#ifdef __WINDOWS__` 保护
- 仅在 Windows 平台编译相关代码
- 不影响 Linux/macOS 构建

### 4. 自动化
- 无需用户手动操作
- 自动设置所有必要的配置项
- 开箱即用

### 5. 可维护性
- 使用字节特征匹配，易于更新
- 详细的文档说明
- 清晰的代码注释

## 测试建议

### 单元测试
1. 验证 `NetworkingPatch::FindPattern` 函数
2. 验证 `NetworkingPatch::PatchByte` 函数
3. 验证配置项自动设置

### 集成测试
1. 编译 GBE Fork
2. 替换 Dota 2 的 `steam_api64.dll`
3. 启动 Dota 2
4. 检查控制台 `net_option IP_AllowWithoutAuth`
5. 创建局域网游戏
6. 连接测试

### 兼容性测试
1. 测试不同版本的 Dota 2
2. 测试不同版本的 `steamnetworkingsockets.dll`
3. 测试其他使用该库的游戏（CS2 等）

## 已知限制

1. **平台限制**：仅支持 Windows x64
2. **版本依赖**：依赖特定的字节特征，Valve 更新后可能失效
3. **VAC 风险**：不能在 VAC 保护的服务器上使用
4. **异步延迟**：补丁在后台线程中执行，可能有短暂延迟

## 未来改进方向

1. **Linux 支持**：适配 Linux 平台的 SO 文件
2. **macOS 支持**：适配 macOS 平台的 DYLIB 文件
3. **自动更新**：检测 DLL 版本并自动更新特征字节
4. **配置文件**：允许用户自定义配置项
5. **GUI 工具**：提供图形界面的补丁管理工具

## 贡献指南

如果您想贡献代码或报告问题：

1. **报告 Bug**：
   - 提供 Dota 2 版本
   - 提供 `gbe_gc_debug.log` 日志
   - 描述复现步骤

2. **更新特征字节**：
   - 使用 Ghidra 定位新的特征字节
   - 更新 `networking_patch.h` 中的 pattern
   - 提交 Pull Request

3. **添加新功能**：
   - 遵循现有代码风格
   - 添加详细注释
   - 更新相关文档

## 许可证

本实现遵循 GBE Fork 的 LGPL v3 许可证。

## 致谢

- 原始文章作者：[Patching CS2 beta for offline multiplayer](https://thomasz.me/2023/10/24/patching-cs2-beta-for-offline-multiplayer/)
- GBE Fork 项目维护者
- Valve 的 GameNetworkingSockets 开源库

## 联系方式

- GitHub Issues: https://github.com/Svenmax/gbe_fork/issues
- 原始需求文档：`dota2lan.md`
