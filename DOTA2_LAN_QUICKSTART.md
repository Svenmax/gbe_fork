# Dota 2 离线局域网补丁 - 快速使用指南

## 快速开始

### 1. 编译 GBE Fork

```bash
# Windows
build_win_premake.bat

# 编译完成后，DLL 文件位于：
# bin\experimental\x64\release\steam_api64.dll
```

### 2. 部署到 Dota 2

将编译好的 `steam_api64.dll` 复制到 Dota 2 目录：

```
Steam\steamapps\common\dota 2 beta\game\bin\win64\steam_api64.dll
```

**重要**：备份原始的 `steam_api64.dll`！

### 3. 启动 Dota 2

1. 启动 Steam（可以在线或离线模式）
2. 启动 Dota 2
3. 打开控制台（按 `~` 或 `` ` ``）

### 4. 验证补丁

在控制台中输入：

```
net_option IP_AllowWithoutAuth
```

如果显示 `IP_AllowWithoutAuth = 1`，说明补丁成功！

### 5. 创建局域网游戏

#### 服务器端（主机）

在控制台输入：

```
map dota loopback=0
```

这将创建一个局域网服务器。

#### 客户端（其他玩家）

在控制台输入：

```
connect <服务器IP地址>
```

例如：`connect 192.168.1.100`

## 常见问题

### Q: 如何打开控制台？

**A:** 
1. 在 Steam 中右键点击 Dota 2 → 属性
2. 在启动选项中添加：`-console`
3. 启动游戏后按 `~` 或 `` ` `` 键

### Q: 提示 "Cannot use unsigned cert" 错误

**A:** 
1. 确认 `steam_api64.dll` 已正确替换
2. 检查 `C:\Users\Public\gbe_gc_debug.log` 日志
3. 运行 `verify_dota2_patch.bat` 验证脚本

### Q: 无法连接到服务器

**A:**
1. 确保所有玩家都使用了补丁版本的 DLL
2. 检查防火墙设置，允许 Dota 2 通过
3. 确认服务器 IP 地址正确
4. 尝试 ping 服务器 IP 确认网络连通性

### Q: 游戏崩溃或无法启动

**A:**
1. 恢复原始的 `steam_api64.dll`
2. 检查是否使用了正确的 64 位版本
3. 确认 Dota 2 版本与 GBE 兼容

## 高级配置

### 查看所有网络配置

```
net_option
```

### 手动设置配置（如果自动设置失败）

```
net_option IP_AllowWithoutAuth 1
net_option IPLocalHost_AllowWithoutAuth 1
net_option Unencrypted 1
```

### 查看网络状态

```
net_status
```

## 故障排除

### 检查日志

查看 `C:\Users\Public\gbe_gc_debug.log` 文件，搜索：
- `DLL LOADED SUCCESSFULLY` - DLL 加载成功
- `NETUTILS_SET_CONFIG` - 配置设置记录
- `NETUTILS_GET_CONFIG` - 配置读取记录

### 重新应用补丁

1. 删除 `C:\Users\Public\gbe_gc_debug.log`
2. 重启 Dota 2
3. 检查新的日志文件

### 版本不兼容

如果 Dota 2 更新后补丁失效：

1. 查看 `DOTA2_LAN_PATCH.md` 中的"故障排除"章节
2. 使用 Ghidra 重新定位特征字节
3. 更新 `dll/dll/networking_patch.h` 中的 pattern
4. 重新编译

## 安全提示

⚠️ **警告**：
- **不要在 VAC 保护的官方服务器上使用此补丁**
- 仅用于离线/局域网游戏
- 使用修改过的 DLL 可能导致 VAC 封禁
- 建议仅在离线模式下使用

## 恢复原始状态

1. 删除或重命名补丁版本的 `steam_api64.dll`
2. 恢复备份的原始 `steam_api64.dll`
3. 重启 Dota 2

## 技术支持

如果遇到问题：

1. 查看 `DOTA2_LAN_PATCH.md` 完整文档
2. 检查 GBE Fork 的 GitHub Issues
3. 提供以下信息：
   - Dota 2 版本
   - GBE Fork 版本
   - `gbe_gc_debug.log` 日志内容
   - 错误信息截图

## 参考资料

- 完整技术文档：`DOTA2_LAN_PATCH.md`
- 原始需求文档：`dota2lan.md`
- GBE Fork 项目：https://github.com/Svenmax/gbe_fork
