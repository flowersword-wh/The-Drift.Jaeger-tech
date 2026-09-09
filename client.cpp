

#include <cstddef>
#include <filesystem>
#define WIN32_LEAN_AND_MEAN

#include "include/logger.h"
#include "include/fileoverview.h"
#include <cstdint>
#include <fstream>
#include <set>
#include <vector>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define BUF_SIZE 256
#pragma comment(lib, "ws2_32.lib")

int judge(int result, const std::string &message)
{
	if (result == SOCKET_ERROR) {
		throw std::runtime_error(message + " failed");
	}
	return 0;
}

// sendAll(fd,发送的数据，发送数据大小)
bool sendAll(SOCKET fd, const void *data, int len)
{
	int sent = 0;
	const char *bytes = static_cast<const char *>(data);
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
	fs::path folderPath = argv[1];
	// 校验目录是否存在、是否为文件夹
	try {
		// 检查是否存在
		if (!fs::exists(folderPath)) {
			logger.error("路径不存在！");
			return -1;
		}
		if (!fs::is_directory(folderPath)) {
			logger.error("文件夹不存在！");
			return -1;
		}
	} catch (std::exception &e) {
		logger.error(e.what());
		return 1;
	}
	// 启动程序 初始化Winsock
	logger.info("Client starting...");
	SetConsoleOutputCP(CP_UTF8);
	logger.info("Console output code page set to UTF-8.");
	WSADATA wsaData;

	logger.info("Initializing Winsock...");
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (result != 0) {
		throw std::runtime_error("WSAStartup failed");
	}
	logger.info("Winsock initialized.");

	//  1. 创建监听套接字 (AF_INET=IPv4, SOCK_STREAM=TCP)
	SOCKET client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd == INVALID_SOCKET) {
		throw std::runtime_error("socket failed: " +
														 std::to_string(WSAGetLastError()));
	}
	logger.info("Client socket created.");
	// 3. 准备地址结构体，绑定端口 8080]
	sockaddr_in sockaddr_in_t{};
	sockaddr_in_t.sin_family = AF_INET;
	sockaddr_in_t.sin_port = htons(8080);
	sockaddr_in_t.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	// 4. 请求连接
	int len = sizeof(sockaddr_in_t);
	logger.info("Connecting to server...");
	judge(connect(client_fd, (sockaddr *) &sockaddr_in_t, len), "connect");
	logger.info("Connection established.");

	// 先创建用于存储服务端文件相对路径的存储逻辑
	std::set<std::string> serverFiles;
	struct file {
		fs::path absolute_path;
		fs::path relative_path;
		size_t size;
		std::string name;
	};

	std::vector<file> filelost{};

	// 接收服务端要同步文件夹里的文件数量
	int serverfileCount;
	if (!recvAll(client_fd, &serverfileCount, sizeof(serverfileCount))) {
		throw std::runtime_error("receive serverfile count failed");
	}

	// 循环接收文件相对路径大小 和 相对路径
	for (int i = 0; i < serverfileCount; ++i) {
		std::uint32_t pathSize = 0;

		if (!recvAll(client_fd, &pathSize, sizeof(pathSize))) {
			throw std::runtime_error("receive filepath size failed");
		}

		std::string relativePath(pathSize, '\0');

		if (!recvAll(client_fd, relativePath.data(), (int) (pathSize))) {
			throw std::runtime_error("receive filepath failed");
		}

		serverFiles.insert(relativePath);
	}

	// 遍历查找 缺失就标记 记录缺失数
	logger.info("Starting file synchronization...");
	std::uint32_t fileCount = 0;
	for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
		std::string currentPath =
				entry.path().lexically_relative(folderPath).string();
		// 如果客户端同步文件夹选择包含client.exe的文件夹，则不能发送client.exe
		if (entry.path().filename() == "client.exe")
			continue;

		if (serverFiles.find(currentPath) == serverFiles.end() &&
				entry.is_regular_file()) {
			filelost.push_back({entry.path(),
													entry.path().lexically_relative(folderPath),
													entry.file_size(), entry.path().filename().string()});
			fileCount++;
		}
	}

	// 发送缺失文件数给server
	if (!sendAll(client_fd, &fileCount, sizeof(fileCount))) {
		throw std::runtime_error("fileCount send failed");
	}

	for (const auto &entry : filelost) {

		logger.info("Sending file: " + entry.name);

		char buffer[BUF_SIZE];

		auto filePath = entry.absolute_path;
		auto filesize = entry.size;

		// 发送文件大小
		if (!sendAll(client_fd, &filesize, sizeof(filesize))) {
			throw std::runtime_error("filesize send failed");
		}

		// 发送文件相对路径大小

		std::string relativePath = entry.relative_path.string();
		std::uint32_t pathSize = (std::uint32_t) (relativePath.size());
		if (!sendAll(client_fd, &pathSize, sizeof(pathSize))) {
			throw std::runtime_error("pathSize send failed");
		}
		// 发送文件相对路径

		if (!sendAll(client_fd, relativePath.data(), pathSize)) {
			throw std::runtime_error("relativePath send failed");
		}

		// 发送文件内容
		std::ifstream file(filePath, std::ios::binary);
		if (!file) {
			throw std::runtime_error("file open failed");
		}

		while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
			std::streamsize count = file.gcount();
			if (count > 0) {
				if (!sendAll(client_fd, buffer, (int) (count))) {
					throw std::runtime_error("file send failed");
				}
			}
		}

		logger.info("File sent: " + std::to_string(filesize) + " B");
	}
	logger.info("File synchronization completed.");
	shutdown(client_fd, SD_BOTH);
	closesocket(client_fd);
	WSACleanup();
	logger.info("Connection closed.");
	return 0;
}
