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

---

## VAC Secure Flag Patch (client.dll)

### 问题描述

好友接受组队邀请时弹出 VAC 弹窗（"无法验证本机是否安全"），accept 被静默转换为 decline。Wireshark 中 4513 消息 body 显示 `10 00`（accept=0）。

### 根因分析

#### 完整调用链

```
dota2.exe 启动
  -> 加载 steamnetworkingsockets.dll（游戏目录下的真实版本）
  -> steamnetworkingsockets.dll 尝试连接 Valve 网络基础设施
  -> LAN 环境下连接失败
  -> dota2.exe 内部签名验证流程无法完成
  -> BSecureAllowed() 返回 false
  -> client.dll 设置全局标志字节 = 1
  -> 处理 lobby invite accept 时检查该标志
  -> 标志非零 -> 弹出 VAC 弹窗 -> accept 转为 decline
```

#### BSecureAllowed 函数

位于 `dota2.exe` 中（偏移 `0x49470`），检查全局状态对象的 5 个条件：

| 偏移 | 要求 | 含义 | 正常Steam值 | emu值 |
|------|------|------|-------------|-------|
| +24 | == 1 | 签名验证结果A | 1 | 5 |
| +28 | == 1 | 签名验证结果B (steam.signatures) | 1 | 2 |
| +2C | 1或2 | 未知 | 1 | 1 (PASS) |
| +30 | 1或2 | 未知 | 2 | 2 (PASS) |
| +44 | == 9 | 初始化阶段计数 | 9 | 1 |

全部满足才返回 true。emu 环境下 3 个条件失败。

#### 为什么 emu 无法控制这些条件

1. **签名验证**（+24, +28）：`dota2.exe` 直接读取 `steam.signatures` / `system.signatures` 文件，验证 Steam 客户端 DLL 的完整性。单个文件 SHA1 匹配，但 DIGEST（RSA 签名）验证需要 Valve 公钥，该公钥的获取依赖正常运行的 Steam 客户端环境。

2. **初始化阶段**（+44）：需要经过完整的网络初始化流程才能从 1 递增到 9。LAN 环境下 `steamnetworkingsockets.dll` 无法连接 Valve 网络，流程卡在阶段 1。

3. **不经过 steam_api64.dll**：这些检查完全在 `dota2.exe` 内部完成，不调用任何我们能 hook 的 Steam API 接口。

#### client.dll 中设置全局标志的代码

位于 `client.dll` 偏移 `0x54FE47`：

```asm
; 调用 BSecureAllowed(0, 0, 0)
call [rsp+28]              ; BSecureAllowed 函数指针
movzx eax, al              ; 获取返回值
test eax, eax              ; 检查
jne +0x0A                  ; 返回 true -> 跳过（安全）
mov dword [rsp+20], 1      ; 返回 false -> 标记不安全
jmp ...
mov dword [rsp+20], 0      ; 返回 true 的路径

; 后续将 [rsp+20] 写入对象+0x41，然后：
test eax, eax              ; 85 C0
je +0x26                   ; 74 26 (如果为0跳过)
mov byte ptr [rip+X], 1   ; C6 05 xx xx xx xx 01  <-- 设置全局标志
xor eax, eax              ; 33 C0
cmp eax, 1                ; 83 F8 01
```

#### client.dll 中检查全局标志的代码

位于 `client.dll` 偏移 `0x24EB7AF`：

```asm
; 先执行 3 个 vtable 检查（都通过了）
call qword ptr [rax+190]   ; 检查1：返回指针，非空则 decline
call qword ptr [rax+B0]    ; 检查2：返回 bool，true 则 decline
call qword ptr [rax+C8]    ; 检查3：返回 bool，true 则 decline

; 第4个检查：全局标志
cmp byte ptr [rip+X], al   ; al=0，比较全局标志和0
jne ShowPopup              ; 不等于0 -> 弹窗 + decline
```

### 修复方案

在 `networking_patch.h` 中的 `ApplyVACPatch()` 函数：

1. 后台线程等待 `client.dll` 加载（最多 120 秒）
2. 用 pattern scan 找到 `mov byte ptr [rip+X], 1` 指令
3. 将立即数 `01` patch 为 `00`

**Pattern 特征码**：
```
85 C0 74 ?? C6 05 ?? ?? ?? ?? 01 33 C0 83 F8 01
```

含义：`test eax,eax / je ? / mov byte [rip+X], 1 / xor eax,eax / cmp eax, 1`

`xor eax,eax` 后紧跟 `cmp eax, 1` 是一个永远为 false 的比较，这个序列在 client.dll 中是唯一的，保证了 pattern 匹配的准确性。

