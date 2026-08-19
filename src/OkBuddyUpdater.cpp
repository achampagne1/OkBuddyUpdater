#include "OkBuddyUpdater.h"

extern "C" __declspec(dllexport)
void setUrl(const char* urlIn){
	url = std::string(urlIn);
}

extern "C" __declspec(dllexport)
void setFlagMaskUint(const uint32_t mask){
	flagMask = mask;
}

extern "C" __declspec(dllexport)
void setFlagMaskString(const char* mask){
	flagMask = parseFlags(mask);
}

extern "C" __declspec(dllexport)
void setRoot(const char* rootIn){
	root = rootIn;
}

extern "C" __declspec(dllexport)
void addIgnore(const char* ignore){
	ignoreList.push_back(ignore);
}

extern "C" __declspec(dllexport)
void addKill(const int pid){
	killList.push_back(std::to_string(pid));
}

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

static uint32_t parseFlags(const char* flags){
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

static std::vector<std::string>* parseArgList(char* args, std::vector<std::string>* argList){
	//example: ignore="file1.txt,file2.txt"
	//         kill="1012,1023"

	if(flagMask & FLAGS::VERBOSE){
		std::cout << "Parsing argument list: " << args << std::endl;
	}

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

static bool extractZip(const char* zipFile, fs::path destination)
{
    archive* in = archive_read_new();
    archive* out = archive_write_disk_new();

	if(flagMask & FLAGS::VERBOSE){
		std::cout << "Extracting zip file: " << zipFile << std::endl;
	}

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
		fs::path fullPath = destination / archive_entry_pathname(entry);

        archive_entry_set_pathname(entry, fullPath.string().c_str());

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

	archive_write_close(out);
	archive_write_free(out);

	archive_read_close(in);
	archive_read_free(in);

    return true;
}

static void copyItem(const fs::directory_entry entry,const fs::path& to,const fs::path& from){
	fs::path target = to / fs::relative(entry.path(), from);
	fs::create_directories(target.parent_path());
	if(entry.is_regular_file()){
		fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing);
	}
	else{
		fs::create_directories(target); //for empty folders
	}
}

static void deleteItem(const fs::directory_entry entry,const fs::path& to,const fs::path& from){
	fs::remove(entry);
}

static int recursiveExplore(DirectoryNode& node){
	for (const fs::directory_entry& childEntry : fs::directory_iterator(node.entry))
	{
		DirectoryNode child = {
			childEntry,
			node.action,
			node.arg1,
			node.arg2
		};

		if(ignoreMap.find(child.entry.path())==ignoreMap.end()){
			if(child.entry.is_directory()){
				int status = recursiveExplore(child);
				if(child.protectedFlag)
					node.protectedFlag = true;
				if(status)
					return status;
			}
			//reach bottom then perform action going back up
			try{
				if(!child.protectedFlag)
					child.action(child.entry,child.arg1,child.arg2);
				return 0;
			}
			catch(const std::runtime_error& e){
				std::cerr<<"Error during copy: " << e.what() <<std::endl;
				return 1;
			}
		}
		else{
			node.protectedFlag = true;
		}
	}
	return 0;
}

static int updateLoad(const std::string updatePath)
{
	std::error_code ec;
	fs::copy_options options = fs::copy_options::recursive;
	int status = 0;

	if(flagMask & FLAGS::VERBOSE){
		std::cout << "Updating files in: " << root << std::endl;
	}

	const fs::path backupPath = root / "tmpcpybak";
	if (fs::exists(backupPath)) {
		fs::remove_all(backupPath);
	}
	if (!fs::create_directory(backupPath)) {
		std::cout << "Failed to create backup directory." << std::endl;
		return 1;
	}
	ignoreMap[backupPath] = 1;

	if (flagMask & FLAGS::VERBOSE) {
		std::cout << "Backing up files to: " << backupPath << std::endl;
	}

	DirectoryNode rootNode = {
		fs::directory_entry(root),
		copyItem,
		backupPath,
		root
	};
	status = recursiveExplore(rootNode);
	if(status){
		std::cout<<"Something went wrong in backup phase"<<std::endl;
	}

	if(flagMask & FLAGS::VERBOSE){
		std::cout << "Removing files from: " << root << std::endl;
	}

	rootNode = {
		fs::directory_entry(root),
		deleteItem
	};
	status = recursiveExplore(rootNode);
	if(status){
		std::cout<<"Something went wrong in the deletion phase"<<std::endl;
		fs::copy(updatePath,root,options,ec);
		if(ec){
			std::cerr << "Error restoring files: " << ec.message() << "\n";
		}
		return 1;
	}

	if(flagMask & FLAGS::VERBOSE){
		std::cout << "Copying files from: " << updatePath << " to: " << root << std::endl;
	}

	rootNode = {
		fs::directory_entry(updatePath),
		copyItem,
		root,
		updatePath
	};
	status = recursiveExplore(rootNode);
	if(status){
		std::cout<<"Something went wrong in the update phase"<<std::endl;
		fs::copy(updatePath,root,options,ec);
		if(ec){
			std::cerr << "Error restoring files: " << ec.message() << "\n";
		}
		return 1;
	}

	if(flagMask & FLAGS::VERBOSE){
		std::cout << "Cleaning up temporary files." << std::endl;
	}

	fs::remove_all(backupPath, ec);
	if (ec) {
		std::cerr << "Error removing backup directory: " << ec.message() << "\n";
		return 1;
	}

	fs::remove_all("tmpZip", ec);
	if (ec) {
		std::cerr << "Error removing tmpZip directory: " << ec.message() << "\n";
		return 1;
	}

	return 0;
}

extern "C" __declspec(dllexport) 
int handleUpdate(){
	CURLcode result;
	CURL* curl;

	if(url.empty()){
		std::cout << "No URL provided." << std::endl;
		return 1;
	}

	if(root.empty()){
		std::cout<<"Root is not set, defaulting to current directory:"<<std::endl;
		std::cout<<"\t"<<fs::absolute(".")<<std::endl;
		root = fs::absolute(".");
	}

	for(std::string ignore : ignoreList){
		fs::path ignorePath = fs::path(ignore);
		if(ignorePath.is_relative()){
			ignorePath = root / ignorePath;
		}
		ignoreMap[ignorePath] = 1;
	}
	ignoreMap[root / "okbdupdater"] = 1;

	if(flagMask & FLAGS::VERBOSE && !ignoreMap.empty()){
		std::cout<<"Files to ignore:" << std::endl;
		for(const auto& [ignore, _] : ignoreMap){
			std::cout<< ignore<< std::endl;
		}
	}

	if(flagMask & FLAGS::VERBOSE && !killList.empty()){
		std::cout<<"Processes to kill:" << std::endl;
		for(std::string kill : killList){
			std::cout << kill<< std::endl;
		}
	}

	result = curl_global_init(CURL_GLOBAL_ALL);
	if (result != CURLE_OK) {
		std::cout << "Failed to initialize libcurl: " << curl_easy_strerror(result) << std::endl;
		return (int)result;
	}

	std::string zipPath = (root / "tmpZip.zip").string();
	std::string unzipPath = (root / "tmpZip").string();
	curl = curl_easy_init();
    if (curl) {
        FILE* pagefile;
        pagefile = fopen(zipPath.c_str(), "wb");
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


	bool status = extractZip(zipPath.c_str(), unzipPath);
	if(!status){
		std::cout << "Failed to extract zip file." << std::endl;
		return 1;
	}
	ignoreMap[unzipPath] = 1;

	status = remove("tmpZip.zip");
	if (status != 0) {
		std::cout << "Failed to remove zip file." << std::endl;
		return 1;
	}

	status = updateLoad(unzipPath);
	if(status != 0){
		std::cout << "Failed to update files." << std::endl;
		return 1;
	}

	return 0;
}

int main(int argc, char* argv[])
{
	if (argc == 1) {
		std::cout << "No arguments provided." << std::endl;
		return 1;
	}

	std::vector<std::string> ignoreList = std::vector<std::string>();
	
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
		else if(((std::string)argv[count]).substr(0, 4) == "root"){
			char* rootChar = argv[count];
			std::string rootStr;
			while(*rootChar != '=' && *rootChar != '\0'){
				rootChar++;
			}
			rootChar++;

			while(*rootChar!='\0'){
				rootStr+=rootChar;
				rootChar++;
			}

			if(!rootStr.empty())
				root = rootStr;
		}
		else { 
			url = argv[count];
		}
		count++;
	}

	return handleUpdate();
}
