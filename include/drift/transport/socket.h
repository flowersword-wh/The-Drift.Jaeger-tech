#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <string>

namespace drift {

bool sendAll(
    SOCKET socket,
    const void* data,
    int length
);

bool receiveAll(
    SOCKET socket,
    void* data,
    int length
);

void check_socket_result(
    int result,
    const std::string& operation
);

void Sender(
    SOCKET socket,
    const void* data,
    int length,
    const std::string& error_message
);

void Receiver(
    SOCKET socket,
    void* data,
    int length,
    const std::string& error_message
);

}