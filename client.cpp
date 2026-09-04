

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
	std::string folderPath = argv[1];
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

	// 5. 请求要同步的文件夹内容
	// std::string ask_msg = "请发送要同步的文件夹现有内容";
	// sendAll(client_fd, ask_msg.data() ,(int)(ask_msg.size()));

	// 6. 读取服务端发来的文件夹内容
	std::uint64_t overviewSize;
	std::uint64_t overview;

	// 接收概览文件大小
	logger.info("Receiving server folder overview...");

	if (!recvAll(client_fd, (char *) &overviewSize, sizeof(overviewSize))) {
		throw std::runtime_error("OverviewFile size receive failed");
	}
	// 接收概览文件
	std::ofstream overviewfile("fileoverview.txt",
														 std::ios::binary | std::ios::trunc);
	if (!overviewfile) {
		throw std::runtime_error("file open failed");
	}

	char overviewBuffer[256];
	std::uint64_t remain = overviewSize;
	while (remain > 0) {
		int min = (int) (std::min<std::uint64_t>(remain, sizeof(overviewBuffer)));
		if (!recvAll(client_fd, overviewBuffer, min)) {
			throw std::runtime_error("overviewfile receive failed");
		}
		overviewfile.write(overviewBuffer, min);
		remain -= min;
	}
	overviewfile.close();
	logger.info("Expected bytes to write: " + std::to_string(overviewSize) +
							" B");
	logger.info("Bytes written: " + std::to_string(overviewSize - remain) + " B");
	if (remain != 0) {
		logger.error("Failed to receive complete file");
		logger.error("Remaining bytes: " + std::to_string(remain) + " B");
	}
	logger.info("Server folder overview received.");
	// 7. 已经接收了服务端传来的概览文件路径
	//    此时读取要传输的文件夹 比对服务端缺失的文件 缺失就发送
	// 把fileoverview放到set里
	std::set<std::string> serverFiles;
	std::string filename;
	std::ifstream readstream("fileoverview.txt");
	if (!readstream) {
		throw std::runtime_error("file open failed");
	}
	while (std::getline(readstream, filename)) {
		if (!filename.empty()) {
			serverFiles.insert(filename);
		}
	}
	// 遍历查找 缺失就标记 记录缺失数
	int fileCount = 0;
	struct file {
		fs::path path;
		size_t size;
		std::string name;
		bool is_folder;
	};
	std::vector<file> filelost{};
	logger.info("Starting file synchronization...");

	for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
		std::string currentFile = entry.path().filename().string();
		if (currentFile == "client.exe") {
			continue;
		}
		logger.info("Client file: [" + currentFile + "]");

		if (serverFiles.find(currentFile) == serverFiles.end()) {
			// 判断指向对象是否是目录（文件夹），如果是,则创建同名文件夹
			if (entry.is_directory()) {
				filelost.push_back({entry.path(), 0, currentFile, true});
			} else {
				filelost.push_back(
						{entry.path(), entry.file_size(), currentFile, false});
				fileCount++;
			}
		}
	}
	///

	logger.info("Server file count: " +
            std::to_string(serverFiles.size()));

for (const auto &name : serverFiles) {
    logger.info("Server file: [" + name + "]");
}
////
	// 发送缺失文件数给server
	if (!sendAll(client_fd, &fileCount, sizeof(fileCount))) {
		throw std::runtime_error("fileCount send failed");
	}

	for (const auto &entry : filelost) {
		if (entry.is_folder) {
			continue;
		}
		
		logger.info("Sending file: " + entry.name);

		char buffer[BUF_SIZE];

		auto filePath = entry.path;
		auto filesize = static_cast<std::uint64_t>(fs::file_size(filePath));

		std::uint32_t filenamelength = (std::uint32_t) (entry.name.size());

		// 发送文件名长度
		if (!sendAll(client_fd, &filenamelength, sizeof(filenamelength))) {
			throw std::runtime_error("filenamelength send failed");
		}

		// 发送文件大小
		if (!sendAll(client_fd, &filesize, sizeof(filesize))) {
			throw std::runtime_error("filesize send failed");
		}

		// 发送文件名
		if (!sendAll(client_fd, entry.name.data(), (int) (entry.name.size()))) {
			throw std::runtime_error("filename send failed");
		}

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
