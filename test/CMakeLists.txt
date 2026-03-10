# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 REQUIRED COMPONENTS Test)

enable_testing()

add_executable(test_path_resolver test_path_resolver.cpp)
target_include_directories(test_path_resolver PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_path_resolver PRIVATE Qt6::Test)
add_test(NAME test_path_resolver COMMAND test_path_resolver)
