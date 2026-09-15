#include "include/filehash.h"
#include <cstddef>
#include <ios>
#include <openssl/evp.h>
#include <fstream>

// 计算文件哈希值
bool calculate_hash(const std::filesystem::path &path, Sha256 &result)
{
	// 先打开文件 二进制读取
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}

	// 创建OpenSSL 上下文
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	if (ctx == nullptr) {
		return false;
	}

	bool flag = false;

	// 初始化摘要上下文参数 ctx
	if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
		EVP_MD_CTX_free(ctx);
		return false;
	}

	// 创建读取缓冲区 64KB
	std::array<char, 64 * 1024> buffer{};

	// 分块读取文件 每次读64KB
	while (file) {
		file.read(buffer.data(), buffer.size());
		const std::streamsize count = file.gcount();

		// 将读到的数据加入SHA-256计算
		if (count > 0) {
			if (EVP_DigestUpdate(ctx, buffer.data(), (std::size_t) (count)) != 1) {
				EVP_MD_CTX_free(ctx);
				return false;
			}
		}
		//多次读取文件 设置flag值来表明是否全部成功
		flag = true;
	}
	if (!file.eof()) {
		EVP_MD_CTX_free(ctx);
		return false;
	}
	// 生成最终hash
	unsigned int digestLength = 0;
	if (EVP_DigestFinal_ex(ctx, result.data(), &digestLength) != 1) {
		EVP_MD_CTX_free(ctx);
		return false;
	}
	EVP_MD_CTX_free(ctx);
	return flag;
}
