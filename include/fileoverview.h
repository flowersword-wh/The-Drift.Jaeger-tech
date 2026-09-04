#pragma once

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

void  recursive_directory_reader(std::string & folderPath);

bool is_CorrectPath(std::string & path);