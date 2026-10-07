#include <drift/transport/Winsock_runtime.h>
#include <drift/transport/socket.h>
#include <drift/sync/client_sync.h>
#include <logger.h>

int main(int argc, char *argv[])
{

	Logger logger;
	// 命令行输入要同步的目录
	if (argc != 2) {
		logger.error("Missing synchronization folder path.");
		logger.error("Usage: " + std::string(argv[0]) + " <sync-folder>");
		return 1;
	}
	std::filesystem::path folderPath = argv[1];

	drift::WinsockRuntime winsock;

	//  1. 创建监听套接字 (AF_INET=IPv4, SOCK_STREAM=TCP)
	SOCKET client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd == INVALID_SOCKET) {
		throw std::runtime_error("socket failed: " +
														 std::to_string(WSAGetLastError()));
	}
	logger.info("Client socket created.");
	// 2. 准备地址结构体，绑定端口 8080]
	sockaddr_in sockaddr_in_t{};
	sockaddr_in_t.sin_family = AF_INET;
	sockaddr_in_t.sin_port = htons(8080);
	sockaddr_in_t.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	// 3. 请求连接
	int len = sizeof(sockaddr_in_t);
	logger.info("Connecting to server...");
	drift::check_socket_result(
			connect(client_fd, (sockaddr *) &sockaddr_in_t, len), "connect");
	logger.info("Connection established.");

	drift::sync::run_client_sync(client_fd, folderPath);

	// 4. 关闭
	drift::check_socket_result(shutdown(client_fd, SD_SEND),
														 "shutdown client socket");

	closesocket(client_fd);

	return 0;
}