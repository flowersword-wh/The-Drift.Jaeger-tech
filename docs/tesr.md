# 文件夹同步与测试框架设计

本文记录当前实现及测试边界。同步方向为客户端向服务端补充或更新目录项；服务端不会删除独有内容，也不会向客户端回传文件。

## 1. 组成与流程

`server.exe <server-folder>` 和 `client.exe <client-folder>` 分别接收已存在的同步根目录。服务端在 TCP `8080` 端口监听，客户端连接 `127.0.0.1:8080`。两端递归遍历目录树，仅处理普通文件和目录。

```text
服务端遍历目录并发送已有项清单
             ↓
客户端按相对路径、类型、大小及 SHA-256 比较
             ↓
客户端发送缺失或不同的目录项
             ↓
服务端创建目录或写入文件
```

服务端清单直接通过 TCP 发送，不生成 `fileoverview.txt`。`fileoverview.cpp` 当前只提供目录参数检查和普通文件计数辅助函数；xmake 没有独立的 `fileoverview` 工具 target。

## 2. 路径与比较规则

目录项使用相对于同步根目录的路径。客户端发送路径时使用 `generic_string()`，以 `/` 分隔；服务端接收后转为本机路径。目录作为独立项参与同步，所以空目录也能创建。符号链接等非普通文件和非目录项不在当前同步范围。

服务端清单包含每项的相对路径与类型；普通文件还包含 64 位文件大小和 32 字节 SHA-256 摘要。客户端按相对路径查找：服务端无此项、类型不同、文件大小不同或摘要不同，都会把本地项加入待发送清单。类型和路径相同的目录无需重传；大小及摘要均相同的文件无需重传。

服务端接收目录项后调用 `create_directories()`；接收文件前创建父目录，再以二进制截断模式写入。同路径旧文件会被客户端版本覆盖。当前代码没有处理服务端独有项的删除，也没有定义同路径文件与目录类型冲突时的恢复策略。

## 3. TCP 消息格式

字段按当前 C++ 实现的原生整数表示发送，双方必须使用兼容的布局和字节序。`u8` 类型值为 `1`（普通文件）或 `2`（目录）。

| 阶段 | 字段顺序 |
| --- | --- |
| 服务端清单 | `u32` 项数；每项为 `u32` 路径字节数、路径字节、`u8` 类型；文件项再附 `u64` 文件大小、32 字节 SHA-256 |
| 客户端待发送项 | `u32` 项数；每项为 `u8` 类型、`u32` 路径字节数、路径字节；文件项再附 `u64` 文件大小、对应字节数的文件内容 |

`net/socket_transfer.cpp` 的 `Sender` / `Receiver` 会循环调用 Winsock `send` / `recv`，直到指定长度全部收发或出错。文件内容分块传输；传输完成后没有独立的文件摘要复核。

## 4. Rust 集成测试框架

`test/runner` 通过 xmake 依赖 C++ `server`、`client`，构建后启动两个进程并传入各自的同步目录。runner 为每个用例生成唯一 `run_id`，把基础输入复制到 `test/sandbox/<case>/runs/<run_id>/{server,client}`，并将 stdout/stderr 写入 `logs/<case>/<run_id>/{server,client}.log`。两个进程共用 10 秒截止时间；超时或非零退出会使用例失败。

执行顺序固定为 `default`、`just_demo`、`empty_file`、`multiple_files`、`binary_file`、`long_filename`、`directory_transfer`、`comprehensive`。可重复使用 `--case` 筛选，用 `--verbose` 打开 DEBUG 日志。`default` 保留开发者放入基础 sandbox 的输入；其他用例自行准备 fixture。

进程成功退出后，通用校验递归确认服务端包含客户端的所有目录项，但不比较内容。各用例的 `verify()` 再做对应的内容检查：`default`、`directory_transfer` 和 `comprehensive` 比较目录项及完整目录树 SHA-256；其他验证方式见 [测试用例详细说明](../test/runner/TEST_CASES.md)。其中 `directory_transfer` 覆盖多层路径，`comprehensive` 覆盖大文件、二进制、大小边界及已有文件更新。

```powershell
xmake build test_runner
xmake run test_runner
xmake run test_runner -- --case directory_transfer --verbose
```

## 5. 当前限制与后续设计点

- 服务端尚未验证客户端路径是否为绝对路径、是否含有 `..` 或是否最终位于同步根目录内。接入不可信客户端前需要路径安全校验。
- 服务端未限制收到的路径长度；客户端接收服务端清单时已有 4096 字节上限，但这不是双方对称的协议约束。
- 消息没有版本号、认证或加密，也没有文件传输后的独立摘要复核和断点续传。
- 固定端口使多个集成用例不能安全并行运行；现有 runner 顺序执行。
- 现有测试没有专门覆盖空目录、路径穿越、同路径类型冲突、符号链接及网络中断恢复。