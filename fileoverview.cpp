
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

void readfolder(std::string &folderPath)
{
	std::ofstream writeFolderFile("overviewfile.txt");
	if (!writeFolderFile) {
		std::cerr << "open file failed" << std::endl;
	}
	for (const auto &entry : fs::directory_iterator(folderPath)) {
		if (!entry.is_directory()) {
			writeFolderFile << entry.path().string() << std::endl;
			if (!writeFolderFile) {
				std::cerr << "write file failed" << std::endl;
			}
		} else
			readfolder((std::string &) (entry.path()));
	}
}