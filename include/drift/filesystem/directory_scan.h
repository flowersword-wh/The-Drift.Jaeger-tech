#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <drift/filesystem/file_hash.h>

namespace drift::filesystem
{

enum class EntryType : std::uint8_t { File = 1, Directory = 2 };

struct EntryData {
	EntryType type;
	std::filesystem::path relative_path;
	std::uint64_t file_size = 0;
	Sha256 hash{};
};

using DirectoryEntries = std::map<std::string, EntryData>;

DirectoryEntries scan_directory(const std::filesystem::path &root);

} // namespace drift::filesystem