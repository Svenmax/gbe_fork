# gbe_fork 项目文档索引

本文档是项目级文档入口，供后续 Agent 在执行实现、审查和排错任务前快速定位技术上下文。

## 项目技术文档

- `CODE_WIKI.md`：项目整体架构、模块职责、关键类与函数、依赖关系、构建运行方式。
- `DEBUG_GUIDE.md`：Dota 2 LAN 补丁调试指南，覆盖日志位置、常见失败原因和诊断步骤。
- `DOTA2_LAN_PATCH.md`：Dota 2 LAN 补丁技术说明。
- `DOTA2_LAN_QUICKSTART.md`：Dota 2 LAN 快速开始和验证流程。
- `IMPLEMENTATION_CHECKLIST.md`：Dota 2 LAN 补丁实现检查清单。
- `IMPLEMENTATION_SUMMARY.md`：Dota 2 LAN 补丁实现总结。
- `REFACTOR_TODO.md`：仓库级重构待办。

## GC 重构规格

- `.monkeycode/specs/gc-refactor-next-steps/requirements.md`：GC 后续重构需求与验收标准。
- `.monkeycode/specs/gc-refactor-next-steps/design.md`：GC 后续重构技术方案。
- `.monkeycode/specs/gc-refactor-next-steps/tasklist.md`：GC 后续重构实施任务清单。
- `.monkeycode/specs/gc-refactor-next-steps/pre-merge-checklist.md`：GC 重构 pre-merge 本地验证清单。
- `.monkeycode/specs/gc-refactor-next-steps/ci-verification-evaluation.md`：GC 重构 CI gate 接入评估。
- `.monkeycode/specs/gc-refactor-next-steps/template-replay-ownership.md`：GC template/replay 数据 ownership 和 patch 点治理记录。
- `.monkeycode/specs/gc-refactor-next-steps/reason-trace-governance.md`：GC reason string 和 trace 边界治理记录。
- `.monkeycode/specs/gc-refactor-next-steps/build-entrypoint-equivalence.md`：GC 构建入口、Premake 路径和 CI gate 评估记录。
- `.monkeycode/specs/gc-refactor-next-steps/dependency-seam-error-boundary.md`：GC 外部依赖 seam、持久化、ownership、错误和 replay 性能治理记录。
- `.monkeycode/specs/gc-refactor-follow-up/requirements.md`：GC 下一轮重构需求，覆盖构建验证、header 收缩、dependency seam、executor、runtime state 和 audit 强化。
- `.monkeycode/specs/gc-refactor-follow-up/design.md`：GC 下一轮重构技术方案。
- `.monkeycode/specs/gc-refactor-follow-up/tasklist.md`：GC 下一轮重构实施任务清单。
- `.monkeycode/specs/gc-refactor-follow-up/runtime-validation-checklist.md`：GC 下一轮真实客户端和集成路径验证清单。
- `.monkeycode/specs/gc-refactor-follow-up/runtime-state-storage-evaluation.md`：GC runtime 小状态 storage struct 评估结论。
- `.monkeycode/specs/gc-refactor-follow-up/post-login-registry-candidates.md`：GC post-login registry 下一批 direct-only 迁移候选和排除路径。
- `.monkeycode/specs/gc-refactor-follow-up/internal-header-declaration-inventory.md`：GC internal header 剩余声明分组、迁移和下沉清单。

## 默认验证入口

```bash
tools/run_gc_verification.sh --fast
```

完成阶段前执行完整 wrapper：

```bash
tools/run_gc_verification.sh
```

Wrapper 支持 `--fast`、`--full`、`--skip-audit`、`--skip-style`，可用 `--help` 查看参数说明。

底层离线测试入口仍可单独运行：

```bash
tools/run_gc_offline_tests.sh
```

```bash
tools/run_gc_offline_tests.sh --full
```

```bash
git diff --check
```

涉及声明迁移、新增 helper 或重构边界审计时执行：

```bash
python3 tools/_audit_gc_refactor.py
```

涉及 Premake 配置时，在具备工具环境执行：

```bash
premake5 --with-gc-tests gmake2
```
