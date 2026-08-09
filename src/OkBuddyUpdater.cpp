#include "OkBuddyUpdater.h"
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

enum FLAGS{
	DASH 	= 0x0,
	VERBOSE = 0x1,
	TEST 	= 0x2
};

static size_t writeCb(char* ptr, size_t size, size_t nmemb, void* stream)
{
    size_t written = fwrite(ptr, size, nmemb, (FILE*)stream);
    return written;
}

static size_t writeString(char* ptr, size_t size, size_t nmemb, void* stream)
{
	std::string* str = (std::string*)stream;
	str->append(ptr, size * nmemb);
	return size * nmemb;
}

static uint32_t parseFlags(char* flags){
	uint32_t outMask = 0;
	while(*flags != '\0'){
		switch(*flags){
			case 'v':
				std::cout << "Verbose mode enabled." << std::endl;
				outMask |= FLAGS::VERBOSE;
				break;
			case '-':
				outMask |= FLAGS::DASH; //does nothing
				break;
			case 't':
				std::cout << "Test mode enabled." << std::endl;
				outMask |= FLAGS::TEST;
				break;
			default:
				std::cout << "Unknown flag: " << *flags << std::endl;
				break;
		}
		flags++;
	}
	return outMask;
}

static std::vector<std::string>* parseArgList(char* args, std::vector<std::string>* argList = nullptr){
	//example: ignore="file1.txt,file2.txt"
	//         kill="1012,1023"
	if(!argList){
		argList = new std::vector<std::string>();
	}

	while(*args != '='){ //go up to =
		if(*args == '\0'){
			std::cout << "Invalid argument list." << std::endl;
			return argList;
		}
		args++;
	}
	args++; //first char

	while(*args != '\0'){
		std::string argStr = "";
		while(*args != ',' && *args != '\0'){ //go up to comma or end of string
			if(*args != ' '){ //ignore all spaces
				argStr += *args;
			}
			args++;
		}
		if(*args == ','){
			args++; //skip comma
		}

		if(!argStr.empty()){
			argList->push_back(argStr);
		}
	}
	return argList;
}

bool extractZip(const char* zipFile, const char* destination)
{
    archive* in = archive_read_new();
    archive* out = archive_write_disk_new();

    archive_read_support_format_zip(in);
    archive_read_support_filter_all(in);

    archive_write_disk_set_options(out,
        ARCHIVE_EXTRACT_TIME |
        ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_ACL |
        ARCHIVE_EXTRACT_FFLAGS);

    if (archive_read_open_filename(in, zipFile, 10240) != ARCHIVE_OK)
    {
        std::cout << archive_error_string(in) << std::endl;
        return false;
    }

    archive_entry* entry;

    while (archive_read_next_header(in, &entry) == ARCHIVE_OK)
    {
        std::string fullPath = std::string(destination) + "\\" + archive_entry_pathname(entry);

        archive_entry_set_pathname(entry, fullPath.c_str());

        int r = archive_write_header(out, entry);

        if (r == ARCHIVE_OK)
        {
            const void* buff;
            size_t size;
            la_int64_t offset;

            while (archive_read_data_block( in,&buff, &size, &offset) == ARCHIVE_OK)
            {
                archive_write_data_block( out, buff, size, offset);
            }
        }

        archive_write_finish_entry(out);
    }

    archive_write_free(out);
    archive_read_free(in);

    return true;
}

static int updateLoad(const std::string path, const std::string updatePath, std::vector<std::string>* ignoreList)
{
	namespace fs = std::filesystem;
	std::unordered_map<std::string, int> ignoreMap;
	std::error_code ec;

	const fs::path sourcePath = fs::absolute(path);
	const fs::path backupPath = sourcePath / "tmpcpybak";
	const std::string backupDirName = backupPath.filename().string();

	if (fs::exists(backupPath)) {
		fs::remove_all(backupPath);
	}

	if (!fs::create_directory(backupPath)) {
		std::cout << "Failed to create backup directory." << std::endl;
		return 1;
	}

	for (const std::string& ignore : *ignoreList) {
		ignoreMap[ignore] = 1;
	}
	ignoreMap[backupDirName] = 1;

	for (auto itEntry = fs::recursive_directory_iterator(sourcePath); itEntry != fs::recursive_directory_iterator(); ++itEntry) {
		const fs::path entryPath = itEntry->path();
		const fs::path relativePath = fs::relative(entryPath, sourcePath);
		const fs::path targetPath = backupPath / relativePath;
		const std::string filenameStr = entryPath.filename().string();
		const std::string relativeStr = relativePath.generic_string();

		if (ignoreMap.find(filenameStr) != ignoreMap.end()) {
			if (itEntry->is_directory()) {
				itEntry.disable_recursion_pending();
			}
			continue;
		}

		if (itEntry->is_directory()) {
			fs::create_directories(targetPath);
		}
		else if (itEntry->is_regular_file()) {
			fs::create_directories(targetPath.parent_path());
			fs::copy_file(entryPath, targetPath, fs::copy_options::overwrite_existing);
		}
	}

	std::vector<fs::path> entriesToRemove;
	for (auto itEntry = fs::recursive_directory_iterator(sourcePath); itEntry != fs::recursive_directory_iterator(); ++itEntry) {
		const fs::path entryPath = itEntry->path();
		const std::string filenameStr = entryPath.filename().string();

		if (ignoreMap.find(filenameStr) != ignoreMap.end()) {
			if (itEntry->is_directory()) {
				itEntry.disable_recursion_pending();
			}
			continue;
		}

		entriesToRemove.push_back(entryPath);
	}

	std::sort(entriesToRemove.begin(), entriesToRemove.end(), [](const fs::path& lhs, const fs::path& rhs) {
		return lhs.native().size() > rhs.native().size();
	});

	for (const fs::path& entryPath : entriesToRemove) {
		if (!fs::exists(entryPath)) {
			continue;
		}

		std::cout << "Removing: " << entryPath << std::endl;
		if (fs::is_directory(entryPath)) {
			fs::remove_all(entryPath);
		}
		else if (fs::is_regular_file(entryPath)) {
			fs::remove(entryPath);
		}
	}

	fs::copy_options options = fs::copy_options::recursive;

	fs::copy(updatePath, path, options, ec);
    if (ec) {
        std::cerr << "Error copying files: " << ec.message() << "\n";
        return 1;
    }

	fs::remove_all(backupPath, ec);
	if (ec) {
		std::cerr << "Error removing backup directory: " << ec.message() << "\n";
		return 0;
	}

	fs::remove_all("tmpZip", ec);
	if (ec) {
		std::cerr << "Error removing tmpZip directory: " << ec.message() << "\n";
		return 0;
	}

	return 0;
}

