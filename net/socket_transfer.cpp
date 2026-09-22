#include "socket_transfer.h"
#include <stdexcept>

bool sendAll(SOCKET fd, const void *data, int len)
{
	int sent = 0;
	const char *bytes = (char *) (data);
	while (sent < len) {
		int result = send(fd, bytes + sent, len - sent, 0);
		if (result <= 0) {
			return false;
		}
		sent += result;
	}
	return true;
}

//recvAll 要把收到的数据写进 data 指向的内存，所以参数应是 void*，不能是 const void*
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

int judge(int result, const std::string &message)
{
	if (result == SOCKET_ERROR) {
		int error = WSAGetLastError();

		throw std::runtime_error(message + " failed, WSA error: " + std::to_string(error));
	}

	return 0;
}

void Sender(
    SOCKET fd,
    const void* data,
    int len,
    const std::string& message
) {
    if (!sendAll(fd, data, len)) {
        throw std::runtime_error(message);
    }
}
void Receiver(
    SOCKET fd,
    void* data,
    int len,
    const std::string& message
){
    if(!recvAll(fd, data, len)){
        throw std::runtime_error(message);
    }
}