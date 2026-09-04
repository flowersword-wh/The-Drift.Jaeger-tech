# The-Drift.Jaeger-tech

一个基于 Windows Winsock TCP 的目录文件同步实验项目。服务端先发送目标目录的文件概览，客户端比较自己的目录内容后，把服务端缺少的普通文件发送过去。

> 当前实现是单向补充同步：文件从客户端发送到服务端，不会删除服务端文件，也不会把服务端独有的文件发送回客户端。

## 功能

- C++17 实现 TCP 服务端和客户端。
- 服务端监听 `8080` 端口，客户端连接本机回环地址 `127.0.0.1:8080`。
- 通过命令行参数指定两端的同步目录。
- 使用 `fileoverview.txt` 记录目录树中发现的文件名；当前协议尚未保留相对路径。
- 支持传输普通文件及空文件。
- 提供 `fileoverview` 工具，用于手动生成目录概览。
- 提供 Rust 集成测试 runner，可自动构建、启动和验证服务端与客户端。
- Rust runner 支持通过 `--case` 选择测试用例，并通过 `--verbose` 输出调试日志。

## 环境要求

- Windows
- [xmake](https://xmake.io/)
- C++17 编译环境，推荐 MSVC
- Rust 工具链（运行集成测试时需要 `cargo`）

检查工具链：

```powershell
xmake --version
rustc --version
cargo --version
```

## 构建

在项目根目录执行：

```powershell
# 构建服务端、客户端和目录概览工具
xmake build server client fileoverview
```

构建产物通常位于 `build/windows/x64/<配置>/` 目录中，具体路径以 xmake 输出为准。

构建 Release 版本：

```powershell
xmake f -m release
xmake build server client fileoverview
```

## 运行

服务端需要指定一个已存在的目录：

```powershell
xmake run server -- C:\path\to\server-folder
```

客户端也需要指定一个已存在的目录：

```powershell
xmake run client -- C:\path\to\client-folder
```

使用时先启动服务端，再启动客户端。两端必须使用不同的工作目录，并确保 `8080` 端口未被其他程序占用。

也可以直接运行生成的 `server.exe` 和 `client.exe`：

```text
server.exe <server-folder>
client.exe <client-folder>
```

程序会在当前工作目录生成或覆盖 `fileoverview.txt`。服务端接收文件时，若目标文件已存在，会直接覆盖。

## 集成测试

`test/runner` 是 Rust 编写的端到端测试编排器。xmake 会先构建 C++ 的 `server.exe` 和 `client.exe`，并将它们复制到测试目录，然后构建并运行 Rust runner：

```powershell
xmake build test_runner
xmake run test_runner
```

默认会运行全部测试用例。也可以选择一个或多个用例：

```powershell
# 运行单个用例
xmake run test_runner -- --case default

# 运行多个用例并输出 DEBUG 日志
xmake run test_runner -- --case default --case binary_file --verbose
```

Release 模式：

```powershell
xmake f -m release
xmake build test_runner
xmake run test_runner
```

也可以只构建 Rust runner：

```powershell
cargo check --manifest-path test\runner\Cargo.toml
cargo build --manifest-path test\runner\Cargo.toml
```

测试日志位于：

```text
logs/<case>/<run_id>/server.log
logs/<case>/<run_id>/client.log
```

runner 默认依次执行 `default`、`just_demo`、`empty_file`、`multiple_files`、`binary_file`、`long_filename` 和 `directory_transfer` 测试。通过重复指定 `--case` 可以选择部分用例，执行顺序仍由 runner 统一控制。每次执行都会生成唯一的 `run_id`，用于隔离同步目录和日志；测试失败时，控制台会输出该 ID 和测试说明。`directory_transfer` 当前用于确认目录传输尚未支持，因此预期失败不会使整个测试流程失败。

runner 在进程完成后会递归检查 server 运行目录是否包含 client 运行目录中的全部目录项，但通用校验不比较文件内容。`default` 还会执行目录项对称差校验和完整目录树 SHA-256 hash 校验；因此 Default 要求两边目录结构和文件内容完全一致。

集成测试出现错误时，优先查看 [`test/runner/TEST_CASES.md`](test/runner/TEST_CASES.md)，确认对应测试的目的、输入数据、校验方式和预期结果，再根据控制台输出的 `run_id` 检查对应日志。

测试 runner 默认在进程超过 10 秒未退出时终止进程。执行 `xmake clean` 会删除测试生成的 `logs` 和 `test/sandbox` 目录，不会删除源代码。

## 目录结构

```text
.
├─ client.cpp                 # TCP 客户端
├─ server.cpp                 # TCP 服务端
├─ fileoverview.cpp           # 目录概览生成逻辑
├─ fileoverview_main.cpp      # 目录概览命令行工具
├─ include/
│  ├─ fileoverview.h
│  └─ logger.h
├─ test/runner/               # Rust 集成测试 runner
├─ docs/                      # 设计与测试相关文档
└─ xmake.lua                  # xmake 构建配置
```

## 当前限制

- C++ 同步协议虽然会遍历子目录，但传输时只使用文件名而不携带相对路径，因此尚未实现可靠的目录树同步；同名文件也可能发生冲突。
- Rust runner 的边界测试会分别覆盖空文件、多文件、二进制文件、大文件和长文件名；目录传输由独立的预期失败测试覆盖。
- Rust runner 的通用校验会递归检查 server 是否包含 client 的目录项，但默认不比较文件内容。
- `default` 会额外使用递归对称差和目录 SHA-256 hash 校验，因此 server 不能包含 client 之外的额外目录项或文件。
- 服务端和客户端固定使用 TCP `8080` 端口。
- 协议未提供文件校验、断点续传或加密能力。
- 接收到的文件名来自客户端，生产环境使用前应补充路径校验和更严格的协议校验。

## 相关文档

- [文件夹同步与测试框架设计](docs/tesr.md)
- [Rust 集成测试 runner 说明](test/runner/README.md)
