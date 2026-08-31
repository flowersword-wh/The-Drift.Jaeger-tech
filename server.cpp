#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <sys/stat.h>

#define BUF_SIZE 256
#pragma comment(lib, "ws2_32.lib")

int judge(int result, const std::string& message) {
  if (result == SOCKET_ERROR) {
    throw std::runtime_error(message + " failed");
  }
  return 0;
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
  SetConsoleOutputCP(CP_UTF8);
  WSADATA wsaData;

  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

  if (result != 0) {
    throw std::runtime_error("WSAStartup failed");
  }
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
  sockaddr_in sockaddr_in_t {};
  sockaddr_in_t.sin_family = AF_INET;
  sockaddr_in_t.sin_port = htons(8080);
  sockaddr_in_t.sin_addr.s_addr = inet_addr("10.22.55.186");

  // 4. 绑定端口
  judge(bind(server_fd, (sockaddr *)&sockaddr_in_t, sizeof(sockaddr_in_t)), "bind");

  // 5. 开始监听 (第二个参数是未完成连接队列的大小，通常设为 SOMAXCONN,表示让系统使用一个合理的最大等待队列长度)
  judge(listen(server_fd, SOMAXCONN), "listen");
  std::cout << "监听中..." << std::endl;

  // 6. 接受客户端连接 (阻塞在这里)
  int len = sizeof(sockaddr_in_t);
  SOCKET client_fd = accept(server_fd, (sockaddr *)&sockaddr_in_t, &len);
  if (client_fd == INVALID_SOCKET) {
    throw std::runtime_error("accept failed: ");
  }
  std::cout << "连接已建立..." << std::endl;
  // 7. 建立连接后，接收客户端发送的消息
  std::uint32_t filenamelength;
  std::uint64_t filesize;

  // 接收文件名长度
  if (!recvAll(client_fd, reinterpret_cast<char *>(&filenamelength),
               sizeof(filenamelength))) {
    throw std::runtime_error("filenamelength receive failed");
  };
  //检查文件名长度是否合规
  if (filenamelength == 0 || filenamelength > 260) {
    throw std::runtime_error("invalid filename length");
  }
  // 接收文件大小
  if (!recvAll(client_fd, reinterpret_cast<char *>(&filesize),
               sizeof(filesize))) {
    throw std::runtime_error("filesize receive failed");
  };
  // 接收文件名
  // 创建一个长度为 filenamelength 的字符串，并用 '\0' 填充
  // 先分配出足够的空间，让 recv() 把文件名写进去
  std::string filename(filenamelength, '\0');

  if (!recvAll(client_fd, filename.data(), static_cast<int>(filenamelength))) {
    throw std::runtime_error("filename receive failed");
  };
  // 接收文件内容
  std::ofstream file(filename, std::ios::binary);
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



  shutdown(server_fd, SD_BOTH);
  closesocket(server_fd);
  shutdown(client_fd, SD_BOTH);
  closesocket(client_fd);
  WSACleanup();
  std::cout << "连接已释放..." << std::endl;
  return 0;
}
