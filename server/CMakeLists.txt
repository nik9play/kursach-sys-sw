cmake_minimum_required(VERSION 3.20)
project(server)

set(CMAKE_CXX_STANDARD 14)
link_libraries(ws2_32 wsock32)
include_directories(.)

add_executable(hidwapi
        hid.c
        hid.h
        hidapi.h
        hidapi_cfgmgr32.h
        hidapi_hidclass.h
        hidapi_hidpi.h
        hidapi_hidsdi.h
        main.c fonts.c fonts.h)
