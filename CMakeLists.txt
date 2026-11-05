cmake_minimum_required(VERSION 3.21)

project(PSGameViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# the GUI needs Quick/Controls/Network (Qt 6.5+ for loadFromModule);
# the core stays Core+Xml only
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Xml Quick QuickControls2 Network)
find_package(Qt6 6.5 QUIET OPTIONAL_COMPONENTS Test)

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

qt_add_executable(psgame-viewer
  src/app/main.cpp
  src/app/appsettings.cpp
  src/app/imagestore.cpp
  src/app/entrytreemodel.cpp
  src/app/trophylistmodel.cpp
  src/app/online.cpp
  src/app/viewercontroller.cpp
)

set_source_files_properties(src/qml/Theme.qml PROPERTIES
  QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(psgame-viewer
  URI PSGameViewer
  VERSION 1.0
  QML_FILES
    src/qml/Theme.qml
    src/qml/Main.qml
    src/qml/ViewerPage.qml
    src/qml/CoverPanel.qml
    src/qml/SpecCard.qml
    src/qml/EmptyState.qml
    src/qml/FilesTab.qml
    src/qml/DetailsTab.qml
    src/qml/TrophiesTab.qml
    src/qml/NamePatternBar.qml
    src/qml/RenameDialog.qml
    src/qml/BatchDialog.qml
    src/qml/AboutDialog.qml
)

set_target_properties(psgame-viewer PROPERTIES
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE)

target_link_libraries(psgame-viewer
  PRIVATE
    psgcore
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network)

# app icon + desktop entry
set(APP_ICON_FILES
  assets/icons/psgame-viewer-16.png
  assets/icons/psgame-viewer-32.png
  assets/icons/psgame-viewer-48.png
  assets/icons/psgame-viewer-64.png
  assets/icons/psgame-viewer-128.png
  assets/icons/psgame-viewer-256.png)

if(WIN32)
  # placeholder for a proper .rc on Windows
elseif(APPLE)
  set(ICON_PATHS)
  foreach(icon ${APP_ICON_FILES})
    list(APPEND ICON_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/${icon})
  endforeach()
  set_target_properties(psgame-viewer PROPERTIES MACOSX_BUNDLE_ICON_FILE "")
else()
  foreach(icon ${APP_ICON_FILES})
    get_filename_component(size ${icon} NAME_WE)
    string(REGEX REPLACE "psgame-viewer-" "" size ${size})
    install(FILES ${icon}
            DESTINATION share/icons/hicolor/${size}x${size}/apps
            RENAME psgame-viewer.png)
  endforeach()
  install(FILES assets/psgame-viewer.desktop
          DESTINATION share/applications)
  install(TARGETS psgame-viewer BUNDLE DESTINATION . RUNTIME DESTINATION bin)
endif()

if(BUILD_TESTING AND TARGET Qt6::Test)
  enable_testing()
  add_subdirectory(tests)
endif()

