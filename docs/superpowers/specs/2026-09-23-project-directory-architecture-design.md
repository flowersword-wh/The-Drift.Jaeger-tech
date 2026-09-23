# The Drift 项目目录与模块架构设计

## 1. 背景

The Drift 是一个运行在 Windows 上的目录同步实验项目。C++ 客户端和服务端通过 Winsock TCP 通信，Rust runner 负责端到端测试。当前实现可工作，但主要业务、协议编解码、网络生命周期和文件系统操作集中在 `client.cpp` 与 `server.cpp`，公共类型也在两个入口内重复定义。

本次重构的目标是在不过度设计的前提下建立明确的职责边界，并为协议校验、安全处理、跨平台适配和更多测试预留扩展空间。

## 2. 范围与兼容要求

本次重构包括：

- 重新组织 C++ 源文件、头文件、Rust 端到端测试和辅助脚本目录。
- 从 `client.cpp`、`server.cpp` 提取同步业务、协议、网络传输和文件系统职责。
- 引入统一共享模型和单向依赖规则。
- 增加路径安全、协议字段和核心比较逻辑的测试能力。

以下行为必须保持不变：

- xmake target 仍命名为 `server`、`client`、`test_runner`。
- `xmake build server client`、`xmake run server`、`xmake run client` 等现有命令继续有效。
- 服务端和客户端仍分别接收一个同步目录参数。
- 默认地址、端口和当前单向补充同步语义保持不变。
- 线上协议字段顺序、字段宽度和当前字节序保持兼容。
- 现有 Rust 端到端用例的业务预期和日志目录语义保持不变。

本次不引入多独立库工程、不设计新协议版本、不增加断点续传或双向删除同步，也不进行跨平台实现。

## 3. 方案选择

### 3.1 备选方案

1. **只按文件类型整理**：把现有 `.cpp` 移入 `src`，保留 `include` 和 `net`。移动成本低，但入口仍会混合业务、协议和网络细节。
2. **按职责划分模块**：建立 `apps`、`sync`、`protocol`、`transport`、`filesystem`，继续使用一个 xmake 工程。边界清楚且迁移成本适中。
3. **拆分独立库和应用**：建立多个 library target 和独立应用目录。扩展性强，但对当前项目规模过重。

采用方案 2。它能解决当前入口膨胀和重复类型问题，同时不引入多库构建复杂度。

## 4. 目标目录结构

```text
.
├─ src/
│  ├─ apps/
│  │  ├─ client_main.cpp
│  │  └─ server_main.cpp
│  ├─ sync/
│  │  ├─ client_sync.cpp
│  │  └─ server_sync.cpp
│  ├─ protocol/
│  │  ├─ overview_codec.cpp
│  │  └─ transfer_codec.cpp
│  ├─ transport/
│  │  ├─ socket.cpp
│  │  └─ winsock_runtime.cpp
│  └─ filesystem/
│     ├─ directory_scan.cpp
│     ├─ file_hash.cpp
│     └─ path_validation.cpp
├─ include/
│  └─ drift/
│     ├─ model/
│     ├─ sync/
│     ├─ protocol/
│     ├─ transport/
│     ├─ filesystem/
│     └─ support/
│        └─ logger.h
├─ tests/
│  ├─ unit/
│  └─ e2e/                  # 现有 Rust runner
├─ scripts/
│  └─ test_runner_run.lua
├─ docs/
└─ xmake.lua
```

`include/drift` 通常与 `src` 的模块结构保持一致。`include/drift/model` 是例外：它只保存跨模块共享的轻量数据类型，不需要对应 `.cpp`。公共头文件统一使用 `#include <drift/...>`，项目符号置于 `drift` 命名空间。实现细节若只在单个 `.cpp` 内使用，则保留在匿名命名空间，不创建公共头文件。

## 5. 模块职责

### 5.1 `apps`

`client_main.cpp` 和 `server_main.cpp` 是薄入口，只负责：

- 解析并校验命令行参数的数量。
- 初始化日志和控制台编码。
- 创建客户端连接或服务端监听连接。
- 调用 `run_client_sync` 或 `run_server_sync`。
- 在顶层捕获异常、记录错误并返回非零退出码。

入口不遍历目录、不计算 hash、不比较快照，也不直接编解码协议字段。

### 5.2 `model`

`include/drift/model` 定义不带网络或磁盘行为的共享数据结构：

- `EntryType`：文件或目录。
- `EntryMetadata`：类型、规范化相对路径、文件大小和 SHA-256。
- `DirectorySnapshot`：按规范化相对路径索引的条目集合。
- `TransferPlan`：客户端比较后需要发送的条目集合。

`filesystem`、`protocol` 和 `sync` 都可以依赖 `model`，但 `model` 不依赖其他项目模块。

### 5.3 `sync`

`client_sync.cpp` 负责：

- 接收服务端目录概览。
- 请求 `filesystem` 扫描客户端目录。
- 比较本地快照和服务端快照并生成传输计划。
- 调用 `protocol` 发送传输计划中的目录和文件。

`server_sync.cpp` 负责：

- 请求 `filesystem` 扫描服务端目录。
- 调用 `protocol` 发送目录概览。
- 接收客户端传来的条目。
- 在路径通过安全校验后创建目录或写入文件。

`sync` 负责编排业务，不实现 socket 完整收发、SHA-256 算法或字段级编解码。

### 5.4 `protocol`

