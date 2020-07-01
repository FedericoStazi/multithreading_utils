find_package(PkgConfig)
pkg_check_modules(GTEST "gtest" REQUIRED)
message(STATUS "gtest found: " ${GTEST_FOUND})

add_executable(UnitTests test_main.cpp semaphore.cpp)

target_link_libraries(UnitTests ${GTEST_LIBRARIES})