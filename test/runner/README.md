# Rust 集成测试运行器

该程序负责构建后的端到端测试编排。被测程序仍然是 C++ 编写的 `server.exe` 和 `client.exe`，Rust runner 负责准备 sandbox、启动两个进程、重定向日志、 等待执行结果、执行校验以及清理超时进程。

## 当前能力

- 分别在服务端和客户端测试目录中启动对应的 exe。
- 为两个进程设置互相隔离的工作目录。
- `DefaultCase` 只创建或打开 server/client 目录，不生成测试文件，供开发者放入自己的测试数据；运行后使用递归对称差和目录 SHA-256 hash 校验两边结果。
- `JustDemo` 测试单个普通文本文件，生成内容为 `hello world` 的 `demo.txt`。
- `EmptyFileCase` 测试零字节文件的传输。
- `MultipleFilesCase` 测试同一轮传输多个文件，以及带空格和标点的文件名。
- `BinaryFileCase` 测试图片、特殊字节和 1 MiB 二进制文件。
- `LongFilenameCase` 测试长文件名和接近平台长度限制的文件名。
- `DirectoryTransferCase` 验证目录传输当前不受支持，这是一个预期失败测试。
- 将各自的同步目录作为命令行参数传给 C++ 程序。
- 将每个进程的 stdout 和 stderr 合并到各自的日志文件。
- 使用统一的 10 秒截止时间，超时后终止并回收进程。
- 根据服务端和客户端的退出状态返回成功或失败。
- 检查服务端是否包含客户端的全部文件。
- 使用模块感知的日志宏输出 runner 自身的运行信息，例如 `[runner]`、`[prepare]` 和 `[process]`。
- 为每个测试生成唯一 `run_id`，隔离本轮 server/client 工作目录和日志目录。
- 测试错误会输出对应的 `run_id`；详细测试说明只在运行错误或校验失败时输出。
- 支持通过 xmake 构建和运行。

进程编排位于 `src/runner.rs`，进程准备和日志路径管理位于 `src/prepare.rs`，文件验证位于 `src/verification.rs`，进程清理位于 `src/process.rs`，测试案例位于 `src/case/`。

每个测试用例的详细目的、输入数据、校验方式和预期结果见 [`TEST_CASES.md`](TEST_CASES.md)。

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
│        ├─ just_demo.rs
│        ├─ empty_file.rs
│        ├─ multiple_files.rs
│        ├─ binary_file.rs
│        ├─ long_filename.rs
│        └─ directory_transfer.rs
├─ server_test/
│  └─ server.exe
└─ client_test/
   └─ client.exe
```

`server_test` 和 `client_test` 是 C++ 可执行文件的部署目录。Default 的输入数据位于 `test/sandbox/default/server` 和 `test/sandbox/default/client`，每轮运行的隔离目录位于对应 sandbox 的 `runs/<run_id>/server` 和 `runs/<run_id>/client`。

## 基础 sandbox 与实际同步目录

`test/sandbox/<case>/server` 和 `test/sandbox/<case>/client` 是测试输入模板目录，不是 C++ 程序实际进行同步的目录。每次运行时，runner 会先在对应 case 下创建唯一的 `runs/<run_id>`，再将基础 sandbox 中的 `server` 和 `client` 内容分别复制到：

```text
test/sandbox/<case>/runs/<run_id>/server
test/sandbox/<case>/runs/<run_id>/client
```

随后，runner 将这两个 `runs/<run_id>` 子目录作为命令行参数传给 `server.exe` 和 `client.exe`。因此，真正发生文件同步、产生同步结果以及执行校验的，是本轮的 `runs/<run_id>/server` 与 `runs/<run_id>/client`；基础 `server` 和 `client` 目录不会被 C++ 程序直接修改或同步。

该流程适用于所有测例，包括 `default`、`just_demo`、`empty_file`、`multiple_files`、`binary_file`、`long_filename` 和 `directory_transfer`。普通测例会先在基础 sandbox 的 client 目录中生成 fixture，再复制到本轮 run 目录；`default` 则直接复制开发者预先放入基础 server/client 目录中的内容。

其中，`default` 使用保留输入的方式打开基础 sandbox，运行结束后基础目录仍会保留；其他测例每次运行会重新创建对应的基础 sandbox。测试结束后如需查看本轮输入和输出，应根据控制台中的 `run_id` 检查 `test/sandbox/<case>/runs/<run_id>/`，而不是只查看基础 `server` 和 `client` 目录。

`DefaultCase` 使用非破坏性方式打开 default sandbox：不存在时自动创建，已存在且为目录时保留全部内容；如果路径存在但不是目录，则返回错误。Default 运行结束后会对 server/client 运行目录执行递归对称差校验和完整目录树 SHA-256 hash 校验。需要清理内容的独立测试可以使用 `SandboxManager::create_sandbox()`。

## Default 的使用方式

Default 是最常用的测试案例，适合开发者直接准备输入文件并运行完整的 C++ server/client 流程。测试前，将服务端和客户端的初始文件分别放入 `test/sandbox/default/server` 和 `test/sandbox/default/client`，然后运行 `xmake run test_runner`。

runner 会自动创建缺失的目录，不会删除或覆盖 default sandbox 中已有的文件。server 和 client 使用不同的同步目录，并通过命令行参数接收目录路径；测试结束后，server 和 client 运行目录必须具有完全一致的目录项和文件内容。

Default 主要用于手动准备测试数据、复现同步问题和验证普通 C++ 程序行为。开发者可以在两次运行之间修改 sandbox 内容，不需要修改 Rust 测试代码，也不需要重新生成 fixture。

Default 的限制如下：

- sandbox 内容会跨运行保留，前一次测试留下的文件可能影响下一次结果，需要开发者自行清理或替换。
- 通用校验会递归检查 server 是否包含 client 的全部目录项；`DefaultCase` 额外要求两边目录项对称一致，并比较整个目录树的 SHA-256 hash。
- Default 的对称差校验要求 server 和 client 的目录项完全一致，server 中存在额外文件会导致失败。
- server 和 client 固定使用 TCP 端口 `8080`，不能安全地并行运行多个 default 测试。
- client 的 stdin 设置为 `null`，需要交互式输入的程序暂不适用。
- 可通过 `--case` 选择测试案例，并可通过 `--verbose` 输出调试日志。

命令行示例：

```powershell
# 运行全部测试案例（默认行为）
xmake run test_runner

