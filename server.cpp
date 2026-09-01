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
bool sendAll(SOCKET fd, const void *data, int len) {
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
	Logger logger;
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

  // 6. 接受客户端连接 (阻塞在这里)
  int len = sizeof(sockaddr_in_t);
  SOCKET client_fd = accept(server_fd, (sockaddr *)&sockaddr_in_t, &len);
  if (client_fd == INVALID_SOCKET) {
    throw std::runtime_error("accept failed: ");
  }
  logger.info("Connection established.");
  // 7. 建立连接后,发送要同步的文件夹
  std::string path;
  std::cout << "请输入要获取的文件夹路径：" << std::endl;
  std::getline(std::cin, path);
  getFileOverview(path); // 得到文件夹内容

  std::ifstream fileview("fileoverview.txt", std::ios::out);
  if (!fileview) {
    throw std::runtime_error("file open failed");
  }
  std::string line;
  while (std::getline(fileview, line)) {
    // 将line 发送给客户端
    send(client_fd, line.data(),(int)(line.size()), 0);
  }
  std::uint32_t filenamelength;
  std::uint64_t filesize;

  // 接收文件名长度
  if (!recvAll(client_fd, reinterpret_cast<char *>(&filenamelength),
               sizeof(filenamelength))) {
    throw std::runtime_error("filenamelength receive failed");
  };
  // 检查文件名长度是否合规
  if (filenamelength == 0 || filenamelength > 260) {
    throw std::runtime_error("invalid filename length");
  }
  logger.info("Received filename length: " + std::to_string(filenamelength));
  // 接收文件大小
  if (!recvAll(client_fd, reinterpret_cast<char *>(&filesize),
               sizeof(filesize))) {
    throw std::runtime_error("filesize receive failed");
  };
  logger.info("Received file size: " + std::to_string(filesize));
  // 接收文件名
  // 创建一个长度为 filenamelength 的字符串，并用 '\0' 填充
  // 先分配出足够的空间，让 recv() 把文件名写进去
  std::string filename(filenamelength, '\0');

  if (!recvAll(client_fd, filename.data(), static_cast<int>(filenamelength))) {
    throw std::runtime_error("filename receive failed");
  };
  logger.info("Received filename: " + filename);
  // 接收文件内容
  std::fstream file(filename, std::ios::binary);
  if (!file) {
    throw std::runtime_error("file open failed");
  }

  char buffer[256];
  std::uint64_t remain = filesize;
  while (remain > 0) {
    int min = static_cast<int>(std::min<std::uint64_t>(remain, sizeof(buffer)));
    if (!recvAll(client_fd, buffer, min)) {
      throw std::runtime_error("file receive error");
    };
    file.write(buffer, min);
    remain -= min;
  }
  logger.info("Expected bytes to write: " + std::to_string(filesize) + " B");
  logger.info("Bytes written: " + std::to_string(filesize - remain) + " B");
  if (remain != 0) {
    logger.error("Failed to receive complete file");
    logger.error("Remaining bytes: " + std::to_string(remain) + " B");
  }

	shutdown(server_fd, SD_BOTH);
	closesocket(server_fd);
	shutdown(client_fd, SD_BOTH);
	closesocket(client_fd);
	WSACleanup();
	logger.info("Connection closed.");
	return 0;
}
