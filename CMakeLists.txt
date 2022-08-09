cmake_minimum_required(VERSION 3.8)
project(sphere_decals)

set (CMAKE_CXX_STANDARD 20)

find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
    include_directories(${OPENGL_INCLUDE_DIRS})
    link_libraries(${OPENGL_LIBRARIES})
endif()
add_subdirectory(libs/glad)
add_subdirectory(libs/glfw)
add_subdirectory(libs/glm)
add_subdirectory(libs/cgltf)
add_subdirectory(libs/stb)
add_subdirectory(libs/imgui)
set(COMMON_LIBS
    glm
    cgltf
	stb
	imgui
)

set(SRCS
	src/main.cpp
	src/utils.hpp src/utils.cpp
)
add_executable(sphere_decals ${SRCS})

source_group("" FILES ${SRCS})

target_link_libraries(sphere_decals
	glad
	glfw
    ${COMMON_LIBS}
)
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT sphere_decals)
set_property(TARGET sphere_decals PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")