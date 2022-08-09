project(stb)

set(SRCS stb_image.h stb_image_write.h stb.c)
add_library(stb STATIC ${SRCS})

target_include_directories(stb PUBLIC ${PROJECT_SOURCE_DIR})
source_group("" FILES ${SRCS})