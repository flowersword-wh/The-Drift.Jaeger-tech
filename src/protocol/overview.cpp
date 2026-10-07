#include <drift/protocol/overview.h>
#include <drift/transport/socket.h>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace drift::protocol
{

using drift::Receiver;
using drift::Sender;
using drift::filesystem::DirectoryEntries;
using drift::filesystem::EntryData;
using drift::filesystem::EntryType;

void send_overview(SOCKET socket, const DirectoryEntries &entries)
{
	const auto entry_count = (std::uint32_t) (entries.size());
	// 发送目录项数
	Sender(socket, &entry_count, sizeof(entry_count),
				 "send overview entry count failed");

	for (const auto &[path_key, entry] : entries) {
		const std::string relative_path = entry.relative_path.generic_string();

		const auto path_size = (std::uint32_t) (relative_path.size());

		const auto type_value = (std::uint8_t) (entry.type);

		Sender(socket, &path_size, sizeof(path_size),
					 "send overview path size failed");

		Sender(socket, relative_path.data(), static_cast<int>(path_size),
					 "send overview relative path failed");

		Sender(socket, &type_value, sizeof(type_value),
					 "send overview entry type failed");
		// 判断如果是文件 再发送文件大小和哈希值
		if (entry.type == EntryType::File) {
			Sender(socket, &entry.file_size, sizeof(entry.file_size),
						 "send overview file size failed");

			Sender(socket, entry.hash.data(), static_cast<int>(entry.hash.size()),
						 "send overview file hash failed");
		}
	}
}

DirectoryEntries receive_overview(SOCKET socket)
{
	DirectoryEntries entries;

	std::uint32_t entry_count = 0;
	// 先接收目录项数
	Receiver(socket, &entry_count, sizeof(entry_count),
					 "receive overview entry count failed");

	for (std::uint32_t i = 0; i < entry_count; i++) {
		std::uint32_t path_size = 0;

		Receiver(socket, &path_size, sizeof(path_size),
						 "receive overview path size failed");

		if (path_size == 0 || path_size > 4096) {
			throw std::runtime_error("Invalid overview path size");
		}

		std::string relative_path(path_size, '\0');

		Receiver(socket, relative_path.data(), static_cast<int>(path_size),
						 "receive overview relative path failed");

		std::uint8_t type_value = 0;

		Receiver(socket, &type_value, sizeof(type_value),
						 "receive overview entry type failed");

		EntryData entry{};
		entry.relative_path = relative_path;

		if (type_value == (std::uint8_t) (EntryType::File)) {
			entry.type = EntryType::File;
			// 判断类型是文件后再接收文件大小和哈希值
			Receiver(socket, &entry.file_size, sizeof(entry.file_size),
							 "receive overview file size failed");

			Receiver(socket, entry.hash.data(), static_cast<int>(entry.hash.size()),
							 "receive overview file hash failed");
		} else if (type_value == (std::uint8_t) (EntryType::Directory)) {
			entry.type = EntryType::Directory;
		} else {
			throw std::runtime_error("Invalid overview entry type");
		}

		entries.emplace(entry.relative_path.generic_string(), std::move(entry));
	}

	return entries;
}

} // namespace drift::protocol