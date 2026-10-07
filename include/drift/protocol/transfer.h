#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <winsock2.h>

#include <drift/filesystem/directory_scan.h>

namespace drift::protocol
{

struct PendingEntry {
	drift::filesystem::EntryType type;
	std::filesystem::path absolute_path;
	std::filesystem::path relative_path;
	std::uint64_t file_size = 0;
};

using PendingEntries = std::vector<PendingEntry>;

void send_transfer(SOCKET socket, const PendingEntries &entries);

void receive_transfer(SOCKET socket, const std::filesystem::path &sync_root);

} // namespace drift::protocol