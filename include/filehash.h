#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

using Sha256 = std::array<std::uint8_t, 32>;

bool calculate_hash(
    const std::filesystem::path& path,
    Sha256& result
);