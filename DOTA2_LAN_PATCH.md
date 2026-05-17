# Dota 2 离线局域网补丁实现

本文档说明了 GBE Fork 中实现的 Dota 2 离线局域网补丁功能。

## 功能概述

该补丁实现了两个核心功能：

1. **运行时内存补丁**：修改 `steamnetworkingsockets.dll` 的内存，使 `IP_AllowWithoutAuth` 配置项在 release 模式下可见
2. **自动配置**：在 GBE 初始化时自动设置 `IP_AllowWithoutAuth = 1`，无需用户手动操作

## 实现原理

### 1. 内存补丁 (`dll/dll/networking_patch.h`)

补丁通过以下步骤工作：

1. **等待 DLL 加载**：在后台线程中等待 `steamnetworkingsockets.dll` 加载（最多 30 秒）
2. **搜索特征字节**：在 DLL 的 `.text` 段中搜索特定的字节序列
   - 特征：`84 DB 75 11 48 8B 40 20`
   - 对应汇编：`TEST BL,BL` + `JNZ +0x11` + `MOV RAX,[RAX+0x20]`
3. **修改跳转指令**：将 `JNZ`(0x75) 改为 `JZ`(0x74)，反转条件判断
4. **效果**：使 `IP_AllowWithoutAuth` 配置项在 release 模式下可见

### 2. 自动配置 (`dll/steam_networking_utils.cpp`)

在 `Steam_Networking_Utils` 构造函数中自动设置：

```cpp
g_gbe_ip_allow_without_auth = 1;
g_gbe_ip_localhost_allow_without_auth = 1;
g_gbe_unencrypted = 1;
```

这些全局变量控制 GBE 的网络配置行为，设置为 1 后允许：
- 无需 Steam 认证的连接
- 本地主机连接
- 未加密的连接

### 3. 初始化时机 (`dll/base.cpp`)

在 `DllMain` 的 `DLL_PROCESS_ATTACH` 阶段调用 `NetworkingPatch::ApplyAll()`：

```cpp
case DLL_PROCESS_ATTACH:
    GBE_LogDllBootstrap("DLL LOADED SUCCESSFULLY");
    PRINT_DEBUG("experimental DLL_PROCESS_ATTACH");
    
    // Apply Dota 2 LAN patch for steamnetworkingsockets.dll
    NetworkingPatch::ApplyAll();
    
    // ... 其他初始化代码
```

## 技术细节

### 为什么需要内存补丁？

Dota 2 使用 Valve 的 `SteamNetworkingSockets` 库进行网络通信。在离线/局域网环境下：

1. 客户端无法从 Steam 获取有效证书
2. 连接被拒绝，报错 "Cannot use unsigned cert; failing connection"
3. 库中存在 `IP_AllowWithoutAuth` 配置项可以解决此问题
4. 但该配置项被标记为 dev 类型，在 release 模式下不可见

### 枚举函数逻辑

库中的枚举函数通过 switch 语句过滤配置项：

```c
switch(id) {
case 0x17:  // IP_AllowWithoutAuth
case 0x22:
case 0x24:
    if (bEnumerateDevVars != 0) {  // 只有 dev 模式才返回
        return id;
    }
    break;
default:
    return id;  // 普通配置项直接返回
}
```

对应的汇编代码：

```asm
TEST BL, BL          ; 检查 bEnumerateDevVars (84 DB)
JNZ  +0x11           ; 如果非零(dev模式)则跳转到返回分支 (75 11)
MOV  RAX,[RAX+0x20]  ; 否则跳过，继续遍历下一个配置项 (48 8B 40 20)
```

### 补丁方法

将 `JNZ`(0x75) 改为 `JZ`(0x74)，反转判断逻辑：
- 原逻辑：如果是 dev 模式（非零）则返回配置项
- 新逻辑：如果不是 dev 模式（为零）则返回配置项

## 使用方法

### 编译

使用标准的 GBE Fork 编译流程：

```bash
# Windows
build_win_premake.bat

# Linux
./build_linux_premake.sh
```

### 部署

1. 将编译好的 `steam_api64.dll` 替换到 Dota 2 目录：
   ```
   Steam\steamapps\common\dota 2 beta\game\bin\win64\
   ```

2. 启动 Dota 2（离线模式）

3. 创建局域网游戏：
   - 一台电脑创建服务器：控制台输入 `map dota loopback=0`
   - 其他电脑连接：控制台输入 `connect <服务器IP>`

### 验证

补丁成功后，可以在控制台中查看配置项：

```
net_option IP_AllowWithoutAuth
```

应该显示值为 1。

## 兼容性

### 支持的平台

- ✅ Windows x64
- ❌ Linux（需要适配不同的 DLL 加载机制）
- ❌ macOS（需要适配不同的 DLL 加载机制）

### 支持的游戏

理论上支持所有使用 `SteamNetworkingSockets` 库的游戏，包括：
- Dota 2
- Counter-Strike 2
- 其他使用该库的 Source 2 引擎游戏

### 版本兼容性

补丁使用字节特征匹配，如果 Valve 更新 `steamnetworkingsockets.dll` 后特征字节变化，补丁可能失效。

**应对方法**：
1. 使用 Ghidra 按照 `dota2lan.md` 中的步骤重新定位
2. 更新 `networking_patch.h` 中的 `pattern` 数组
3. 重新编译

## 注意事项

⚠️ **重要警告**：

1. **不要在 VAC 官方服务器上使用**，有封号风险
2. **仅用于离线/局域网游戏**
3. 每次 Dota 2 更新后需要验证补丁是否仍然有效
4. 内存补丁方案不修改磁盘文件，相对更安全

## 故障排除

### 补丁未生效

**症状**：连接时仍然报错 "Cannot use unsigned cert"

**可能原因**：
1. `steamnetworkingsockets.dll` 版本不匹配，特征字节已变化
2. DLL 未加载或加载时间超过 30 秒
3. 补丁线程启动失败

**解决方法**：
1. 检查 `C:\Users\Public\gbe_gc_debug.log` 日志
2. 使用 Ghidra 重新定位特征字节
3. 更新 `networking_patch.h` 中的 pattern

### 无法创建局域网游戏

**症状**：`map dota loopback=0` 命令无效

**可能原因**：
1. Dota 2 版本不支持该命令
2. 需要额外的启动参数

**解决方法**：
1. 添加启动参数：`-console -insecure`
2. 尝试其他地图创建命令

## 参考资料

- 原始文章：[Patching CS2 beta for offline multiplayer](https://thomasz.me/2023/10/24/patching-cs2-beta-for-offline-multiplayer/)
- Valve 开源库：[GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets)
- GBE Fork：[https://github.com/Svenmax/gbe_fork](https://github.com/Svenmax/gbe_fork)

## 贡献

如果您发现补丁在新版本中失效，欢迎提交 Issue 或 Pull Request 更新特征字节。

## 许可证

本补丁遵循 GBE Fork 的 LGPL v3 许可证。
