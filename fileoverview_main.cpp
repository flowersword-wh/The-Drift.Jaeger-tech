#include "include/fileoverview.h"

#include <fileapi.h>
#include <iostream>
#include <string>

int main()
{
	std::string path;

	std::cout << "请输入要获取的文件夹路径：" << std::endl;
	std::getline(std::cin, path);

	if (getFileOverview(path) != 0) {
		std::cout << "获取失败" << std::endl;
	}

	is_CorrectPath(path);

	readfolder(path);
	

	return 0;
}
