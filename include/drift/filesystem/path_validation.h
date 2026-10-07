#pragma once

#include <filesystem>

namespace drift::filesystem
{

namespace fs = std::filesystem;

void validate_sync_root(const fs::path &root);

fs::path resolve_target_path(const fs::path &root, const fs::path &relative);

} // namespace drift::filesystem