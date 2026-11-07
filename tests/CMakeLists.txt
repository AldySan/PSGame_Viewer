add_executable(tst_psgcore tst_psgcore.cpp)

target_link_libraries(tst_psgcore PRIVATE Qt6::Test psgcore)
target_compile_definitions(tst_psgcore PRIVATE
  FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME psgcore COMMAND tst_psgcore)

# app-layer tests: controller flows (open/rename/batch/extract), the
# async trophy loader, settings, library and update checker
add_executable(tst_app tst_app.cpp)

target_link_libraries(tst_app PRIVATE Qt6::Test psgvapp)
target_compile_definitions(tst_app PRIVATE
  FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# the language-switch test loads the compiled .qm catalogs, which live
# in the executable's resources — give the test binary the same bundle
qt_add_resources(tst_app "i18n"
  PREFIX "/i18n"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/../i18n"
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../i18n/psgameviewer_zh_CN.qm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../i18n/psgameviewer_id.qm")

add_test(NAME app COMMAND tst_app)
set_tests_properties(app PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# the real GUI, headless: opens every supported fixture format through
# the assembled QML stack and renders a screenshot per format. The
# software renderer keeps it platform-independent (no GPU on CI).
add_test(NAME uitest
  COMMAND psgame-viewer --ui-test "${CMAKE_CURRENT_BINARY_DIR}/ui-shots")
set_tests_properties(uitest PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;PSGV_BASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/ui-test-data"
  TIMEOUT 300)
