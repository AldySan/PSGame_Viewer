add_executable(tst_psgcore tst_psgcore.cpp)

target_link_libraries(tst_psgcore PRIVATE Qt6::Test psgcore)
target_compile_definitions(tst_psgcore PRIVATE
  FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME psgcore COMMAND tst_psgcore)
