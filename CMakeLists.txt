# CMakeList.txt : CMake project for OkBuddyUpdater, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("OkBuddyUpdater")

option(BUILD_AS_DLL "Build the project as a shared library/DLL instead of an executable" OFF)

find_package(CURL REQUIRED)
find_package(Libarchive REQUIRED)

# Add source to this project's executable or shared library.
if(BUILD_AS_DLL)
  add_library(okbdupdater SHARED "src/OkBuddyUpdater.cpp" "src/OkBuddyUpdater.h")
else()
  add_executable(okbdupdater "src/OkBuddyUpdater.cpp" "src/OkBuddyUpdater.h")
endif()

target_link_libraries(okbdupdater PRIVATE CURL::libcurl LibArchive::LibArchive)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  set_property(TARGET okbdupdater PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
