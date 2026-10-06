# GC Production Build Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让每个面向 `dev` 的 GC 改动同时经过完整离线验证和实际 Linux 生产目标编译，确保拆分出的 GC 翻译单元会被生成项目并编译。

**Architecture:** 保持现有 `emu-build-all-linux.yml` 作为唯一 Linux 构建实现，通过 `workflow_call` 输入将其矩阵缩小为调用方指定的目标。PR 工作流调用该复用工作流，仅构建 `api_experimental` 的 x64 release 目标；该目标由 Premake 的 `dll/**` 源列表收集所有 GC 翻译单元，覆盖 `gbe_dota_wrapped_custom_game_handlers.cpp`。现有 `gc-verification` job 保持负责离线协议、handler 和审计验证。

**Tech Stack:** GitHub Actions reusable workflows、Premake `gmake`、GNU Make、Bash、C++17 GC offline tests。

## Global Constraints

- 保留 `emu-build-all-linux.yml` 在 push 场景的现有全矩阵默认值。
- PR 生产编译必须执行 `--genproto`，以生成 `net.pb.h` 等生产翻译单元依赖。
- 使用 `api_experimental`、`release`、`x64` 作为 GC 的 Linux PR 编译目标。
- GC 逻辑改动完成后继续运行 `bash tools/run_gc_verification.sh --full --base-sha <base-sha>`。
- 每个任务独立验证并创建一个提交。

---

### Task 1: 参数化 Linux 复用构建工作流

**Files:**
- Modify: `.github/workflows/emu-build-all-linux.yml:3-54`
- Test: `.github/workflows/emu-build-all-linux.yml` 的 `workflow_dispatch` 默认矩阵与 PR 调用矩阵

**Interfaces:**
- Consumes: GitHub Actions `workflow_call.inputs` 字符串输入。
- Produces: `matrix.prj`、`matrix.arch`、`matrix.cfg`，分别来自 JSON 字符串输入。
- Default values: 当前 7 个 project、`["x64", "x86"]`、`["debug", "release"]`。

- [ ] **Step 1: 写入复用工作流输入和默认矩阵**

在 `.github/workflows/emu-build-all-linux.yml` 的 `on.workflow_call` 下添加以下输入，保留当前矩阵的全部默认项目：

```yaml
inputs:
  matrix_prj:
    required: false
    type: string
    default: '["api_regular", "steamclient_regular", "api_experimental", "steamclient_experimental", "tool_lobby_connect", "tool_generate_interfaces", "test_gamepad_linux"]'
  matrix_arch:
    required: false
    type: string
    default: '["x64", "x86"]'
  matrix_cfg:
    required: false
    type: string
    default: '["debug", "release"]'
```

将原有固定 matrix 值替换为：

```yaml
matrix:
  prj: ${{ fromJSON(inputs.matrix_prj) }}
  arch: ${{ fromJSON(inputs.matrix_arch) }}
  cfg: ${{ fromJSON(inputs.matrix_cfg) }}
```

- [ ] **Step 2: 校验 YAML 结构与默认矩阵语义**

运行：

```bash
python3 - <<'PY'
from pathlib import Path

workflow = Path(".github/workflows/emu-build-all-linux.yml").read_text()
for required in (
    "matrix_prj:",
    "matrix_arch:",
    "matrix_cfg:",
    "${{ fromJSON(inputs.matrix_prj) }}",
    "${{ fromJSON(inputs.matrix_arch) }}",
    "${{ fromJSON(inputs.matrix_cfg) }}",
):
    assert required in workflow, required
PY
```

预期：命令以 `0` 退出，默认数组保留当前 7 个 project 和两个架构、两个构建配置。

- [ ] **Step 3: 提交复用工作流参数化改动**

```bash
git add .github/workflows/emu-build-all-linux.yml
git commit -m "ci: parameterize linux build matrix"
```

### Task 2: 在 PR 中执行 GC 的实际 Linux 生产编译

**Files:**
- Modify: `.github/workflows/emu-pull-request.yml:45-75`
- Test: `.github/workflows/emu-pull-request.yml` 的 reusable workflow 调用

