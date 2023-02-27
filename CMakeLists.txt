cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0079 )
  cmake_policy(SET CMP0079 NEW)
endif()


add_subdirectory(src EXCLUDE_FROM_ALL)

set_target_properties(DeployWin
			PROPERTIES DEPLOYWIN_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}
			)

#启用deploywin
function(deploywin_enable targetname)
target_link_libraries(${targetname} DeployWin)
endfunction()

#通过deploywin install(必须是可执行文件，若为动态库则无效)
include(GNUInstallDirs)
function(deploywin_install targetname)
get_target_property(TARGET_TYPE ${targetname} TYPE)
if(TARGET_TYPE STREQUAL "EXECUTABLE")
message(STATUS "${targetname} is  a executable!")
install(TARGETS ${targetname}
       RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
       LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
       ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
)
get_target_property(DEPLOYWIN_ROOT_DIR DeployWin DEPLOYWIN_ROOT_DIR)
get_target_property(DEPLOYWIN_BUILD DeployWin DEPLOYWIN_BUILD)
set(DEPLOYWIN_TARGET "${targetname}")
set(DEPLOYWIN_TARGET_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
configure_file(${DEPLOYWIN_ROOT_DIR}/deploywin.cmake.in  ${CMAKE_BINARY_DIR}/deploywin.${targetname}.cmake @ONLY)
install(SCRIPT ${CMAKE_BINARY_DIR}/deploywin.${targetname}.cmake)
else()
message(FATAL_ERROR "${targetname} is not a executable!")
endif()
endfunction()
