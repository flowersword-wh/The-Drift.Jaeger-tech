#include <drift/sync/server_sync.h>

#include <drift/filesystem/directory_scan.h>
#include <drift/filesystem/path_validation.h>
#include <drift/protocol/overview.h>
#include <drift/protocol/transfer.h>

namespace drift::sync
{

void run_server_sync(SOCKET client_socket,
										 const std::filesystem::path &sync_root)
{
	// 1. 确认服务端同步根目录存在且是目录
	drift::filesystem::validate_sync_root(sync_root);

	// 2.扫描服务端现有的文件和目录
	const auto server_entries = drift::filesystem::scan_directory(sync_root);

	// 3.将服务端目录概览发送给客户端
	drift::protocol::send_overview(client_socket, server_entries);

	// 4.接收客户端计算出的待同步条目并保存
	drift::protocol::receive_transfer(client_socket, sync_root);
}

} // namespace drift::sync