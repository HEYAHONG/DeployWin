cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0079 )
  cmake_policy(SET CMP0079 NEW)
endif()


add_subdirectory(src EXCLUDE_FROM_ALL)

#启用deploywin
function(deploywin_enable targetname)
target_link_libraries(${targetname} DeployWin)
endfunction()
