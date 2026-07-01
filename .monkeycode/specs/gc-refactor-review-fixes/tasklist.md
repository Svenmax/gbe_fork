# GC 重构审查修复任务清单

- [x] 1. 修复 equip op 解析边界
  - [x] 1.1 改造 `GBE_ParseDotaEquipOps` 使用安全 protobuf wire 解析
    - 覆盖审查发现 F1：畸形 varint 不触发未定义行为，截断 payload 返回失败。
    - 覆盖审查发现 F3：缺失必需字段或未知 wire type 不生成默认 equip op。
  - [x] 1.2 调整 `GBE_HandleDotaEquipItemsRequest` 的解析失败处理
    - 覆盖审查发现 F1：调用方检查 helper 返回值，避免畸形请求继续进入成功语义路径。
    - 覆盖审查发现 F3：只处理字段完整且范围合法的 equip op。
  - [x] 1.3 补充 equip op 边界测试
    - 覆盖 malformed varint、截断 sub-message、缺失字段、越界 class/slot、正常卸载路径。

- [x] 2. 修复 unlock style bitmask 越界
  - [x] 2.1 为 `GBE_ApplyDotaUnlockStyleBitmask` 增加 style 范围校验
    - 覆盖审查发现 F2：`style_index >= 32` 返回失败，不执行左移。
    - 覆盖审查发现 F2：`item.style` 只在合法 style 范围内写入。
  - [x] 2.2 调整 unlock handler 的失败处理和日志
    - 覆盖审查发现 F2：非法 style 不产生错误 bitmask，不记录更新成功。
  - [x] 2.3 补充 style bitmask 边界测试
    - 覆盖合法 OR、创建 attr 400、`style_index=32`、`style_index=255`、超大 style。

- [x] 3. 修复 SOCreate item 序列化 helper
  - [x] 3.1 抽出共享 `Econ_Item` 到 `CSOEconItem` 序列化函数
    - 覆盖审查发现 F4：保留 custom name、custom desc、equip states、attributes、contains equipped state 标志。
    - 覆盖审查发现 F4：`item_to_gcprotobuf` 和 `GBE_BuildSOSingleObjectFromItem` 共享同一字段映射。
  - [x] 3.2 为 `GBE_BuildSOSingleObjectFromItem` 设置 SO owner 信息
    - 覆盖审查发现 F5：使用 `owner_soid { type=1, id=steam64 }` 表达对象归属。
    - 覆盖审查发现 F5：保留 type id、object data、version 的既有语义。
  - [x] 3.3 补充 SOCreate helper 字段测试
    - 覆盖 owner_soid、基础字段、attributes、equip states、custom 字段。

- [x] 4. 补强 payload helper 测试质量
  - [x] 4.1 替换明显的 smoke-only 断言
    - 覆盖审查发现 F6：关键 helper 测试断言返回值和输出关键字段。
  - [x] 4.2 修正 payload helper test 进度编号
    - 覆盖审查发现 F8：日志分母与实际测试组数量一致。

- [x] 5. 同步重构文档
  - [x] 5.1 更新 `REFACTOR_TODO.md` 当前阶段信息
    - 覆盖审查发现 F7：记录当前 HEAD、Phase 2.13、关键文件行数和完成状态。
  - [x] 5.2 更新 GC maintainability tasklist 覆盖入口
    - 覆盖审查发现 F7：补充 payload helper 测试入口与实际 replay fixtures。

- [x] 6. 检查点 - 确保所有测试通过
  - 确保所有测试通过,如有疑问请询问用户
  - 已通过：`tools/run_gc_offline_tests.sh`，payload helper 断言数提升至 `92/92`。