int main(int argc, char* argv[])
{
	uint32_t flagMask = 0;
	CURLcode result;
	CURL* curl;
	std::string url = "";
	std::vector<std::string> ignoreList = std::vector<std::string>();
	std::vector<std::string> killList = std::vector<std::string>();

	ignoreList.push_back("okbdupdater");

	if (argc == 1) {
		std::cout << "No arguments provided." << std::endl;
		return 1;
	}
	
	int count = 1;
	while (count < argc) {
		if (argv[count][0] == '-') {
			flagMask = parseFlags(argv[count]);
		} 
		else if(((std::string)argv[count]).substr(0, 6) == "ignore"){
			parseArgList(argv[count], &ignoreList);
		}
		else if(((std::string)argv[count]).substr(0, 4) == "kill"){
			parseArgList(argv[count], &killList);
		}
		else { 
			url = argv[count];
		}
		count++;
	}

	if(url.empty()){
		std::cout << "No URL provided." << std::endl;
		return 1;
	}

	if(flagMask & FLAGS::VERBOSE && !ignoreList.empty()){
		std::cout<<"Files to ignore:" << std::endl;
		for(std::string ignore : ignoreList){
			std::cout << "\t" << ignore<< std::endl;
		}
	}

	if(flagMask & FLAGS::VERBOSE && !killList.empty()){
		std::cout<<"Processes to kill:" << std::endl;
		for(std::string kill : killList){
			std::cout << "\t" << kill<< std::endl;
		}
	}

	result = curl_global_init(CURL_GLOBAL_ALL);
	if (result != CURLE_OK) {
		std::cout << "Failed to initialize libcurl: " << curl_easy_strerror(result) << std::endl;
		return (int)result;
	}

	curl = curl_easy_init();
    if (curl) {
        FILE* pagefile;
        pagefile = fopen("tmpZip.zip", "wb");
        std::string signedUrl = "";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		if(flagMask & FLAGS::VERBOSE){
        	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		}
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);

        result = curl_easy_perform(curl);

		if (result != CURLE_OK) {
			std::cout << "Failed to download: " << curl_easy_strerror(result) << std::endl;
			curl_easy_cleanup(curl);
			return (int)result;
		}

        fclose(pagefile);
        curl_easy_cleanup(curl);
    }
	else {
		std::cout << "Failed to initialize curl." << std::endl;
		return 1;
	}

	for(std::string pid : killList){
		#ifdef _WIN32
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, std::stoi(pid));
		if (hProcess == NULL) {
			std::cout << "Failed to open process with PID " << pid << std::endl;
			continue;
		}
		if (!TerminateProcess(hProcess, 0)) {
			std::cout << "Failed to terminate process with PID " << pid << std::endl;
		} else {
			std::cout << "Terminated process with PID " << pid << std::endl;
		}
		CloseHandle(hProcess);
		#elif __linux__
		if (kill(std::stoi(pid), SIGTERM) != 0) {
			std::cout << "Failed to terminate process with PID " << pid << std::endl;
		} else {
			std::cout << "Terminated process with PID " << pid << std::endl;
		}
		#endif
	}


	bool status = extractZip("tmpZip.zip", "tmpZip");
	if(!status){
		std::cout << "Failed to extract zip file." << std::endl;
		return 1;
	}

	status = remove("tmpZip.zip");
	if (status != 0) {
		std::cout << "Failed to remove zip file." << std::endl;
		return 1;
	}

	status = updateLoad("../", "tmpZip", &ignoreList);
	if(!status){
		std::cout << "Failed to update files." << std::endl;
		return 1;
	}

	return 0;
}
