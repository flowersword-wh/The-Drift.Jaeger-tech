# Rust 集成测试运行器

该程序负责构建后的端到端测试编排。被测程序仍然是 C++ 编写的 `server.exe` 和 `client.exe`，Rust runner 负责准备 sandbox、启动两个进程、重定向日志、 等待执行结果、执行校验以及清理超时进程。

## 当前能力

- 分别在服务端和客户端测试目录中启动对应的 exe。
- 为两个进程设置互相隔离的工作目录。
- 默认使用 `DefaultCase`，只创建或打开 server/client 目录，不生成测试文件。
- 保留 `JustDemo`，用于生成内容为 `hello world` 的 `demo.txt`。
- 将各自的同步目录作为命令行参数传给 C++ 程序。
- 将每个进程的 stdout 和 stderr 合并到各自的日志文件。
- 使用统一的 10 秒截止时间，超时后终止并回收进程。
- 根据服务端和客户端的退出状态返回成功或失败。
- 检查服务端是否包含客户端的全部文件。
- 使用全局 `LOGGER` 输出 runner 自身的运行信息。
- 支持通过 xmake 构建和运行。

进程编排位于 `src/runner.rs`，进程准备和日志路径管理位于 `src/prepare.rs`， 文件验证位于 `src/verification.rs`，测试案例位于 `src/case/`。

## 目录布局

```text
test/
├─ runner/
│  ├─ Cargo.toml
│  ├─ Cargo.lock
│  ├─ README.md
│  └─ src/
│     ├─ main.rs
│     ├─ log.rs
│     ├─ runner.rs
│     ├─ prepare.rs
│     ├─ sandbox.rs
│     ├─ verification.rs
│     └─ case/
│        ├─ mod.rs
│        ├─ default.rs
│        └─ just_demo.rs
├─ server_test/
│  └─ server.exe
└─ client_test/
   └─ client.exe
```

`server_test` 和 `client_test` 是 C++ 可执行文件的部署目录和进程工作目录。 实际同步数据位于 `test/sandbox/default/server` 和 `test/sandbox/default/client`。

`DefaultCase` 使用非破坏性方式打开 default sandbox：不存在时自动创建，已存在且为目录时保留全部内容；如果路径存在但不是目录，则返回错误。需要清理内容的独立测试可以使用 `SandboxManager::create_sandbox()`。

## Default 的使用方式

Default 是最常用的测试案例，适合开发者直接准备输入文件并运行完整的 C++ server/client 流程。测试前，将服务端和客户端的初始文件分别放入 `test/sandbox/default/server` 和 `test/sandbox/default/client`，然后运行 `xmake run test_runner`。

runner 会自动创建缺失的目录，不会删除或覆盖 default sandbox 中已有的文件。server 和 client 使用不同的同步目录，并通过命令行参数接收目录路径；测试结束后，服务端目录中应至少包含客户端目录中的全部文件。

Default 主要用于手动准备测试数据、复现同步问题和验证普通 C++ 程序行为。开发者可以在两次运行之间修改 sandbox 内容，不需要修改 Rust 测试代码，也不需要重新生成 fixture。

Default 的限制如下：

- sandbox 内容会跨运行保留，前一次测试留下的文件可能影响下一次结果，需要开发者自行清理或替换。
- 当前只校验直接子项的文件名，不比较文件内容，也不递归校验子目录。
- 默认校验只要求 server 包含 client 的文件，server 中存在额外文件不会导致失败。
- server 和 client 固定使用 TCP 端口 `8080`，不能安全地并行运行多个 default 测试。
- client 的 stdin 设置为 `null`，需要交互式输入的程序暂不适用。
- 当前 runner 尚未接入 Clap，因此还不能通过命令行选择其他测试案例。

## 环境要求

- Windows
- Rust 工具链（`cargo` 和 `rustc`）
- xmake
- 能够构建本项目 C++ 程序的 MSVC 环境

检查 Rust 环境：

```powershell
rustc --version
cargo --version
```

## 使用 xmake

在项目根目录执行：

```powershell
# 构建 server、client 和 Rust runner
xmake build test_runner

# 构建并运行集成测试
xmake run test_runner
```

`test_runner` 依赖 `server` 和 `client`。构建时，xmake 会先生成两个 C++ 程序 并将其复制到对应测试目录，然后调用 Cargo 构建 runner。

Release 模式：

```powershell
xmake f -m release
xmake build test_runner
xmake run test_runner
```

## 日志

每次运行会在项目根目录的 `logs/` 下创建带时间戳的日志：

```text
logs/server-2026-09-03_15-30-00.log
logs/client-2026-09-03_15-30-00.log
```

每个文件包含对应进程的 stdout 和 stderr。`prepare()` 会将实际日志路径返回给 runner，因此进程失败或校验失败时可以读取正确的日志文件；读取失败也会明确 记录，不会静默忽略。

输出路径可能带有 Windows 的 `\\?\` 前缀，例如：

```text
\\?\D:\code\C++\The-Drift.Jaeger-tech\logs\server-2026-09-03_15-30-00.log
```

这是 Rust `canonicalize()` 返回的 Windows 扩展长度路径，不是错误。

## 执行结果

当两个进程都在统一截止时间前以成功状态退出，且 server 包含 client 的全部文件时， runner 返回退出码 `0`。出现以下任一情况时返回失败：

- 无法创建测试数据或日志文件。
- 无法启动服务端或客户端。
- 任一进程返回非零退出码。
- 任一进程超过统一的 10 秒截止时间仍未退出。
- server 缺少 client 中的文件。

测试失败后优先检查 `logs/` 下最新的 server/client 日志。如果双方都显示连接 成功但最终超时，通常意味着应用层协议的发送顺序、长度字段或消息边界不一致， 而不是 TCP 连接本身失败。


## 当前限制

- 服务端和客户端仍固定使用端口 `8080`，因此不能安全地并行运行多个测例。
- client 的 stdin 当前设置为 `null`；如果程序要求交互式输入，需要调整 runner。
- `verify_files()` 的对称差异校验已保留，但当前默认流程暂未启用。
- default sandbox 中的测试数据由开发者自行准备，runner 不会清理。
- 当前 runner 尚未真正解析命令行参数。