**Interfaces:**
- Consumes: Task 1 的 `matrix_prj`、`matrix_arch`、`matrix_cfg` 输入。
- Produces: 名为 `emu-linux-gc-production` 的 PR job，运行 `api_experimental` x64 release 生产构建。
- Coexists with: 现有 `gc-verification` job；两者都只在 `pull_request` 事件运行。

- [ ] **Step 1: 写入受限 Linux PR 构建 job**

在 `.github/workflows/emu-pull-request.yml` 的 `emu-win-release` 后添加：

```yaml
  emu-linux-gc-production:
    name: "linux gc production"
    if: ${{ !cancelled() && github.event_name == 'pull_request' }}
    uses: "./.github/workflows/emu-build-all-linux.yml"
    with:
      matrix_prj: '["api_experimental"]'
      matrix_arch: '["x64"]'
      matrix_cfg: '["release"]'
```

该 job 必须保留复用工作流中的 `--genproto` 调用和 `make config=release_x64 api_experimental`，确保新 GC TU 使用真实生产 headers、生成的 protobuf headers 与链接设置编译。

- [ ] **Step 2: 校验 PR 工作流只请求一个 Linux 构建组合**

运行：

```bash
python3 - <<'PY'
from pathlib import Path

workflow = Path(".github/workflows/emu-pull-request.yml").read_text()
assert "emu-linux-gc-production:" in workflow
assert "matrix_prj: '[\"api_experimental\"]'" in workflow
assert "matrix_arch: '[\"x64\"]'" in workflow
assert "matrix_cfg: '[\"release\"]'" in workflow
PY
```

预期：命令以 `0` 退出；PR 保持 Windows release 构建、GC 离线验证和新增 Linux GC production 编译三个互补门禁。

- [ ] **Step 3: 提交 PR 生产构建门禁**

```bash
git add .github/workflows/emu-pull-request.yml
git commit -m "ci: build gc production target on pull requests"
```

### Task 3: 完成验证与记录 CI 覆盖边界

**Files:**
- Modify: `docs/gc/reason-trace-governance.md`
- Test: `tools/run_gc_verification.sh`

**Interfaces:**
- Consumes: Task 1 和 Task 2 的 workflow 配置。
- Produces: 文档化的验证层次：离线协议/状态/审计由 `gc-verification` 覆盖，生成 protobuf 的生产 C++ 编译由 `emu-linux-gc-production` 覆盖。

- [ ] **Step 1: 增加 CI 覆盖说明**

在 `docs/gc/reason-trace-governance.md` 的验证相关章节添加以下内容：

```markdown
### 生产编译门禁

`gc-verification` 运行 `tools/run_gc_verification.sh --full`，覆盖离线协议、lobby 状态、handler 副作用和重构审计。

`emu-linux-gc-production` 在 pull request 中运行 `api_experimental` 的 x64 release 构建。该复用工作流先运行 Premake `--genproto`，再执行 `make config=release_x64 api_experimental`，用于覆盖全部 `dll/**` 生产翻译单元与生成的 protobuf 头依赖。
```

- [ ] **Step 2: 执行本地 GC 验证和 whitespace 检查**

运行：

```bash
bash tools/run_gc_verification.sh --full --base-sha "$(git merge-base HEAD origin/dev)"
git diff --check
```

预期：GC offline tests、handler tests、8 项重构审计和 whitespace 检查全部通过。

- [ ] **Step 3: 检查 workflow diff 并提交验证文档**

运行：

```bash
git diff --check HEAD~2..HEAD
git diff -- .github/workflows/emu-build-all-linux.yml .github/workflows/emu-pull-request.yml docs/gc/reason-trace-governance.md
git add docs/gc/reason-trace-governance.md
git commit -m "docs(gc): document production compile gate"
```

预期：diff 仅包含 Linux matrix 参数化、PR 生产构建 job 和验证覆盖说明。

## 自检

- 需求覆盖：Task 1 保持 push 的完整 Linux 构建矩阵并提供 PR 限缩接口；Task 2 让 GC PR 生成 protobuf 后编译真实生产目标；Task 3 将离线与生产验证边界写入项目文档并执行完整 GC 验证。
- 占位符检查：任务、文件路径、YAML 值、命令、预期结果和提交信息均已指定。
- 接口一致性：Task 1 产出的三个 workflow inputs 被 Task 2 以相同名称和 JSON 字符串格式消费；Task 3 记录同一 job 名和编译配置。
