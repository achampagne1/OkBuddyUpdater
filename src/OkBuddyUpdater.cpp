#include "OkBuddyUpdater.h"
#include <curl/curl.h>
#include <string>
#include <vector>
#include <archive.h>
#include <archive_entry.h>

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

static std::vector<std::string>* parseArgList(char* args){
	//example: ignore="file1.txt,file2.txt"
	//         kill="1012,1023"
	std::vector<std::string>* argList = new std::vector<std::string>();
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

int main(int argc, char* argv[])
{
	uint32_t flagMask = 0;
	CURLcode result;
	CURL* curl;
	std::string url = "";
	std::vector<std::string>* ignoreList;
	std::vector<std::string>* killList;

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
			ignoreList = parseArgList(argv[count]);
		}
		else if(((std::string)argv[count]).substr(0, 4) == "kill"){
			killList = parseArgList(argv[count]);
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

	if(flagMask & FLAGS::VERBOSE){
		std::cout<<"Files to ignore:" << std::endl;
		for(std::string ignore : *ignoreList){
			std::cout << "\t" << ignore<< std::endl;
		}
	}

	if(flagMask & FLAGS::VERBOSE){
		std::cout<<"Processes to kill:" << std::endl;
		for(std::string kill : *killList){
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
        pagefile = fopen("tempUpdateDirectory", "wb");
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

	for(std::string pid : *killList){
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

	delete ignoreList;
	delete killList;

	return 0;
}
