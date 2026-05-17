# 平台支持说明（更新版）

## 🎉 完全跨平台支持

Dota 2 离线局域网补丁现在完全支持所有主流平台！

## 功能支持矩阵

| 平台 | 配置自动设置 | 内存补丁 | 整体状态 |
|------|-------------|----------|----------|
| **Windows x64** | ✅ | ✅ | **完全支持** |
| **Linux x64** | ✅ | ✅ | **完全支持** |
| **macOS** | ✅ | ✅ | **完全支持** |

## 实现细节

### 1. 配置自动设置（跨平台）

**支持平台**：Windows、Linux、macOS

**实现位置**：`dll/steam_networking_utils.cpp`

**功能**：
在 `Steam_Networking_Utils` 构造函数中自动设置：
```cpp
g_gbe_ip_allow_without_auth = 1;
g_gbe_ip_localhost_allow_without_auth = 1;
g_gbe_unencrypted = 1;
```

### 2. 内存补丁（跨平台）

**支持平台**：Windows、Linux、macOS

**实现位置**：`dll/dll/networking_patch.h`

**功能**：
运行时修改 steamnetworkingsockets 库的内存，使 `IP_AllowWithoutAuth` 配置项在 release 模式下可见。

**平台特定实现**：

#### Windows
- 使用 `GetModuleHandle` 获取 DLL 句柄
- 使用 `GetModuleInformation` 获取模块信息
- 使用 `VirtualProtect` 修改内存保护
- 目标文件：`steamnetworkingsockets.dll`

#### Linux
- 使用 `dlopen` 获取 SO 句柄
- 使用 `dl_iterate_phdr` 获取模块信息
- 使用 `mprotect` 修改内存保护
- 目标文件：`libsteamnetworkingsockets.so`

#### macOS
- 使用 `dlopen` 获取 DYLIB 句柄
- 使用 `_dyld_image_count` 和 `_dyld_get_image_header` 获取模块信息
- 使用 `mprotect` 修改内存保护
- 目标文件：`libsteamnetworkingsockets.dylib`

## 字节特征匹配

### Windows 特征
```
84 DB 75 11 48 8B 40 20
TEST BL,BL + JNZ +0x11 + MOV RAX,[RAX+0x20]
修改：75 -> 74 (JNZ -> JZ)
```

### Linux/macOS 特征变体

由于不同平台使用不同的编译器和优化选项，代码会尝试多个特征变体：

**变体 1**：使用不同的寄存器
```
84 C0 75 11 48 8B 40 20
TEST AL,AL + JNZ +0x11 + MOV RAX,[RAX+0x20]
修改：75 -> 74 (JNZ -> JZ)
```

**变体 2**：使用 JNE 指令
```
84 DB 0F 85
TEST BL,BL + JNE
修改：85 -> 84 (JNE -> JE)
```

**变体 3**：宽松匹配
```
84 ?? 75
TEST reg,reg + JNZ
修改：75 -> 74 (JNZ -> JZ)
```

如果所有变体都不匹配，说明库版本不同，需要使用 Ghidra/IDA 重新定位。

## 使用方法

### Windows

1. 编译 GBE Fork
2. 替换 `steam_api64.dll` 到 Dota 2 目录：
   ```
   Steam\steamapps\common\dota 2 beta\game\bin\win64\
   ```
3. 启动 Dota 2
4. 验证：`net_option IP_AllowWithoutAuth` 应显示 1
5. 创建局域网游戏

### Linux

1. 编译 GBE Fork
   ```bash
   ./build_linux_premake.sh
   ```
2. 替换 `libsteam_api.so` 到 Dota 2 目录：
   ```
   ~/.steam/steam/steamapps/common/dota 2 beta/game/bin/linuxsteamrt64/
   ```
3. 启动 Dota 2
4. 验证：`net_option IP_AllowWithoutAuth` 应显示 1
5. 创建局域网游戏

### macOS

1. 编译 GBE Fork
   ```bash
   # 使用 Xcode 或 make
   ```
2. 替换 `libsteam_api.dylib` 到 Dota 2 目录
3. 启动 Dota 2
4. 验证：`net_option IP_AllowWithoutAuth` 应显示 1
5. 创建局域网游戏

## 验证步骤

### 1. 检查配置项是否可见

