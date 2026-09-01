
#include "include/fileoverview.h"
#include <string>

int getFileOverview(const std::string &folderPath)
{
	try {
		// 检查是否存在
		if (!fs::exists(folderPath)) {
			std::cerr << "路径不存在！" << std::endl;
			return -1;
		}
		if (!fs::is_directory(folderPath)) {
			std::cerr << "文件夹不存在！" << std::endl;
			return -1;
		}

		// 遍历文件夹 将所有文件名写入txt文件
		std::ofstream file("fileoverview.txt", std::ios::out);
		if (!file) {
			std::cerr << "Unable to open file!" << std::endl;
			return -1;
		}
		for (const auto &entry : fs::directory_iterator(folderPath)) {
			file << entry.path().filename().string() << std::endl;
			if (!file) {
				std::cerr << "写入文件失败！" << std::endl;
				return -1;
			}
		}
	} catch (const fs::filesystem_error &e) {
		std::cerr << "文件系统错误： " << e.what() << std::endl;
		return -1;
	} catch (const std::exception &e) {
		std::cerr << "其他错误： " << e.what() << std::endl;
		return -1;
	}
	return 0;
}
