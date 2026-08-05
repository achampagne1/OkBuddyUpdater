#include "OkBuddyUpdater.h"
#include <curl/curl.h>

enum FLAGS{
	VERBOSE = 'v'
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

int main(int argc, char* argv[])
{
	CURLcode result;
	CURL* curl;
	std::string url = "";
	char* flags = nullptr;
	int flagLength = 0;
	char verboseFlag = 0;

	if (argc == 1) {
		std::cout << "No arguments provided." << std::endl;
		return 1;
	}
	else if (argc == 2) {
		url = argv[1];
	}
	else if (argc == 3) {
		if(argv[1][0] != '-'){
			std::cout<< "Invalid argument format. Expected a flag starting with '-'." << std::endl;
		}
		else if(argv[1][0] == '-' && strlen(argv[1]) < 2){
			std::cout<< "Invalid argument format. Flag must contain at least one character after '-'." << std::endl;
		}
		else{
			flags = argv[1];
			flagLength = strlen(flags);
		}
		url = argv[2];
	}

	while(flags != nullptr && *flags != '\0'){
		switch(*flags){
			case FLAGS::VERBOSE:
				verboseFlag = 1;
				std::cout << "Verbose mode enabled." << std::endl;
				break;
			default:
				std::cout << "Unknown flag: " << *flags << std::endl;
				break;
		}
		flags++;
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
		if(verboseFlag){
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

	return 0;
}
