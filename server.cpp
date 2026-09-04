#include <exception>
#include <filesystem>
#define WIN32_LEAN_AND_MEAN

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
#include <stdio.h>

#define BUF_SIZE 256
#pragma comment(lib, "ws2_32.lib")

int judge(int result, const std::string &message)
{
	if (result == SOCKET_ERROR) {
		int error = WSAGetLastError();

		throw std::runtime_error(message +
														 " failed, WSA error: " + std::to_string(error));
	}

	return 0;
}
bool sendAll(SOCKET fd, const void *data, int len)
{
	int sent = 0;
	const char *bytes = (char *) (data);
	while (sent < len) {
		int result = send(fd, bytes + sent, len - sent, 0);
		if (result <= 0) {
			return false;
		}
		sent += result;
	}
	return true;
}

bool recvAll(SOCKET fd, void *data, int len)
{
	int received = 0;
	char *bytes = (char *) (data);
	while (received < len) {
		int result = recv(fd, bytes + received, len - received, 0);
		if (result <= 0) {
			return false;
		}
		received += result;
	}
	return true;
}
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
	// 递归遍历读取所有文件
	recursive_directory_reader(folderpath);

	// 得到概览文件大小
	std::uint64_t overviewSize;
	FILE *fp;
	fp = fopen("fileoverview.txt", "rb");
	if (!fp) {
		logger.error("file open failed");
		return -1;
	}
	std::fseek(fp, 0, SEEK_END);
	overviewSize = ftell(fp);
	fclose(fp);

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

	// 发送概览文件
	logger.info("Sending server folder overview...");
	// 发送概览文件大小
	if (!sendAll(client_fd, &overviewSize, sizeof(overviewSize))) {
		throw std::runtime_error("overviewSize send failed");
	}
	// 发送概览文件内容
	char buffer[256];
	std::ifstream fileview("fileoverview.txt", std::ios::binary);
	if (!fileview) {
		throw std::runtime_error("file open failed");
	}
	while (fileview.read(buffer, sizeof(buffer)) || fileview.gcount() > 0) {
		std::streamsize count = fileview.gcount();
		if (count > 0) {
			if (!sendAll(client_fd, buffer, count)) {
				throw std::runtime_error("overviewfile send failed");
			};
		}
	}
	logger.info("Server folder overview sent.");

	// 接收客户端发送的缺失文件数
	std::uint32_t fileCount;
	if (!recvAll(client_fd, (char *) &fileCount, sizeof(fileCount))) {
		throw std::runtime_error("fileCount receive failed");
	}

	// 接收客户端发送的文件
	logger.info("Waiting for files from client...");

	while (fileCount--) {

		std::uint64_t filesize;

		// 接收文件大小
		if (!recvAll(client_fd, (char *) (&filesize), sizeof(filesize))) {
			throw std::runtime_error("filesize receive failed");
		};
		logger.info("Received file size: " + std::to_string(filesize));

		// 接收文件相对路径大小
		std::uint32_t pathSize;
		if (!recvAll(client_fd, &pathSize, sizeof(pathSize))) {
			throw std::runtime_error("receive pathSize failed");
		}
		// 接收文件相对路径
		std::string relativePath(pathSize, '\0');
		if (!recvAll(client_fd, relativePath.data(), pathSize)) {
			throw std::runtime_error("receivec relativePath failed");
		}

		// 接收文件内容
		fs::path savepath = fs::path(folderpath) / fs::path(relativePath);
		logger.info("Saving file to: " + savepath.string());
		fs::create_directories(savepath.parent_path());
		std::ofstream file(savepath, std::ios::binary | std::ios::trunc);
		if (!file) {
			throw std::runtime_error("file open failed");
		}

		char buffer[256];
		std::uint64_t remain = filesize;
		while (remain > 0) {
			int min =
					static_cast<int>(std::min<std::uint64_t>(remain, sizeof(buffer)));
			if (!recvAll(client_fd, buffer, min)) {
				throw std::runtime_error("file receive error");
			};
			file.write(buffer, min);
			if (!file) {
				throw std::runtime_error("file write failed");
			}
			remain -= min;
		}

		logger.info("Expected bytes to write: " + std::to_string(filesize) + " B");
		logger.info("Bytes written: " + std::to_string(filesize - remain) + " B");
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
