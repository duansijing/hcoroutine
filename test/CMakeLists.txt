# test/CMakeLists.txt
include(GoogleTest)

# 获取编译器的 bin 目录, 用于设置测试运行时 PATH
get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)

function(add_hco_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE hcoroutine pthread)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "PATH=${MINGW_BIN_DIR}\\;%PATH%"
    )
endfunction()

add_subdirectory(unit)
