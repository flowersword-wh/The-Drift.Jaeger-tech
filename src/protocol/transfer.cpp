#include <drift/protocol/transfer.h>

#include <drift/filesystem/path_validation.h>
#include <drift/transport/socket.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
namespace drift::protocol
{
using drift::Receiver;
using drift::Sender;
using drift::filesystem::EntryType;

void send_transfer(SOCKET socket, const PendingEntries &entries)
{
	const auto entry_count = static_cast<std::uint32_t>(entries.size());

	// 先发送待传输条目总数。
	drift::Sender(socket, &entry_count, sizeof(entry_count),
								"send transfer entry count failed");

	for (const PendingEntry &entry : entries) {
		const auto type_value = (std::uint8_t)(entry.type);

		const std::string relative_path = entry.relative_path.generic_string();

		const auto path_size = (std::uint32_t)(relative_path.size());

		Sender(socket, &type_value, sizeof(type_value),
					 "send transfer entry type failed");

		Sender(socket, &path_size, sizeof(path_size),
					 "send transfer path size failed");

		Sender(socket, relative_path.data(), static_cast<int>(path_size),
					 "send transfer relative path failed");

		if (entry.type == EntryType::Directory) {
			continue;
		}

		Sender(socket, &entry.file_size, sizeof(entry.file_size),
					 "send transfer file size failed");

		std::ifstream input(entry.absolute_path, std::ios::binary);

		if (!input) {
			throw std::runtime_error("Failed to open file: " +
															 entry.absolute_path.string());
		}

		std::array<char, 64 * 1024> buffer{};

		while (input) {
			input.read(buffer.data(), buffer.size());
			//stramsize 用于表示I/O传输中的字符串数或缓冲区的容量
			const std::streamsize bytes_read = input.gcount();

			if (bytes_read > 0) {
				Sender(socket, buffer.data(), static_cast<int>(bytes_read),
							 "send transfer file content failed");
			}
		}
		// 检查文件是否读取完整
		if (!input.eof()) {
			throw std::runtime_error("Failed to read file: " +
															 entry.absolute_path.string());
		}
	}
}

void receive_transfer(SOCKET socket, const std::filesystem::path &sync_root)
{
	std::uint32_t pending_count = 0;

	Receiver(socket, &pending_count, sizeof(pending_count),
					 "receive pending count failed");

	for (std::uint32_t i = 0; i < pending_count; i++) {
		std::uint8_t type_value = 0;

		Receiver(socket, &type_value, sizeof(type_value),
						 "receive entry type failed");

		EntryType entry_type{};

		if (type_value == (std::uint8_t)(EntryType::File)) {
			entry_type = EntryType::File;
		} else if (type_value == (std::uint8_t)(EntryType::Directory)) {
			entry_type = EntryType::Directory;
		} else {
			throw std::runtime_error("Invalid transfer entry type");
		}

		std::uint32_t path_size = 0;

		Receiver(socket, &path_size, sizeof(path_size), "receive path size failed");

		if (path_size == 0 || path_size > 4096) {
			throw std::runtime_error("Invalid transfer path size");
		}

		std::string relative_path(path_size, '\0');

		Receiver(socket, relative_path.data(), static_cast<int>(path_size),
						 "receive relative path failed");

		const std::filesystem::path save_path =
				drift::filesystem::resolve_target_path(
						sync_root, std::filesystem::path(relative_path));

		if (entry_type == EntryType::Directory) {
			std::filesystem::create_directories(save_path);

			continue;
		}

		std::uint64_t file_size = 0;

		Receiver(socket, &file_size, sizeof(file_size), "receive file size failed");

		std::filesystem::create_directories(save_path.parent_path());

		std::ofstream file(save_path, std::ios::binary | std::ios::trunc);

		if (!file) {
			throw std::runtime_error("Failed to open target file: " +
															 save_path.string());
		}

		std::array<char, 64 * 1024> buffer{};
		std::uint64_t remaining = file_size;

		while (remaining > 0) {
			const int already_read =
					(int) (std::min<std::uint64_t>(remaining, buffer.size()));

			Receiver(socket, buffer.data(), already_read,
							 "receive file content failed");

			file.write(buffer.data(), already_read);

			if (!file) {
				throw std::runtime_error("Failed to write target file: " +
																 save_path.string());
			}

			remaining -= (std::uint64_t)(already_read);
		}
	}
}
} // namespace drift::protocol