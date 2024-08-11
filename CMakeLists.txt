cmake_minimum_required(VERSION 3.10)
project(bsp_project)
add_executable(bsp bsp.c)
if(WIN32)
    # Windows specific options (libm is not needed)
else()
    # Linux and other UNIX-like systems
    target_link_libraries(bsp m)
endif()
