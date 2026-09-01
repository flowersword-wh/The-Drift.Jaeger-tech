# Rust 集成测试运行器

该程序负责构建后的端到端测试编排。被测程序仍然是 C++ 编写的`server.exe` 和 `client.exe`，`Rust runner` 负责准备测试数据、启动两个进程、重定向日志、等待执行结果以及清理超时进程。

## 当前能力

- 分别在服务端和客户端测试目录中启动对应的 exe。
- 为两个进程设置互相隔离的工作目录。
- 自动在客户端目录生成内容为 `hello world` 的 `demo.txt`。
- 通过管道向服务端标准输入写入项目根目录。
- 将每个进程的 stdout 和 stderr 合并到各自的日志文件。
- 进程超过 10 秒未退出时自动终止，避免留下后台进程。
- 根据服务端和客户端的退出状态返回成功或失败。
- 支持通过 xmake 构建和运行。

当前只有一个直接写在 `src/main.rs` 中的测试流程。后续会将测试用例拆分到独立的 `cases` 模块和 fixture 目录中。

## 目录布局

```text
test/
├─ runner/
│  ├─ Cargo.toml
│  ├─ Cargo.lock
│  ├─ README.md
│  └─ src/
│     └─ main.rs
├─ server_test/
│  ├─ server.exe
│  └─ server.log
└─ client_test/
   ├─ client.exe
   ├─ demo.txt
   └─ client.log
```

`server_test` 和 `client_test` 同时是部署目录和进程工作目录。C++ 程序中使用的相对路径会以各自的工作目录为基准。

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

`test_runner` 依赖 `server` 和 `client`。构建时，`xmake` 会先生成两个 `C++` 程序并将其复制到对应测试目录，然后调用 Cargo 构建 runner。

Release 模式：

```powershell
xmake f -m release
xmake build test_runner
xmake run test_runner
```

## 直接使用 Cargo

只检查或构建 runner：

```powershell
cargo check --manifest-path test\runner\Cargo.toml
cargo build --manifest-path test\runner\Cargo.toml
```

直接运行前，需要确保以下文件已经存在：

```text
test/server_test/server.exe
test/client_test/client.exe
```

然后执行：

```powershell
cargo run --manifest-path test\runner\Cargo.toml
```

## 日志

每次运行会覆盖上一次日志：

```text
test/server_test/server.log
test/client_test/client.log
```

每个文件包含对应进程的 stdout 和 stderr。runner 当前还会在进程结束后把日志内容输出到终端，方便立即查看失败位置。

输出路径可能带有 Windows 的 `\\?\` 前缀，例如：

```text
\\?\D:\code\C++\The-Drift.Jaeger-tech\test\server_test\server.log
```

这是 Rust `canonicalize()` 返回的 Windows 扩展长度路径，不是错误。

## 执行结果

当两个进程都在超时前以成功状态退出时，runner 返回退出码 `0`。出现以下任一情况时返回失败：

- 无法创建测试数据或日志文件。
- 无法启动服务端或客户端。
- 无法向服务端写入测试目录。
- 任一进程返回非零退出码。
- 任一进程超过 10 秒仍未退出。

测试失败后优先检查 `server.log` 和 `client.log`。如果双方都显示连接成功但最终超时，通常意味着应用层协议的发送顺序、长度字段或消息边界不一致，而不是 TCP 连接本身失败。


## 当前限制

- 服务端和客户端仍固定使用端口 `8080`，因此不能安全地并行运行多个测例。
- 客户端 stdin 当前设置为 `null`；客户端如果要求交互式输入，需要改为由 runner 通过管道提供。
- 测试数据和期望结果尚未抽象为独立测例。
- 当前测试会覆盖 `client_test/demo.txt` 和已有日志文件。

## 后续拆分建议

```text
src/
├─ main.rs          # 命令行解析与结果汇总
├─ process.rs       # 进程启动、日志和超时
├─ runner.rs        # 测例执行流程
├─ sandbox.rs       # 隔离目录和 fixture 部署
├─ test_case.rs     # TestCase、Expectation 等类型
├─ verification.rs  # 文件和退出状态验证
└─ cases/
   ├─ mod.rs
   └─ basic_transfer.rs
```

`main.rs` 最终只负责选择测例并调用 runner；测试输入、stdin、超时和期望结果由各个测例独立描述。
