#pragma once

#include <filesystem>
#include <WinSock2.h>

namespace drift::sync
{
void run_client_sync(SOCKET socket, const std::filesystem::path &root);
}
