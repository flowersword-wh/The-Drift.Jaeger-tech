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

bool is_CorrectPath(std::string &path);

int ServerFiles_Count(std::string &folderPath);