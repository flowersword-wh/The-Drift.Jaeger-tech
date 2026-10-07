#include <drift/filesystem/file_hash.h>

#include <array>
#include <fstream>
#include <stdexcept>

#include <openssl/evp.h>

Sha256 calculate_hash(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        throw std::runtime_error(
            "Failed to open file for hashing: " + path.string());
    }

    // 1. 创建 OpenSSL 摘要上下文
    EVP_MD_CTX* context = EVP_MD_CTX_new();

    if (context == nullptr) {
        throw std::runtime_error(
            "Failed to create SHA-256 context for: " + path.string());
    }

    try {
        // 2. 初始化为 SHA-256
        if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
            throw std::runtime_error(
                "Failed to initialize SHA-256 for: " + path.string());
        }

        std::array<char, 64 * 1024> buffer{};

        while (file) {
            file.read(buffer.data(), buffer.size());

            const std::streamsize count = file.gcount();

            // 3. 将本次读取的数据加入hash计算
            if (count > 0 &&
                EVP_DigestUpdate(
                    context,
                    buffer.data(),
                    static_cast<std::size_t>(count)) != 1) {
                throw std::runtime_error(
                    "Failed to update SHA-256 for: " + path.string());
            }
        }

        // 循环因为正常到达 EOF 而结束
        if (!file.eof()) {
            throw std::runtime_error(
                "Failed to read file for hashing: " + path.string());
        }

        Sha256 result{};
        unsigned int digest_length = 0;

        // 4. 生成最终 SHA-256
        if (EVP_DigestFinal_ex(
                context,
                result.data(),
                &digest_length) != 1) {
            throw std::runtime_error(
                "Failed to finalize SHA-256 for: " + path.string());
        }

        if (digest_length != result.size()) {
            throw std::runtime_error(
                "Unexpected SHA-256 length for: " + path.string());
        }

        // 5. 成功路径释放 OpenSSL 资源
        EVP_MD_CTX_free(context);

        return result;
    } catch (...) {
        // 异常路径 释放 OpenSSL 资源。
        EVP_MD_CTX_free(context);
        throw;
    }
}