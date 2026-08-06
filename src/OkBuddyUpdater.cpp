#include "OkBuddyUpdater.h"
#include <curl/curl.h>
#include <vector>

enum FLAGS{
	DASH 	= 0,
	VERBOSE = 1,
	TEST 	= 2
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

static std::vector<std::string>* parseIgnore(char* ignores){
	//example: ignore:[file1.txt,file2.txt]
	std::vector<std::string>* ignoreList = new std::vector<std::string>();
	while(*ignores != '=' && *ignores != ' '){ //up to equal sign and trim leading spaces
		if(*ignores == '\0'){
			std::cout << "Invalid ignore list." << std::endl;
			return ignoreList;
		}
		ignores++;
	}
	ignores++; //first char

	while(*ignores != '\0'){
		std::string ignoreStr = "";
		while(*ignores != ',' && *ignores != ' ' && *ignores != '\0'){
			ignoreStr += *ignores;
			ignores++;
		}
		while(*ignores == ','||*ignores == ' '){//skip until you reach next name
			ignores++;
		}
		if(!ignoreStr.empty()){
			ignoreList->push_back(ignoreStr);
		}
	}
	return ignoreList;
}

int main(int argc, char* argv[])
{
	uint32_t flagMask = 0;
	CURLcode result;
	CURL* curl;
	std::string url = "";
	std::vector<std::string>* ignoreList;

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
			ignoreList = parseIgnore(argv[count]);
		}
		else {
			url = argv[count];
		}
		count++;
	}

	for(std::string ignore : *ignoreList){
		std::cout << "Ignoring: " << ignore <<"."<< std::endl;
	}

	if(url.empty()){
		std::cout << "No URL provided." << std::endl;
		return 1;
	}

	result = curl_global_init(CURL_GLOBAL_ALL);
	if (result != CURLE_OK) {
		std::cout << "Failed to initialize libcurl: " << curl_easy_strerror(result) << std::endl;
		return (int)result;
	}

	curl = curl_easy_init();
    if (curl) {
        FILE* pagefile;
        pagefile = fopen("switchTrack.zip", "wb");
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

	delete ignoreList;

	return 0;
}
