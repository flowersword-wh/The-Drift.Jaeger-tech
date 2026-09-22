#include <filesystem>
#define WIN32_LEAN_AND_MEAN

#include "net/socket_transfer.h"
#include "include/fileoverview.h"
#include "include/logger.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "include/filehash.h"

#define BUF_SIZE 256
#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[])
{
	Logger logger;
	// 命令行输入要同步的目录
	if (argc != 2) {
		logger.error("Missing synchronization folder path.");
		logger.error("Usage: " + std::string(argv[0]) + " <sync-folder>");
		return 1;
	}
	std::string folderpath = argv[1];
	// 校验目录是否存在、是否为文件夹
	if (!is_CorrectPath(folderpath)) {
		logger.error("create overviewfile failed");
		return 0;
	};

	// 启动程序 初始化Winsock
	logger.info("Server starting...");
	logger.info("Setting console output code page to UTF-8...");
	SetConsoleOutputCP(CP_UTF8);
	logger.info("Console output code page set to UTF-8.");
	logger.info("Initializing Winsock...");
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		throw std::runtime_error("WSAStartup failed");
	}
	logger.info("Winsock initialized.");

	// 1. 创建监听套接字 (AF_INET=IPv4, SOCK_STREAM=TCP)
	SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == INVALID_SOCKET) {
		throw std::runtime_error("socket failed: " +
														 std::to_string(WSAGetLastError()));
	}
	// 2. 设置端口复用 (关键：必须在 bind 之前)
	int opt = 1;
	// SOL_SOCKET: 套接字层  SO_REUSEADDR: 允许重用本地地址
	judge(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
									 reinterpret_cast<const char *>(&opt), sizeof(opt)),
				"setsockopt");

	// 3. 准备地址结构体，绑定端口 8080]
	sockaddr_in sockaddr_in_t{};
	sockaddr_in_t.sin_family = AF_INET;
	sockaddr_in_t.sin_port = htons(8080);
	sockaddr_in_t.sin_addr.s_addr = htonl(INADDR_ANY);

	// 4. 绑定端口
	judge(bind(server_fd, (sockaddr *) &sockaddr_in_t, sizeof(sockaddr_in_t)),
				"bind");

	// 5. 开始监听 (第二个参数是未完成连接队列的大小，通常设为
	// SOMAXCONN,表示让系统使用一个合理的最大等待队列长度)
	judge(listen(server_fd, SOMAXCONN), "listen");
	logger.info("Listening...");

	// 6. 接受客户端连接
	int len = sizeof(sockaddr_in_t);
	SOCKET client_fd = accept(server_fd, (sockaddr *) &sockaddr_in_t, &len);
	if (client_fd == INVALID_SOCKET) {
		throw std::runtime_error("accept failed: ");
	}
	logger.info("Connection established.");

	// 读取文件夹种所有的项 （空目录和文件）
	std::uint32_t serverEntryCount = 0;
	for (const auto &entry : fs::recursive_directory_iterator(folderpath)) {
		if (entry.is_regular_file() || entry.is_directory()) {
			++serverEntryCount;
		}
	}
	// 发送服务端项数
	Sender(client_fd, &serverEntryCount, sizeof(serverEntryCount),
				 "send serverEntryCount failed");
	// 区分文件和空目录
	enum class EntryType : std::uint8_t {
		File = 1,
		Directory = 2,
	};
	// 发送文件夹内容
	for (const auto &entry : fs::recursive_directory_iterator(folderpath)) {
		// 发送文件相对路径
		std::string relativePath =
				entry.path().lexically_relative(folderpath).string();
		uint32_t pathSize = (uint32_t) (relativePath.size());

		Sender(client_fd, &pathSize, sizeof(pathSize),
					 "send serverfile pathsize failed");
		Sender(client_fd, relativePath.data(), pathSize,
					 "send serverfile relativepath failed");
		EntryType entryType;
		// 通过 entryType 区别发送的内容
		if (entry.is_regular_file()) {
			entryType = EntryType::File;
			std::uint8_t typeValue = (std::uint8_t) entryType;
			std::uint64_t filesize = entry.file_size();
			Sha256 file_hash;
			// 发送文件类型值 ——> 文件大小 ——> 文件哈希值
			Sender(client_fd, &typeValue, sizeof(typeValue),
						 "send file typevalue failed");
			Sender(client_fd, &filesize, sizeof(filesize), "send filesize failed");
			if (!calculate_hash(entry.path(), file_hash)) {
				throw std::runtime_error("calculate file hash failed");
			}
			Sender(client_fd, &file_hash, (int) file_hash.size(),
						 "send file hash failed");
		} else if (entry.is_directory()) {
			// 发送文件类型值
			entryType = EntryType::Directory;
			std::uint8_t typeValue = std::uint8_t(entryType);
			Sender(client_fd, &typeValue, sizeof(typeValue),
						 "send file typevalue failed");
		}
	}
	// 接收客户端发送的缺失文件数
	std::uint32_t fileCount;
	Receiver(client_fd, (char *) &fileCount, sizeof(fileCount),
					 "fileCount receive failed");

	// 接收客户端发送的文件
	logger.info("Waiting for files from client...");

	while (fileCount--) {

		std::uint64_t filesize;
		std::uint8_t typeValue = 0;
		// 接收文件类型值
		Receiver(client_fd, &typeValue, sizeof(typeValue),
						 "typeValue receive failed");
		// 接收文件相对路径大小
		std::uint32_t pathSize;
		Receiver(client_fd, &pathSize, sizeof(pathSize), "receive pathSize failed");
		// 接收文件相对路径
		std::string relativePath(pathSize, '\0');
		Receiver(client_fd, relativePath.data(), pathSize,
						 "receive relativePath failed");
		if (typeValue == (std::uint8_t) EntryType::File) {
			// 接收文件大小
			Receiver(client_fd, &filesize, sizeof(filesize),
							 "filesize receive failed");
			logger.info("Received file size: " + std::to_string(filesize));

			// 接收文件内容
			fs::path relative = fs::path(relativePath).make_preferred();
			fs::path savepath = fs::path(folderpath) / relative;
			logger.info("Saving file to: " + savepath.string());
			fs::create_directories(savepath.parent_path());
			std::ofstream file(savepath, std::ios::binary | std::ios::trunc);
			if (!file) {
				throw std::runtime_error("file open failed");
			}

			char buffer[256];
			std::uint64_t remain = filesize;
			while (remain > 0) {
				int min = (int) (std::min<std::uint64_t>(remain, sizeof(buffer)));
				Receiver(client_fd, buffer, min, "file receive error");
				file.write(buffer, min);
				if (!file) {
					throw std::runtime_error("file write failed");
				}
				remain -= min;
			}

			logger.info("Expected bytes to write: " + std::to_string(filesize) +
									" B");
			logger.info("Bytes written: " + std::to_string(filesize - remain) + " B");
		} else if (typeValue == (std::uint8_t) EntryType::Directory) {
			fs::path relative = fs::path(relativePath).make_preferred();
			fs::path savepath = fs::path(folderpath) / relative;
			fs::create_directories(savepath);
		}
	}
	logger.info("All files received.");
	// 关闭连接
	shutdown(server_fd, SD_BOTH);
	closesocket(server_fd);
	shutdown(client_fd, SD_BOTH);
	closesocket(client_fd);
	WSACleanup();
	logger.info("Connection closed.");
	return 0;
}
