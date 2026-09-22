#include <atomic>
#include <filesystem>
#include <map>
#define WIN32_LEAN_AND_MEAN

#include "include/logger.h"
#include "include/fileoverview.h"
#include <cstdint>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "include/filehash.h"

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

	// 区分项是文件还是目录
	enum class EntryType : std::uint8_t {
		File = 1,
		Directory = 2,
	};

	// 服务端发来的信息的存储容器
	struct EntryData {
		EntryType entryType;
		std::uint64_t fileSize = 0;
		fs::path relativePath;
		Sha256 hash{};
	};
	std::map<std::string, EntryData> serverEntries;
	// 存储对比本地后缺失文件信息的清单
	struct PendingEntry {
		EntryType entryType;
		uint64_t size;
		fs::path absolute_path;
		fs::path relative_path;
	};

	std::vector<PendingEntry> LostEntries;
	// 接收服务端发来的 entry 数量
	std::uint32_t serverEntryCount = 0;
	if (!recvAll(client_fd, &serverEntryCount, sizeof(serverEntryCount))) {
		throw std::runtime_error("receive serverEntryCount failed");
	}

	for (int i = 0; i < serverEntryCount; i++) {
		EntryData data{};
		std::uint32_t pathSize = 0;
		// 接收相对路径大小
		if (!recvAll(client_fd, &pathSize, sizeof(pathSize))) {
			throw std::runtime_error("receive filepath size failed");
		}
		// 接收相对路径
		std::string relativePath(pathSize, '\0');

		if (!recvAll(client_fd, relativePath.data(), (int) (pathSize))) {
			throw std::runtime_error("receive filepath failed");
		}
		// 接收文件类型值
		std::uint8_t typeValue;
		if (!recvAll(client_fd, &typeValue, sizeof(typeValue))) {
			throw std::runtime_error("receive file typevalue failed");
		}
		EntryType entryType;
		if (typeValue == (std::uint8_t) (EntryType::File)) {
			entryType = EntryType::File;
		} else if (typeValue == (std::uint8_t) (EntryType::Directory)) {
			entryType = EntryType::Directory;
		} else {
			throw std::runtime_error("invalid entry type");
		}

		data.entryType = entryType;
		data.relativePath = fs::path(relativePath);

		if (entryType == EntryType::File) {
			// 如果是文件 接收文件大小和文件哈希值
			if (!recvAll(client_fd, &data.fileSize, sizeof(data.fileSize))) {
				throw std::runtime_error("receive filesize failed");
			}
			// 接收哈希值
			Sha256 serverHash{};
			if (!recvAll(client_fd, serverHash.data(),
									 static_cast<int>(serverHash.size()))) {
				throw std::runtime_error("receive file hash failed");
			}
			data.hash = serverHash;
		}
		serverEntries.emplace(data.relativePath.string(), data);
	}
	// 遍历查找 缺失就标记 记录缺失数
	logger.info("Starting file synchronization...");
	for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
		// 初始化本地文件信息清单 pending项
		PendingEntry pending{};
		pending.absolute_path = entry.path();
		pending.relative_path = entry.path().lexically_relative(folderPath);

		if (entry.is_directory()) {
			pending.entryType = EntryType::Directory;
		} else if (entry.is_regular_file()) {
			pending.size = entry.file_size();
			pending.entryType = EntryType::File;
		} else {
			continue;
		}

		// 对比服务端信息清单
		std::string path = pending.relative_path.generic_string();
		auto serverone = serverEntries.find(path);
		// 找不到 加入
		if (serverone == serverEntries.end()) {
			LostEntries.push_back(pending);
			continue;
		}
		// 路径相同 类型不同 加入
		if (serverone->second.entryType != pending.entryType) {
			LostEntries.push_back(pending);
			continue;
		}
		// 如果是文件 对比文件大小和哈希值
		// 如果是目录 路径和类型相同就可过
		if (serverone->second.entryType == EntryType::File) {
			// 文件大小不同 一定不同 加入
			if (serverone->second.fileSize != pending.size) {
				LostEntries.push_back(pending);
				continue;
			}
			// 哈希值不同 一定不同 加入
			Sha256 clientHash{};
			if (!calculate_hash(entry.path(), clientHash)) {
				throw std::runtime_error("calculate clientHash failed");
				;
			}
			if (serverone->second.hash != clientHash) {
				LostEntries.push_back(pending);
				continue;
			}
		}
	}

	// 发送缺失文件数给server
	std::uint32_t entryCount = (std::uint32_t) LostEntries.size();
	if (!sendAll(client_fd, &entryCount, sizeof(entryCount))) {
		throw std::runtime_error("send entryCount failed");
	}
	for (const auto &entry : LostEntries) {
		std::uint8_t typeValue = (std::uint8_t) entry.entryType;
		std::string relativePath = entry.relative_path.generic_string();
		std::uint32_t pathSize = (std::uint32_t) relativePath.size();

		// 发送文件类型值
		if (!sendAll(client_fd, &typeValue, sizeof(typeValue))) {
			throw std::runtime_error("send typeValue failed");
		}
		// 发送相对路径大小
		if (!sendAll(client_fd, &pathSize, sizeof(pathSize))) {
			throw std::runtime_error("send pathSize failed");
		}
		// 发送路径
		if (!sendAll(client_fd, relativePath.data(), pathSize)) {
			throw std::runtime_error("send relativePath failed");
		}
		// 如果是空目录 跳过发送大小
		if (entry.entryType == EntryType::Directory) {
			continue;
		}
		// 发送文件大小
		if (!sendAll(client_fd, &entry.size, sizeof(entry.size))) {
			throw std::runtime_error("send filesize failed");
		}
		// 发送文件
		std::ifstream file(entry.absolute_path, std::ios::binary);
		if (!file) {
			throw std::runtime_error("open file failed");
		}
		char buffer[64 * 1024];
		while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
			auto byteRead = file.gcount();
			if (byteRead > 0) {
				if (!sendAll(client_fd, &buffer, byteRead)) {
					throw std::runtime_error("send file content failed");
				}
			}
		}
	}
	logger.info("File synchronization completed.");
	shutdown(client_fd, SD_BOTH);
	closesocket(client_fd);
	WSACleanup();
	logger.info("Connection closed.");
	return 0;
}