### 调试 VAC Patch

#### 检查日志

日志文件：`C:\Users\Public\gbe_networking_patch.log`

成功时显示：
```
[VAC_MAIN] VACPatch thread starting
[VAC_THREAD] VAC patch thread started
[VAC_PATCH] Searching for client.dll...
[VAC_PATCH] Found client.dll at base 0x..., size ... bytes
[VAC_PATCH] Starting VAC secure flag patch in ... bytes...
[VAC_PATCH] Found exact pattern at offset 0x54FE43
[PATCH] Successfully patched byte at 0x...: 0x01 -> 0x00
[VAC_THREAD] VAC secure flag patch applied successfully!
```

失败时显示：
```
[VAC_PATCH] No matching pattern found
```
或：
```
[VAC_PATCH] Failed to find client.dll after 120 seconds
```

#### 手动验证（x64dbg）

1. 附加 x64dbg 到 Dota2 进程
2. `Alt+E` 找到 `client.dll` 基地址
3. 计算全局标志地址：基地址 + `0x6011E18`
4. 在 Dump 面板查看该字节：
   - `00` = patch 成功，accept 正常
   - `01` = patch 失败，会触发 decline
5. 手动修改为 0 验证：命令栏输入 `mov byte:[地址],0`

#### Dota2 更新后 pattern 失效

如果 Valve 更新了 `client.dll` 导致 pattern 匹配失败：

1. 用 x64dbg 附加到 Dota2
2. 找到 `client.dll` 基地址
3. 搜索字符串 `"ShowPopup"` 定位 VAC 检查函数
4. 往上找 `cmp byte ptr [rip+X], al` + `jne` 的组合
5. 再往上找设置该全局字节的代码（`mov byte ptr [rip+X], 1`）
6. 记录新的字节序列，更新 `networking_patch.h` 中的 pattern

#### 验证签名文件状态

确认 `steam.signatures` 是否和当前 Steam 客户端匹配：

```powershell
# PowerShell
Get-FileHash "C:\Program Files (x86)\Steam\steamclient64.dll" -Algorithm SHA1
```

对比 `steam.signatures` 中 `steamclient64.dll` 行的 SHA1 值。

### 相关文件

| 文件 | 说明 |
|------|------|
| `dll/dll/networking_patch.h` | patch 实现（`ApplyVACPatch`、`PatchVACSecureFlag`） |
| `dll/base.cpp` | 调用入口（DllMain / CppRuntimeTrick） |
| `steam.signatures` | Steam 客户端 DLL 签名文件（游戏目录） |
| `system.signatures` | 系统完整性签名文件（游戏目录） |

### 已排除的方案

| 方案 | 为什么不行 |
|------|-----------|
| 修改 GetCertAsync 返回 k_EResultOK | 不影响 BSecureAllowed 检查 |
| 清零 2002 消息中的 ban 字段 | 不影响 BSecureAllowed 检查 |
| 修改 account_flags | 不影响 BSecureAllowed 检查 |
| steamclient 模式（不替换 steam_api64.dll） | 签名文件不校验 steam_api64.dll，问题在 DIGEST 验证和网络初始化 |
| 启动真正的 Steam 客户端 | LAN 环境下 Steam 也无法完成网络初始化 |
| 从 emu 层面设置 BSecureAllowed 条件 | 检查完全在 dota2.exe 内部，不经过 steam_api64.dll |

## 测试清单

### steamnetworkingsockets patch
- [ ] 日志文件已创建
- [ ] 日志显示 `[MAIN]` 被调用
- [ ] 日志显示找到了 DLL
- [ ] 日志显示找到了字节特征
- [ ] 日志显示补丁成功应用
- [ ] 游戏控制台中配置项可见
- [ ] 配置项值为 1
- [ ] 能够创建局域网游戏
- [ ] 能够连接到局域网游戏

### VAC secure flag patch
- [ ] 日志显示 `[VAC_THREAD]` 启动
- [ ] 日志显示找到了 client.dll
- [ ] 日志显示找到了 pattern
- [ ] 日志显示 patch 成功
- [ ] 好友接受邀请不弹 VAC 弹窗
- [ ] Wireshark 中 4513 body 显示 `10 01`（accept=1）
- [ ] 好友成功加入房间

## 联系支持

如果以上步骤都无法解决问题，请在 GitHub 上提交 Issue：
https://github.com/Svenmax/gbe_fork/issues

包含：
- 完整的日志文件
- Dota 2 版本信息
- 系统信息
- 已尝试的解决方法
