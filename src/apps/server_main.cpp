#include <drift/sync/server_sync.h>
#include <drift/transport/socket.h>
#include <drift/transport/Winsock_runtime.h>

#include <logger.h>

#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char *argv[])
{
	Logger logger;

	try {
		if (argc != 2) {
			logger.error("Usage: " + std::string(argv[0]) + " <sync-folder>");

			return 1;
		}

		const std::filesystem::path folderpath = argv[1];

		drift::WinsockRuntime winsock;
		// 1. 创建监听套接字 socket
		SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if (server_fd == INVALID_SOCKET) {
			throw std::runtime_error("socket failed, WSA error: " +
															 std::to_string(WSAGetLastError()));
		}

		int reuse_address = 1;
		// 2. 设置端口复用 在bind()前
		drift::check_socket_result(
				setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
									 reinterpret_cast<const char *>(&reuse_address),
									 sizeof(reuse_address)),
				"setsockopt");

		// 3. 准备地址结构体 绑定端口 8080
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(8080);
		address.sin_addr.s_addr = htonl(INADDR_ANY);
		// 4. 绑定端口
		drift::check_socket_result(bind(server_fd,
																		reinterpret_cast<sockaddr *>(&address),
																		sizeof(address)),
															 "bind");
		// 5， 开始监听
		drift::check_socket_result(listen(server_fd, SOMAXCONN), "listen");

		logger.info("Listening...");

		int address_length = sizeof(address);
		// 6. 接收客户端连接
		SOCKET client_fd = accept(server_fd, reinterpret_cast<sockaddr *>(&address),
															&address_length);

		if (client_fd == INVALID_SOCKET) {
			throw std::runtime_error("accept failed, WSA error: " +
															 std::to_string(WSAGetLastError()));
		}

		logger.info("Connection established.");

		drift::sync::run_server_sync(client_fd, folderpath);

		closesocket(client_fd);
		closesocket(server_fd);

		return 0;
	} catch (const std::exception &error) {
		logger.error(error.what());
		return 1;
	}
}