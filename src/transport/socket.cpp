#include <drift/transport/socket.h>

#include <stdexcept>
#include <string>

namespace drift
{

bool sendAll(SOCKET socket, const void *data, int length)
{
	int sent = 0;

	const auto *bytes = static_cast<const char *>(data);

	while (sent < length) {
		const int result = send(socket, bytes + sent, length - sent, 0);

		if (result <= 0) {
			return false;
		}

		sent += result;
	}

	return true;
}

bool receiveAll(SOCKET socket, void *data, int length)
{
	int received = 0;

	auto *bytes = static_cast<char *>(data);

	while (received < length) {
		const int result = recv(socket, bytes + received, length - received, 0);

		if (result <= 0) {
			return false;
		}

		received += result;
	}

	return true;
}

void check_socket_result(int result, const std::string &operation)
{
	if (result == SOCKET_ERROR) {
		const int error = WSAGetLastError();

		throw std::runtime_error(operation +
														 " failed, WSA error: " + std::to_string(error));
	}
}

void Sender(SOCKET socket, const void *data, int length,
						const std::string &error_message)
{
	if (!sendAll(socket, data, length)) {
		throw std::runtime_error(error_message);
	}
}

void Receiver(SOCKET socket, void *data, int length,
							const std::string &error_message)
{
	if (!receiveAll(socket, data, length)) {
		throw std::runtime_error(error_message);
	}
}

} // namespace drift