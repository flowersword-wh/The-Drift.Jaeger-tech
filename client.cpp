#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <sys/stat.h>
#include "include/logger.h"
#include "fileoverview.h"


#define BUF_SIZE 256
#pragma comment(lib, "ws2_32.lib")

int judge(int result, const std::string& message) {
  if (result == SOCKET_ERROR) {
    throw std::runtime_error(message + " failed");
  }
  return 0;
}

// sendAll(fd,发送的数据，发送数据大小)
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

bool recvAll(SOCKET fd, char *data, int len) {
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
int main() {
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
  // 1. 创建监听套接字 (AF_INET=IPv4, SOCK_STREAM=TCP)
  SOCKET client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd == INVALID_SOCKET) {
    throw std::runtime_error("socket failed: " +
                             std::to_string(WSAGetLastError()));
  }
  logger.info("Client socket created.");
  // 3. 准备地址结构体，绑定端口 8080]
  sockaddr_in sockaddr_in_t {};
  sockaddr_in_t.sin_family = AF_INET;
  sockaddr_in_t.sin_port = htons(8080);
  sockaddr_in_t.sin_addr.s_addr = inet_addr("10.22.55.186");

  // 4. 请求连接
  int len = sizeof(sockaddr_in_t);
  logger.info("Connecting to server...");
  judge(connect(client_fd, (sockaddr *)&sockaddr_in_t, len), "connect");
  logger.info("Connection established.");

  // 5. 请求要同步的文件夹内容
  //std::string ask_msg = "请发送要同步的文件夹现有内容";
  //sendAll(client_fd, ask_msg.data() ,(int)(ask_msg.size()));

  // 6. 读取服务端发来的文件夹内容
  // 6. 读取要传输的文件
  const char *filename = "demo.txt";
  char buffer[BUF_SIZE] = {0};

  // 7. 文件名及文件大小 写入buffer
  // stat是一个存储文件信息的结构体 其中有文件大小和创建时间、访问时间、修改时间
  // stat(filename,&buf)
  struct stat statbuf;
  if (stat("demo.txt", &statbuf) != 0) {
    logger.error("Failed to get file.");
    return 1;
  }

  std::uint32_t filenamelength = static_cast<std::uint32_t>((strlen(filename)));
  std::uint64_t filesize = static_cast<std::uint64_t>(statbuf.st_size);
  logger.info("File: " + std::string(filename));
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
  if (!sendAll(client_fd, filename, filenamelength)) {
    throw std::runtime_error("filename send failed");
  }
  // 发送文件内容
  logger.info("Sending file content...");
  std::ifstream file("demo.txt", std::ios::binary);
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

  shutdown(client_fd, SD_BOTH);
  closesocket(client_fd);
  WSACleanup();
  logger.info("Connection closed.");
  return 0;
}
