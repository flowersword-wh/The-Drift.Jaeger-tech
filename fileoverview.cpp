
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

void recursive_directory_reader(std::string &folderPath)
{
	std::ofstream writeFolderFile("fileoverview.txt");
	if (!writeFolderFile) {
		std::cerr << "open file failed" << std::endl;
	}
	for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
		if (entry.is_regular_file()) {
			writeFolderFile << entry.path().filename().string() << std::endl;
			if (!writeFolderFile) {
				std::cerr << "write file failed" << std::endl;
			}
		} else if(entry.is_directory()){
			//如果是目录先跳过，暂时只处理文件
			continue;
		}
	}
}