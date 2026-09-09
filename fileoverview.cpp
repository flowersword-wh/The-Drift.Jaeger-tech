
#include "include/fileoverview.h"
#include <fileapi.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

bool is_CorrectPath(std::string &path)
{
	if (!fs::exists(path)) {
		std::cerr << "路径不存在！" << std::endl;
		return false;
	}
	if (!fs::is_directory(path)) {
		std::cerr << "文件夹不存在！" << std::endl;
		return false;
	}
	return true;
}

int ServerFiles_Count(std::string &folderPath)
{
	int count = 0;
	for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
		if (entry.is_regular_file()) {
			count++;
		}
	}
	return count;
}