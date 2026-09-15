#include <array>
#include <cstdint>
#include <filesystem>

//sha256 输出固定是 256bit = 32B 用单位大小固定的数组保存
using Sha256 = std::array<std::uint8_t, 32>;

// 形参 path 和 result 计算path指向文件的哈希值
bool calculate_hash(
    const std::filesystem::path& path,
    Sha256& result
);