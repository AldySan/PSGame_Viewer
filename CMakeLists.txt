cmake_minimum_required(VERSION 3.21)

project(PSGameViewer VERSION 0.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# the GUI needs Quick/Controls/Network (Qt 6.5+ for loadFromModule);
# the core stays Core+Xml only
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Xml Quick QuickControls2 Network Sql)
find_package(Qt6 6.5 QUIET OPTIONAL_COMPONENTS Test)
find_package(ZLIB REQUIRED)

qt_standard_project_setup()

# QML module under the :/qt/qml resource prefix so the compiled-in module
# resolves from qrc:/qt/qml (the engine's default import path). Without
# this the app only runs from the build tree — copy the executable
# elsewhere (portable archive) and "Module PSGameViewer contains no type
# named Main" follows.
qt_policy(SET QTP0001 NEW)

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
  src/core/ps5/pfs.cpp
  src/core/ps5/ufs2.cpp
  src/core/ps5/ps5image.cpp
  src/core/trophies.cpp
  src/core/entrytree.cpp
  src/core/extract.cpp
  src/core/rename.cpp
  src/core/detect.cpp
  src/core/db/library.cpp
)

target_include_directories(psgcore PUBLIC src)
target_link_libraries(psgcore
  PUBLIC
    Qt6::Core
    Qt6::Xml
    Qt6::Sql
    ZLIB::ZLIB
)

# every app-layer piece except main.cpp lives in an object library so
# the controller stack can be linked into the unit tests as well
add_library(psgvapp OBJECT
  src/app/librarycontroller.cpp
  src/app/appsettings.cpp
  src/app/imagestore.cpp
  src/app/entrytreemodel.cpp
  src/app/trophylistmodel.cpp
  src/app/online.cpp
  src/app/trophyloader.cpp
  src/app/updatechecker.cpp
  src/app/viewercontroller.cpp
)
target_include_directories(psgvapp PUBLIC src)
target_link_libraries(psgvapp
  PUBLIC
    psgcore
    Qt6::Quick
    Qt6::Network
    Qt6::Sql)

qt_add_executable(psgame-viewer
  src/app/main.cpp
  $<TARGET_OBJECTS:psgvapp>
)

# the displayed/about version comes from the project version
target_compile_definitions(psgame-viewer
  PRIVATE PSGV_VERSION="${PROJECT_VERSION}")

if(WIN32)
  # the resource file is generated from a template: the icon path is
  # absolute and the version fields come from the project version, so the
  # resource compiles the same way whatever working directory rc.exe runs
  # in (Ninja and the Visual Studio generators differ there). Keep the
  # forward-slash form of the path — rc.exe eats single backslashes as
  # C-style escapes (\a in "D:\a\..." would swallow the drive directory)
  set(PSGV_ICON_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/psgame-viewer.ico")
  configure_file(src/app/psgame-viewer.rc.in src/app/psgame-viewer.rc @ONLY)
  target_sources(psgame-viewer PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/src/app/psgame-viewer.rc")
endif()

set_source_files_properties(src/qml/Theme.qml PROPERTIES
  QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(psgame-viewer
  URI PSGameViewer
  VERSION 1.0
  QML_FILES
    src/qml/Theme.qml
    src/qml/controls/AppButton.qml
    src/qml/controls/AppCheckBox.qml
    src/qml/controls/AppComboBox.qml
    src/qml/controls/AppScrollBar.qml
    src/qml/controls/AppTabButton.qml
    src/qml/controls/AppTextField.qml
    src/qml/controls/AppToast.qml
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
    src/qml/UpdateDialog.qml
    src/qml/LibraryPage.qml
    src/qml/SettingsDialog.qml
)

# translations: .ts sources live in i18n/, compiled .qm are committed
# (regenerate with lrelease i18n/*.ts after updating the sources)
qt_add_resources(psgame-viewer "i18n"
  PREFIX "/i18n"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/i18n"
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/i18n/psgameviewer_zh_CN.qm"
    "${CMAKE_CURRENT_SOURCE_DIR}/i18n/psgameviewer_id.qm"
)

set_target_properties(psgame-viewer PROPERTIES
  MACOSX_BUNDLE TRUE
  WIN32_EXECUTABLE TRUE)

target_link_libraries(psgame-viewer
  PRIVATE
    psgvapp
    psgcore
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network)

# portable distribution only: no install rules, no setup installer.
# Everything the app writes lands next to the executable (see
# src/app/portable.h); binary archives are produced by
# packaging/make-portable.sh (Linux) and packaging/make-portable.ps1
# (Windows).

if(BUILD_TESTING AND TARGET Qt6::Test)
  enable_testing()
  # the offscreen UI matrix mode is compiled into the GUI binary for the
  # uitest; release builds without BUILD_TESTING never see --ui-test
  target_compile_definitions(psgame-viewer PRIVATE
    PSGV_UI_TEST=1 FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
  add_subdirectory(tests)
endif()

