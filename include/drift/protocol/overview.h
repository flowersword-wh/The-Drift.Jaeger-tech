#pragma once

#include <drift/filesystem/directory_scan.h>
#include <winsock2.h>

namespace drift::protocol
{
void send_overview(SOCKET socket, const drift::filesystem::DirectoryEntries &entries);

drift::filesystem::DirectoryEntries receive_overview(SOCKET socket);

} // namespace drift::protocol
