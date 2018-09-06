cmake_minimum_required(VERSION 3.1)
project(Path-Tracer)

cmake_policy(SET CMP0015 NEW)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build: ${CMAKE_BUILD_TYPE} - Note: This does not apply for MSVC.")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wall -pedantic -Werror -Wno-unused-parameter -Wno-nested-anon-types")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

string(TOLOWER ${CMAKE_BUILD_TYPE} BUILD_TYPE)

file(GLOB_RECURSE
     SRC_LIST
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     source/main.cpp source/application/*.cpp source/application/*.h*)

file(GLOB_RECURSE
	 CL_KERNELS
	 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	 assets/*.cl*)
	 
file(GLOB_RECURSE
     ASSET_SOURCE_FILES
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     assets/*.frag assets/*.vert assets/*.geom assets/*.glsl assets/*.cl* assets/*.comp*)
	 
file(GLOB_RECURSE ALL_ASSET_FILES
	 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	 assets/*)

set(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(ASSET_DIR assets CACHE PATH "Asset Root Folder")
add_definitions(-DASSET_ROOT_FOLDER="${ASSET_DIR}/")

# Set output directories and copy assets to binary directory
if(MSVC)
	message(STATUS "***** Using MSVC compiler *****")
			
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        file(GLOB DLLS DLLs/x64/*)
    else()
        file(GLOB DLLS DLLs/x86/*)
    endif()
	
	if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()
	
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4310 /wd4127 /wd4100 /wd4201 /wd4530 /wd4577 /wd4515 /wd4324 /wd4505 /wd4996 /wd4800")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4310 /wd4127 /wd4100 /wd4201 /wd4530 /wd4577 /wd4515 /wd4324 /wd4505 /wd4996 /wd4800")
	
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
	set(PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Temp)

    # Handle Visual Studio Debug/Release... config folders
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/bin)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/lib)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/lib)
        set(PDB_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/Temp)
		
		file(COPY ${DLLS} DESTINATION ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/bin)
		file(COPY ${ASSET_DIR} DESTINATION ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/bin)
    endforeach()

	# Create source groups for Visual Studio filters
	# Get all directories first:
	set(dir_list "")
	foreach(file_path ${SRC_LIST} ${ASSET_SOURCE_FILES})
		get_filename_component(dir_path ${file_path} PATH)
		set(dir_list ${dir_list} ${dir_path})
	endforeach()
	list(REMOVE_DUPLICATES dir_list)

	file(GLOB sources *.cpp *.h*)
	source_group(\\ FILES ${sources})

	foreach(dir ${dir_list})
		set(abs_dir "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
		file(GLOB sources ${abs_dir}/*.cpp ${abs_dir}/*.h* ${abs_dir}/*.vert ${abs_dir}/*.frag ${abs_dir}/*.geom ${abs_dir}/*.glsl ${abs_dir}/*.cl* ${abs_dir}/*.comp* ${abs_dir}/*.cu*)
		string(REPLACE "/" "\\\\" Group ${dir})
		source_group(${Group} FILES ${sources})
	endforeach()

    # Copy DLLs and assets    
    file(COPY ${DLLS} DESTINATION ${CMAKE_BINARY_DIR}/bin)
    file(COPY ${ASSET_DIR} DESTINATION ${CMAKE_BINARY_DIR}/bin)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

    # Copy assets to the binary directory
    file(COPY ${ASSET_DIR} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

set(SDL2 "${CMAKE_SOURCE_DIR}/third_party/SDL2-2.0.4")
set(GLEW "${CMAKE_SOURCE_DIR}/third_party/glew-1.13.0")
	
#add_definitions(-DGLM_FORCE_LEFT_HANDED) # Causes VS2015 to throw "cmd.exe" exited with code 1

add_subdirectory(third_party)
INCLUDE_DIRECTORIES(source)
INCLUDE_DIRECTORIES(third_party/soil2)
INCLUDE_DIRECTORIES(third_party/glm)
INCLUDE_DIRECTORIES(third_party/imgui)
INCLUDE_DIRECTORIES(third_party/assimp/include)
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR}/third_party/assimp/include)
INCLUDE_DIRECTORIES(third_party/RadeonRays/RadeonRays/include)
INCLUDE_DIRECTORIES(${NATIVE_FILE_DIALOG_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${ASSET_DIR}/kernels)

find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

add_definitions(${OpenGL_DEFINITIONS})
include_directories(${SDL2_INCLUDE_DIR} ${OpenGL_INCLUDE_DIRS} ${GLEW_INCLUDE_DIR})
link_directories(${OpenGL_LIBRARY_DIRS})

set(KERNEL_INCLUDE_DIRS "-I assets/kernels")
add_definitions(-DKERNEL_INCLUDE_DIRS="${KERNEL_INCLUDE_DIRS}")

add_subdirectory(source/engine)

add_executable(${PROJECT_NAME} ${ASSET_SOURCE_FILES} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} imgui engine soil2 assimp RadeonRays nativefiledialog 
									  ${SDL2_LIBRARY} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES})
