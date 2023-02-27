cmake_minimum_required(VERSION 3.16)

#设置C++标准
set(CMAKE_CXX_STANDARD 17)

set(DEPLOYWIN_BUILD OFF)

if(WIN32)
set(DEPLOYWIN_BUILD ON)
endif()

#支持MSYS编译
if(MSYS)
set(DEPLOYWIN_BUILD ON)
endif()

#支持CYGWIN编译
if(CYGWIN)
set(DEPLOYWIN_BUILD ON)
endif()

include(CheckIncludeFileCXX)


check_include_file_cxx(windows.h    HAVE_WINDOWS_H)
if(NOT  HAVE_WINDOWS_H)
set(DEPLOYWIN_BUILD OFF)
endif()


include(CheckSymbolExists)

check_symbol_exists(getenv "stdlib.h" HAVE_GETENV)
if(NOT HAVE_GETENV)
set(DEPLOYWIN_BUILD OFF)
endif()

check_symbol_exists(strcasecmp "string.h" HAVE_STRCASECMP)
check_symbol_exists(stricmp "string.h" HAVE_STRICMP)


if(MSVC)
    add_compile_options(-utf-8 )
    #暂时不支持MSVC
    set(DEPLOYWIN_BUILD OFF)
endif()

add_library(DeployWin STATIC)


#添加线程库
find_package(Threads REQUIRED)
target_link_libraries(DeployWin  ${CMAKE_THREAD_LIBS_INIT})



if(HAVE_WINDOWS_H)
target_compile_definitions(DeployWin PRIVATE -D_WIN32_WINNT=_WIN32_WINNT_VISTA)
endif()


if(DEPLOYWIN_BUILD)
file(GLOB  DEPLOYWIN_CPP_FILES *.cpp *.h)
target_sources(DeployWin PUBLIC ${DEPLOYWIN_CPP_FILES})
target_include_directories(DeployWin PUBLIC ./)
else()
file(GLOB  DEPLOYWIN_CPP_FILES dummy/*.cpp *.h)
target_sources(DeployWin PUBLIC ${DEPLOYWIN_CPP_FILES})
target_include_directories(DeployWin PUBLIC ./)
endif()

set_target_properties(DeployWin
                        PROPERTIES DEPLOYWIN_BUILD ${DEPLOYWIN_BUILD}
                        )