# 只运行一个案例
xmake run test_runner -- --case default

# 运行多个案例并输出调试日志
xmake run test_runner -- --case default --case binary_file --verbose
```

## 其他测试案例

除 Default 外，runner 会按顺序执行 `just_demo`、`empty_file`、`multiple_files`、`binary_file`、`long_filename` 和 `directory_transfer`。这些测试分别位于 `src/case/` 下的独立文件中，每个 case 负责准备自己的输入并执行自己的内容校验。

`directory_transfer` 会在客户端目录中创建多层子目录和文件。由于当前 C++ 协议不携带文件的相对路径，该测试预期失败；runner 会将其记录为 expected error，但不会因此返回失败退出码。其他 case 的进程异常退出、超时或校验失败均属于实际错误。

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

每次运行会在项目根目录的 `logs/` 下按测试和 `run_id` 创建隔离日志：

```text
logs/<case>/<run_id>/server.log
logs/<case>/<run_id>/client.log
```

每个文件包含对应进程的 stdout 和 stderr。`prepare()` 会将实际日志路径返回给 runner，因此进程失败或校验失败时可以读取正确的日志文件；读取失败也会明确记录，不会静默忽略。runner 的控制台日志会显示源文件模块名和本轮 `run_id`，例如 `[runner]` 和 `[prepare]`。

输出路径可能带有 Windows 的 `\\?\` 前缀，例如：

```text
\\?\D:\code\C++\The-Drift.Jaeger-tech\logs\default\<run_id>\server.log
```

这是 Rust `canonicalize()` 返回的 Windows 扩展长度路径，不是错误。

## 执行结果

当两个进程都在统一截止时间前以成功状态退出，通用递归校验通过，且当前测试案例的专属校验通过时，runner 返回退出码 `0`。Default 还要求目录项对称一致并且两边目录树的 SHA-256 hash 相同。出现以下任一情况时返回失败：

- 无法创建测试数据或日志文件。
- 无法启动服务端或客户端。
- 任一进程返回非零退出码。
- 任一进程超过统一的 10 秒截止时间仍未退出。
- server 缺少 client 中的文件。
- 测试案例专属校验失败，例如 Default 的目录对称差或 hash 校验失败。

测试失败后先根据控制台中的 `run_id` 定位 `logs/<case>/<run_id>/`，再检查其中的 server/client 日志和对应的 `test/sandbox/<case>/runs/<run_id>/`。如果双方都显示连接成功但最终超时，通常意味着应用层协议的发送顺序、长度字段或消息边界不一致，而不是 TCP 连接本身失败。


## 当前限制

- 服务端和客户端仍固定使用端口 `8080`，因此不能安全地并行运行多个测例。
- client 的 stdin 当前设置为 `null`；如果程序要求交互式输入，需要调整 runner。
- `verify_files()` 的递归对称差异校验已接入 Default 流程。
- default sandbox 中的测试数据由开发者自行准备，runner 不会清理。
- 未指定 `--case` 时运行全部测试案例；`--case` 可重复指定。
- `--verbose` 开启 DEBUG 级别日志。

## 清理生成物

`xmake clean` 会删除项目根目录的 `logs` 和 `test/sandbox`。Default sandbox 的内容会跨运行保留，因此如果只想清理 Default 的输入数据，应手动删除或替换对应文件；不要依赖 runner 自动清理。