该模块定义同步双方共享的线上编解码规则。`overview_codec.cpp` 负责编解码服务端目录概览；`transfer_codec.cpp` 负责传输计划及文件内容的编解码。该层使用 `model` 中的数据结构，验证条目类型、路径长度、条目数量和消息完整性，但不直接访问磁盘。

为了保持兼容，本次重构不改变现有字段顺序、字段宽度或字节序。协议演进和网络字节序迁移应在后续通过显式版本设计完成。

### 5.5 `transport`

`socket.cpp` 封装：

- `send_all` 和 `recv_all`。
- Winsock 调用失败时的错误码和操作上下文。
- socket 的关闭与移动语义。

`winsock_runtime.cpp` 使用 RAII 管理 `WSAStartup` 和 `WSACleanup`。transport 只负责可靠传输字节，不依赖 `sync`、`protocol` 或 `filesystem` 中的业务类型。

### 5.6 `filesystem`

`directory_scan.cpp` 递归扫描文件与目录，并生成统一的 `DirectorySnapshot` 输入数据。`file_hash.cpp` 计算 SHA-256。`path_validation.cpp` 负责：

- 验证同步根目录存在且为目录。
- 规范化协议中的相对路径。
- 拒绝绝对路径、根路径逃逸和包含 `..` 的路径穿越。
- 将安全相对路径拼接到同步根目录。

该模块不依赖 Winsock。原 `fileoverview.cpp` 不保留为独立概念：路径校验迁入 `path_validation.cpp`，扫描和计数职责迁入 `directory_scan.cpp`，无调用价值的重复计数函数删除。

### 5.7 `tests` 与 `scripts`

现有 `test/runner` 迁入 `tests/e2e`，Rust 源码、用例选择和验证行为保持不变。原 `lua/test_runner_run.lua` 迁入 `scripts/test_runner_run.lua`，xmake 中同步更新路径。

`tests/unit` 放置不依赖真实网络进程的 C++ 单元测试，首批覆盖快照比较、非法相对路径和协议字段校验。

## 6. 依赖规则

允许的主要依赖方向为：

```text
apps ───────→ sync
  │            ├────→ protocol ───→ transport
  │            ├────→ filesystem
  │            └────→ model
  └──────────→ transport

protocol ───→ model
filesystem ─→ model
```

必须遵守以下约束：

- `filesystem` 与 `protocol` 互不依赖；两者只共同依赖 `model` 中的纯数据类型。
- `model` 不依赖任何其他项目模块。
- `transport` 不依赖 `sync`、`protocol` 或 `filesystem`。
- `protocol` 不直接扫描或写入磁盘。
- `apps` 不包含同步决策和协议字段处理。
- 不使用全局 `using namespace std`，不在多个入口重复声明协议类型。

## 7. 数据流

客户端流程：

1. `client_main` 校验参数并建立 TCP 连接。
2. `run_client_sync` 通过 `protocol` 接收服务端 `DirectorySnapshot`。
3. `filesystem` 扫描客户端根目录并生成本地快照。
4. `sync` 比较两份快照，生成 `TransferPlan`。
5. `protocol` 按计划发送目录项或文件项。

服务端流程：

1. `server_main` 校验参数、监听端口并接受一个连接。
2. `filesystem` 扫描服务端根目录。
3. `run_server_sync` 通过 `protocol` 发送服务端快照。
4. `protocol` 接收客户端传输的条目。
5. `filesystem` 校验目标相对路径，并创建目录或写入文件。

## 8. 错误处理

- `transport` 将 Winsock 失败转换为包含操作名和错误码的异常。
- `protocol` 对非法条目类型、超过约定上限的路径、异常条目数量和截断消息立即失败。
- `filesystem` 对扫描、hash、创建目录、打开文件和写入失败提供路径上下文。
- `apps` 在顶层统一捕获异常并返回非零退出码。
- Winsock runtime 和 socket 通过 RAII 自动清理，避免异常路径泄漏资源。
- 服务端在写入前校验所有来自客户端的相对路径，拒绝目标根目录之外的落盘位置。

## 9. 迁移策略

采用小步迁移，每一步均保持 `server` 和 `client` 可构建：

1. 建立共享模型、目录扫描、hash 和路径校验。
2. 封装 Winsock runtime、socket 生命周期和完整收发。
3. 提取概览与文件传输协议编解码。
4. 提取客户端和服务端同步编排。
5. 将两个 `main` 缩减为薄入口。
6. 迁移 Rust runner、Lua 脚本和文档中的路径。
7. 删除已被替代的旧文件并更新 README 目录结构。

每一阶段只切换一类职责；完成切换并通过构建后再删除对应旧实现，避免一次性大搬迁导致难以定位回归。

## 10. 测试与验收

迁移期间每个阶段至少执行 C++ 构建。全部迁移完成后执行：

- `xmake build server client`
- `cargo check --manifest-path tests/e2e/Cargo.toml`
- `xmake build test_runner`
- `xmake run test_runner`

新增单元测试至少验证：

- 两份目录快照在缺失条目、类型不同、大小不同和 hash 不同时生成正确传输计划。
- 安全路径接受正常相对路径，拒绝绝对路径和路径穿越。
- 协议编解码拒绝非法类型和超过上限的路径。

验收条件为：

- 所有构建和测试通过。
- 原有 xmake target、命令行接口、端口与同步结果不变。
- `client_main.cpp`、`server_main.cpp` 不再包含目录遍历、hash 比较或字段级收发循环。
- 各模块满足第 6 节的依赖规则。
- README 与最终目录结构一致。
