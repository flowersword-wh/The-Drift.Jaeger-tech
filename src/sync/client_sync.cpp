#include <drift/sync/client_sync.h>
#include <drift/filesystem/directory_scan.h>
#include <drift/protocol/overview.h>
#include <drift/protocol/transfer.h>
#include <filesystem>

using drift::filesystem::DirectoryEntries;
using drift::filesystem::EntryData;
using drift::filesystem::EntryType;
using drift::protocol::PendingEntries;
using drift::protocol::PendingEntry;

bool need_transfer(EntryData local_entry, DirectoryEntries serverEntries)
{

	std::string path = local_entry.relative_path.generic_string();
	const auto serverone = serverEntries.find(path);

	// 找不到 加入
	if (serverone == serverEntries.end()) {
		return true;
	}
	// 路径相同 类型不同 加入
	if (serverone->second.type != local_entry.type) {
		return true;
	}
	// 如果是文件 对比文件大小和哈希值
	// 如果是目录 路径和类型相同就可过
	if (local_entry.type == EntryType::Directory) {
		return false;
	}
	if (serverone->second.type == EntryType::File) {
		// 文件大小不同 一定不同 加入
		if (serverone->second.file_size != local_entry.file_size) {
			return true;
		}
		// 哈希值不同 一定不同 加入
		if (serverone->second.hash != local_entry.hash) {
			return true;
		}
	}
	return false;
}
PendingEntries build_Lost_entries(const std::filesystem::path &root,
								const DirectoryEntries local_entries,
								const DirectoryEntries server_entries)
{
	PendingEntries lost_entries{};

	for (const auto &item : local_entries) {
		EntryData local_entry = item.second;

		if (!need_transfer(local_entry, server_entries)) {
			continue;
		}

		PendingEntry pending{};

		pending.absolute_path = root / local_entry.relative_path;
		pending.relative_path = local_entry.relative_path;
		pending.file_size = local_entry.file_size;
		pending.type = local_entry.type;

		lost_entries.push_back(std::move(pending));
	}
	return lost_entries;
}

void drift::sync::run_client_sync(SOCKET socket, const std::filesystem::path &root)
{
	// 1. 接收服务端目录概览
	const auto server_entries = drift::protocol::receive_overview(socket);

	// 2. 扫描客户端目录
	const auto local_entries = drift::filesystem::scan_directory(root);

	// 3. 比较两边目录，生成待发送列表
	const auto pending_entries =
			build_Lost_entries(root, local_entries, server_entries);

	// 4. 发送服务端缺少或不同的条目
	drift::protocol::send_transfer(socket, pending_entries);
	
}
