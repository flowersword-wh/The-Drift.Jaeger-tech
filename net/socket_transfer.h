#define WIN32_LEAN_AND_MEAN
#pragma once
#include <sys/stat.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

using namespace std;

bool sendAll(SOCKET fd, const void *data, int len);

bool recvAll(SOCKET fd, void *data, int len);

int judge(int result, const std::string &message);

void Sender(
    SOCKET fd,
    const void *data,
    int len,
    const std::string &message
);

void Receiver(
    SOCKET fd,
    void* data,
    int len,
    const std::string& message
);
