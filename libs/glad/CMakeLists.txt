project(glad)

set(SRCS include/glad/glad.h src/glad.c)
add_library(glad STATIC ${SRCS})

target_include_directories(glad PUBLIC include)
target_link_libraries(glad ${CMAKE_DL_LIBS})
source_group("" FILES ${SRCS})