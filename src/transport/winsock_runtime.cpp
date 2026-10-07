#include <drift/transport/Winsock_runtime.h>
#include <../include/logger.h>

drift::WinsockRuntime::WinsockRuntime(){
    Logger logger;
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
}
drift::WinsockRuntime::~WinsockRuntime(){
    WSACleanup();
}