
cmake_minimum_required(VERSION 3.14)

project(test C CXX ASM)


#添加可执行文件
add_executable(test)

#设置C++标准
set_property(TARGET test PROPERTY CXX_STANDARD 20)


#包含DeployWin
add_subdirectory(../ DeployWin)

#启用DeployWin
deploywin_enable(test)

#添加源代码
file(GLOB  TEST_CPP_C_FILES *.cpp *.CPP *.c *.C *.h)
target_sources(test PUBLIC ${TEST_CPP_C_FILES})