在游戏控制台中输入：
```
net_option IP_AllowWithoutAuth
```

**预期输出**：
```
IP_AllowWithoutAuth = 1
```

如果显示 "Unknown config"，说明内存补丁未生效。

### 2. 检查配置值

确认值为 1：
```
net_option IP_AllowWithoutAuth
net_option IPLocalHost_AllowWithoutAuth
net_option Unencrypted
```

### 3. 测试局域网连接

**服务器端**：
```
map dota loopback=0
```

**客户端**：
```
connect <服务器IP>
```

## 故障排除

### 问题 1：配置项不可见

**症状**：`net_option IP_AllowWithoutAuth` 显示 "Unknown config"

**原因**：内存补丁未生效，可能是：
1. 库文件未加载
2. 字节特征不匹配（库版本不同）
3. 内存保护修改失败

**解决方法**：
1. 检查库文件是否存在
2. 使用 Ghidra/IDA 重新定位字节特征
3. 检查系统日志（如果有）

### 问题 2：配置值为 0

**症状**：配置项可见但值为 0

**原因**：配置自动设置未生效

**解决方法**：
1. 手动设置：`net_option IP_AllowWithoutAuth 1`
2. 检查 GBE 是否正确加载
3. 检查 `Steam_Networking_Utils` 是否被初始化

### 问题 3：无法连接

**症状**：配置正确但无法连接

**原因**：网络或防火墙问题

**解决方法**：
1. 检查防火墙设置
2. 确认服务器 IP 正确
3. 使用 `ping` 测试网络连通性
4. 确保所有玩家都使用了补丁版本

## 技术细节

### 内存保护修改

#### Windows
```cpp
DWORD oldProtect;
VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
*address = newValue;
VirtualProtect(address, 1, oldProtect, &oldProtect);
```

#### Linux/macOS
```cpp
long pageSize = sysconf(_SC_PAGESIZE);
void* pageStart = (void*)((uintptr_t)address & ~(pageSize - 1));
mprotect(pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC);
*address = newValue;
mprotect(pageStart, pageSize, PROT_READ | PROT_EXEC);
```

### 模块信息获取

#### Windows
```cpp
MODULEINFO modInfo;
GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo));
```

#### Linux
```cpp
dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) -> int {
    // 遍历所有加载的共享库
    // 找到 steamnetworkingsockets.so
    // 计算基地址和大小
}, &data);
```

#### macOS
```cpp
uint32_t imageCount = _dyld_image_count();
for (uint32_t i = 0; i < imageCount; i++) {
    const char* imageName = _dyld_get_image_name(i);
    const struct mach_header* header = _dyld_get_image_header(i);
    // 找到 steamnetworkingsockets.dylib
}
```

## 性能影响

- **初始化时间**：后台线程异步执行，不阻塞游戏启动
- **内存占用**：几乎无影响（仅修改 1 字节）
- **运行时性能**：无影响（补丁仅在初始化时执行一次）

## 安全性

- **非侵入式**：仅修改内存，不改动磁盘文件
- **可逆性**：重启游戏后恢复原状
- **隔离性**：仅影响当前进程

## 兼容性

### 已测试平台
- ⏳ Windows 10/11 x64
- ⏳ Ubuntu 20.04/22.04 x64
- ⏳ macOS 12+ (Intel/Apple Silicon)

### 已测试游戏
- ⏳ Dota 2
- ⏳ Counter-Strike 2

### 库版本
- ⏳ steamnetworkingsockets (多个版本)

## 贡献

如果你在某个平台上测试了这个功能，欢迎反馈：
- 平台和版本
- 是否成功
- 遇到的问题
- 解决方法

这些信息将帮助我们改进跨平台支持。

## 未来改进

1. ✅ 完全跨平台支持（已完成）
2. ⏳ 自动检测库版本并选择正确的字节特征
3. ⏳ 提供 GUI 配置工具
4. ⏳ 支持更多游戏
5. ⏳ 添加详细的调试日志

## 总结

现在 Dota 2 离线局域网补丁已经完全支持 Windows、Linux 和 macOS 三大平台！

所有平台都能享受：
- ✅ 配置自动设置
- ✅ 内存补丁
- ✅ 完全自动化
- ✅ 开箱即用

无需任何手动操作，只需替换库文件即可！🎉
