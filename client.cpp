

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

bool recvAll(SOCKET fd, char *data, int len)
{
	int received = 0;
	while (received < len) {
		int result = recv(fd, data + received, len - received, 0);
		if (result <= 0) {
			return false;
		}
		received += result;
	}
	return true;
}
int main()
{
	// 启动程序 初始化Winsock
	Logger logger;
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

	// 提示输入客户端同步目录
	std::string folderPath;
	logger.info("Enter the folder path to synchronize on the client:");
	std::getline(std::cin, folderPath);

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
	std::uint32_t overviewSize;
	std::uint32_t overview;

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
	// 7. 已经接收了服务端传来的概览文件
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
	};
	std::vector<file> filelost{

	};
	logger.info("Starting file synchronization...");

	for (const auto &entry : fs::directory_iterator(folderPath)) {
		std::string currentFile = entry.path().filename().string();

		if (serverFiles.find(currentFile) == serverFiles.end()) {
      filelost.push_back({
        entry.path(),
        entry.file_size(),
        currentFile
      });
      fileCount++;
		}
		
	}
	// 发送缺失文件数给server
	if (!sendAll(client_fd, &fileCount, sizeof(fileCount))) {
		throw std::runtime_error("fileCount send failed");
	}
  int count = 0;
	for (const auto &entry : filelost) {
    if(count >= fileCount) break;
		logger.info("Sending file: " + filelost[count].name);
		// 创建buffer缓冲区
		char buffer[BUF_SIZE] = {0};
		// 取得当前要传输的文件信息
		auto filePath = filelost[count].path;
		auto filesize = (uint64_t) fs::file_size(filePath);
		std::uint32_t filenamelength = (std::uint32_t) (filelost[count].name.size());
		logger.info("File: " + std::string(filelost[count].name));
		logger.info("File size: " + std::to_string(filesize) + " B");

		// 发送文件名长度
		logger.info("Sending filename length...");
		if (!sendAll(client_fd, &filenamelength, sizeof(filenamelength))) {
			throw std::runtime_error("filenamelength send failed");
		}
		// 发送文件大小 // filesize 得到的是文件大小
		// sizeof(filesize)表示这个文件大小数值 占用多少字节
		logger.info("Sending file size...");
		if (!sendAll(client_fd, &filesize, sizeof(filesize))) {
			throw std::runtime_error("filesize send failed");
		}
		// 发送文件名
		logger.info("Sending filename...");
		if (!sendAll(client_fd, filelost[count].name.data(), (int) (filelost[count].name.size()))) {
			throw std::runtime_error("filename send failed");
		}
		// 发送文件内容
		logger.info("Sending file content...");
		std::ifstream file(filePath, std::ios::binary);
		if (!file) {
			throw std::runtime_error("file open failed");
		}
		while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
			std::streamsize count = file.gcount();
			if (count > 0) {
				if (!sendAll(client_fd, buffer, count)) {
					throw std::runtime_error("file send failed");
				};
			}
		}
		file.close();
		logger.info("File sent: " + std::to_string(filesize) + " B");
    count++;
	}
  logger.info("File synchronization completed.");
shutdown(client_fd, SD_BOTH);
closesocket(client_fd);
WSACleanup();
logger.info("Connection closed.");
return 0;
}

