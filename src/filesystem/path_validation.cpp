#include <drift/filesystem/path_validation.h>
#include <stdexcept>

namespace drift::filesystem
{

void validate_sync_root(const fs::path &root)
{
	if (root.empty()) {
		throw std::invalid_argument("Sync root must not be empty");
	}

	std::error_code error;

	bool exists = fs::exists(root, error);
	// 首先判断检查过程是否出错
	if (error) {
		throw std::runtime_error("Failed to inspect sync root '" + root.string() +
														 "': " + error.message());
	}
	// 然后判断是否存在
	if (!exists) {
		throw std::runtime_error("Sync root does not exist: " + root.string());
	}

	bool is_directory = fs::is_directory(root, error);
	// 首先判断检查过程是否出错
	if (error) {
		throw std::runtime_error("Failed to inspect sync root '" + root.string() +
														 "': " + error.message());
	}
	// 然后判断是否是目录
	if (!is_directory) {
		throw std::runtime_error("Sync root is not a directory: " + root.string());
	}
}

fs::path resolve_target_path(const fs::path &root, const fs::path &relative)
{
	validate_sync_root(root);
	// 判断relative path是否是空路径
	if (relative.empty()) {
		throw std::invalid_argument("Relative path must not be empty");
	}

	// Windows 下类似 C:XXX/XX 的路径可能有 root_name
	// 但不一定被 is_absolute() 判定为绝对路径
	// 拒绝含有 root_name 的路径
	if (relative.is_absolute() || relative.has_root_name() ||
			relative.has_root_directory()) {
		throw std::invalid_argument("Target path must be relative: " +
																relative.string());
	}

	const fs::path normalized_relative = relative.lexically_normal();

	// std::filesystem::path 可以遍历路径中的每个部分
	// 检查并禁止包含 .. 的路径，防止逃出同步根目录
	for (const fs::path &component : normalized_relative) {
		if (component == "..") {
			throw std::invalid_argument("Path traversal is not allowed: " +
																	relative.string());
		}
	}

	std::error_code error;
	// 根目录绝对路径规范化
	const fs::path canonical_root = fs::weakly_canonical(root, error);

	if (error) {
		throw std::runtime_error("Failed to normalize sync root '" + root.string() +
														 "': " + error.message());
	}
	// 相对路径规范化
	const fs::path target =
			fs::weakly_canonical(canonical_root / normalized_relative, error);

	if (error) {
		throw std::runtime_error("Failed to resolve target path '" +
														 relative.string() + "': " + error.message());
	}
	//weakly_canonical 可解析目录链接 其真实目录可能逃出同步根目录
	// 确认 canonical_root 是 target 的路径前缀
	// 保证同步文件路径是再在同步根目录内
	auto root_part = canonical_root.begin();
	auto target_part = target.begin();

	while (root_part != canonical_root.end() && target_part != target.end() &&
				 *root_part == *target_part) {
		root_part++;
		target_part++;
	}
	
	if (root_part != canonical_root.end()) {
		throw std::invalid_argument("Target path escapes sync root: " +
																relative.string());
	}

	//返回拼接结果
	return (root / normalized_relative).lexically_normal();
}

} // namespace drift::filesystem