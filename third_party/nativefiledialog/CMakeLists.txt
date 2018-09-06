cmake_minimum_required(VERSION 2.6)

project(nativefiledialog)
set(NATIVE_FILE_DIALOG_SRCS nfd_common.c)

if(WIN32)
set(NATIVE_FILE_DIALOG_SRCS ${NATIVE_FILE_DIALOG_SRCS} nfd_win.cpp)
elseif(UNIX)
set(NATIVE_FILE_DIALOG_SRCS ${NATIVE_FILE_DIALOG_SRCS} nfd_gtk.c)
elseif(APPLE)
set(NATIVE_FILE_DIALOG_SRCS ${NATIVE_FILE_DIALOG_SRCS} nfd_cocoa.m)
endif()
	 
set(NATIVE_FILE_DIALOG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE FILEPATH "Native file dialog include directory.")

include_directories(${NATIVE_FILE_DIALOG_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_library(${PROJECT_NAME} STATIC ${NATIVE_FILE_DIALOG_SRCS})