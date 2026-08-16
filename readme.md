# OkBuddyUpdater
OkBuddyUpdater is a library (or standalone exe) designed to assist in updating delivered software apps on Windows. 
# Features 
When an app that OkBuddyUpdater is integrated into has a new version released, it will:
 1. Pull compiled binaries from a specified url
 2. Kill specified processes before updating
 3. Create a backup of the current binaries in the event of a failure
 4. Replace old binaries with the newer versioned binaries
# Configuration
It can be compiled in 2 ways: shared library (.dll) or standalone (.exe).
Standalone is good for simpler project where UI is not a big considerations. It can be commanded to begin with a cli call, and will run until completion. [Example](#standalone)
Shared library is better for plugging into project where the user wants to define its own UI. It will act as a normal shared library where API calls are made to it. [C# example](#shared-library).
# Examples
## Standalone

    .\okbdupdater.exe https://endpoint.com/your/binaries kill="1234"
This call will kill process 1234, download all files stored at the endpoint, and update everything contained in the defined root of the project.
## Shared Library
    [DllImport(@"okbdupdater\okbdupdater.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern void setUrl(string urlIn);
    [DllImport(@"okbdupdater\okbdupdater.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern int handleUpdate();
    [DllImport(@"okbdupdater\okbdupdater.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern void addIgnore(string ignore);

    setUrl("https://endpoint.com/your/binaries");
    addIgnore("updater.dll");
    addIgnore("updater.exe");
    handleUpdate();
This sets up the API between the main updater program (containing ui and other logic) and OkBuddyUpdater.
It then sets the endpoint, adds multiple files to be ignored by the updater, then triggers the update. The specific program that includes the shared library must be ignored to avoid crashes. 
 


# Configure

    cmake --preset x64-debug -DBUILD_AS_DLL=OFF //standalone (.exe)
    cmake --preset x64-debug -DBUILD_AS_DLL=ON //shared library (.dll)
    cmake --build out\build\x64-debug --config Debug --target okbdupdater