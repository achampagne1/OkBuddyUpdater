#pragma once

#define UPDATER_API __declspec(dllexport)

#include <iostream>
#include <curl/curl.h>
#include <string>
#include <vector>
#include <archive.h>
#include <archive_entry.h>
#include <filesystem>
#include <unordered_map>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

enum FLAGS {
	DASH = 0x0,
	VERBOSE = 0x1,
	TEST = 0x2
};

static uint32_t flagMask = 0;
static std::string url = "";
static std::vector<std::string> ignoreList = std::vector<std::string>();
static std::vector<std::string> killList = std::vector<std::string>();

 
extern "C" UPDATER_API void setFlagMaskUint(const uint32_t mask);
extern "C" UPDATER_API void setFlagMaskString(const char* mask);
extern "C" UPDATER_API void addIgnore(const char* ignore);
extern "C" UPDATER_API void addKill(const int pid);

static size_t writeCb(char* ptr, size_t size, size_t nmemb, void* stream);
static size_t writeString(char* ptr, size_t size, size_t nmemb, void* stream);
static uint32_t parseFlags(const char* flags);
static std::vector<std::string>* parseArgList(char* args, std::vector<std::string>* argList = nullptr);
static bool extractZip(const char* zipFile, const char* destination);
static int updateLoad(const std::string path, const std::string updatePath, std::vector<std::string>* ignoreList = nullptr);
extern "C" UPDATER_API int handleUpdate();

