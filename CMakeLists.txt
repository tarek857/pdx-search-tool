cmake_minimum_required(VERSION 3.16)

project(pdx-search-tool
    VERSION 0.1.0
    DESCRIPTION "PDX/ODX diagnostic package inspection tool"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

set(PUGIXML_INSTALL OFF CACHE BOOL "Disable pugixml install rules" FORCE)
add_subdirectory(external/pugixml)

add_library(CommandLineParser OBJECT src/CommandLineParser.cpp)
target_include_directories(CommandLineParser
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(pdxinfo
    src/Bytes.cpp
    src/OdxDidExtractor.cpp
    src/OdxDtcParser.cpp
    src/OdxMessageParser.cpp
    src/OdxNodeReader.cpp
    src/OdxParser.cpp
    src/OdxServiceParser.cpp
    src/PackageLoader.cpp
    src/Report.cpp
    src/ValueFinder.cpp
    src/Xml.cpp
    src/ZipArchive.cpp)

target_include_directories(pdxinfo
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(pdxinfo
    PUBLIC
        ZLIB::ZLIB
        pugixml::pugixml)

target_compile_options(pdxinfo PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(pdx-search-tool
    src/Application.cpp
    src/main.cpp
    $<TARGET_OBJECTS:CommandLineParser>)
target_link_libraries(pdx-search-tool PRIVATE pdxinfo)
