# 调试指南 - Dota 2 LAN 补丁

## 问题：配置项未找到

如果在游戏控制台中输入 `net_option IP_AllowWithoutAuth` 显示：
```
SteamNetworkingSockets config option not found
```

这说明内存补丁没有生效。

## 调试步骤

### 1. 检查调试日志

查看日志文件：
```
C:\Users\Public\gbe_networking_patch.log
```

日志会显示：
- `[MAIN]` - 补丁入口是否被调用
- `[THREAD]` - 后台线程是否启动
- `[INIT]` - 是否找到 steamnetworkingsockets.dll
- `[PATCH]` - 是否找到字节特征
- `[PATCH]` - 补丁是否成功应用

### 2. 常见问题诊断

#### 问题 A：DLL 未找到

**日志显示**：
```
[INIT] Failed to find steamnetworkingsockets DLL after 30 seconds
```

**原因**：
- Dota 2 没有加载 steamnetworkingsockets.dll
- DLL 名称不匹配

**解决方法**：
1. 检查 Dota 2 目录中是否存在该 DLL
2. 使用 Process Explorer 查看 Dota 2 加载了哪些 DLL
3. 可能需要添加其他 DLL 名称变体

#### 问题 B：字节特征未找到

**日志显示**：
```
[PATCH] No matching pattern found
```

**原因**：
- Valve 更新了 DLL，字节特征已变化
- 编译器优化导致代码不同

**解决方法**：
1. 使用 Ghidra 分析当前版本的 DLL
2. 按照 `dota2lan.md` 中的步骤重新定位
3. 更新 `networking_patch.h` 中的 pattern

#### 问题 C：字节不匹配

**日志显示**：
```
[PATCH] Byte mismatch at 0x...: expected 0x75, found 0xXX
```

**原因**：
- 找到了位置，但字节值不是预期的 0x75
- 可能是不同的跳转指令

**解决方法**：
1. 记录实际找到的字节值
2. 使用 Ghidra 确认该位置的指令
3. 调整补丁逻辑

#### 问题 D：内存保护失败

**日志显示**：
```
[PATCH] VirtualProtect failed: error XXXX
```

**原因**：
- 权限不足
- 内存页面被保护

**解决方法**：
1. 以管理员身份运行 Dota 2
2. 检查杀毒软件是否阻止
3. 检查 DEP (数据执行保护) 设置

### 3. 使用 Ghidra 重新定位字节特征

如果字节特征不匹配，需要重新定位：

#### 步骤 1：导入 DLL
1. 打开 Ghidra
2. File → Import File
3. 选择 `steamnetworkingsockets.dll`
4. 分析完成后打开 CodeBrowser

#### 步骤 2：搜索字符串
1. Search → For Strings
2. 搜索 `IP_AllowWithoutAuth`
3. 双击结果跳转

#### 步骤 3：找到配置项结构
1. 字符串地址往上看
2. 找到配置项数据结构
3. 第一个 4 字节 = 配置项 ID (应该是 0x17)

#### 步骤 4：找到枚举函数
1. 搜索对配置项表的引用
2. 找到包含 switch 语句的函数
3. 在 switch 中找到 case 0x17

#### 步骤 5：定位条件跳转
1. 在 case 0x17 分支中找到：
   - `TEST` 指令（检查 bEnumerateDevVars）
   - 条件跳转指令（`JNZ` 或 `JE`）
2. 记录字节序列

#### 步骤 6：更新代码
在 `networking_patch.h` 中更新 pattern：
```cpp
byte_t pattern_win[] = { 0xXX, 0xXX, 0xXX, ... };
```

### 4. 临时解决方案

如果补丁无法工作，可以手动设置配置：

1. 启动 Dota 2
2. 打开控制台
3. 输入：
   ```
   net_option IP_AllowWithoutAuth 1
   net_option IPLocalHost_AllowWithoutAuth 1
   net_option Unencrypted 1
   ```

**注意**：手动设置在游戏重启后会丢失。

### 5. 报告问题

如果无法解决，请提供以下信息：

1. **日志文件内容**：
   ```
   C:\Users\Public\gbe_networking_patch.log
   ```

2. **Dota 2 版本**：
   - 在 Steam 中右键 Dota 2 → 属性 → 本地文件
   - 查看版本号

3. **DLL 信息**：
   - 文件路径
   - 文件大小
   - 修改日期

4. **系统信息**：
   - Windows 版本
   - 是否以管理员运行
   - 杀毒软件

## 高级调试

### 使用 x64dbg 动态调试

1. 用 x64dbg 附加到 Dota 2 进程
2. 在 `NetworkingPatch::ApplyAll` 设置断点
3. 单步执行，查看：
   - DLL 是否被找到
   - 字节搜索是否成功
   - 内存修改是否成功

### 使用 Process Monitor

1. 运行 Process Monitor
2. 过滤 Dota 2 进程
3. 查看：
   - 加载了哪些 DLL
   - 加载顺序
   - 加载时间

### 使用 API Monitor

1. 运行 API Monitor
2. 监控 Dota 2 进程
3. 查看：
   - `GetModuleHandle` 调用
   - `VirtualProtect` 调用
   - 返回值和错误码

## 已知问题

### 问题 1：DLL 加载太晚

**症状**：日志显示 30 秒后仍未找到 DLL

**原因**：steamnetworkingsockets.dll 在游戏启动很久后才加载

**解决方法**：增加等待时间或改为事件驱动

### 问题 2：多个 DLL 版本

**症状**：找到了 DLL 但字节特征不匹配

**原因**：Dota 2 可能加载了多个版本的 DLL

**解决方法**：检查所有加载的 DLL，选择正确的版本

### 问题 3：代码混淆

**症状**：无法找到清晰的字节特征

**原因**：Valve 可能使用了代码混淆或保护

**解决方法**：使用更宽松的匹配模式或放弃内存补丁

## 测试清单

- [ ] 日志文件已创建
- [ ] 日志显示 `[MAIN]` 被调用
- [ ] 日志显示找到了 DLL
- [ ] 日志显示找到了字节特征
- [ ] 日志显示补丁成功应用
- [ ] 游戏控制台中配置项可见
- [ ] 配置项值为 1
- [ ] 能够创建局域网游戏
- [ ] 能够连接到局域网游戏

## 联系支持

如果以上步骤都无法解决问题，请在 GitHub 上提交 Issue：
https://github.com/Svenmax/gbe_fork/issues

包含：
- 完整的日志文件
- Dota 2 版本信息
- 系统信息
- 已尝试的解决方法
