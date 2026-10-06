# Legacy Wrapped Parser Guard 任务清单

## 待实施

- [x] 盘点 `GBE_ExtractWrappedDotaDirectContext` 的声明、定义和调用点。
- [x] 更新 `docs/gc/ACTIVE_QUEUE.md`，标记本切片 WIP。
- [x] 增加审计，保护 `LEGACY_UNUSED` parser 无生产调用。
- [x] 增加审计回归，覆盖生产调用漂移和 routing inventory marker 漂移。
- [x] 运行 `bash tools/run_gc_verification.sh --full`。
- [x] 运行 `git diff --check`。

## 完成定义

- [x] legacy wrapped parser no-call contract 由审计保护。
- [x] wrapped post-login 生产路径行为保持。
- [x] 全量 GC 验证和 diff check 通过。
