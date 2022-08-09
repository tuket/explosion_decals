project(cgltf)

set(SRCS cgltf.h cgltf.c)
add_library(cgltf STATIC ${SRCS})
target_include_directories(cgltf PUBLIC ${PROJECT_SOURCE_DIR})
source_group("" FILES ${SRCS})