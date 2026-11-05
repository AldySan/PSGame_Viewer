cmake_minimum_required(VERSION 3.21)

project(PSGameViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Xml)
find_package(Qt6 6.4 QUIET OPTIONAL_COMPONENTS Test)

qt_standard_project_setup()

add_library(psgcore STATIC
  src/core/types.cpp
  src/core/util.cpp
  src/core/aes.cpp
  src/core/sfo.cpp
  src/core/iso9660.cpp
  src/core/details.cpp
  src/core/ps2/ps2disc.cpp
  src/core/ps3/ps3pkg.cpp
  src/core/ps3/ps3folder.cpp
  src/core/ps3/ps3iso.cpp
  src/core/ps4/ps4pkg.cpp
  src/core/ps5/ps5pkg.cpp
  src/core/ps5/exfat.cpp
  src/core/ps5/ps5image.cpp
  src/core/trophies.cpp
  src/core/entrytree.cpp
  src/core/extract.cpp
  src/core/rename.cpp
  src/core/detect.cpp
)

target_include_directories(psgcore PUBLIC src)
target_link_libraries(psgcore
  PUBLIC
    Qt6::Core
    Qt6::Xml
)

if(BUILD_TESTING AND TARGET Qt6::Test)
  enable_testing()
  add_subdirectory(tests)
endif()
