project(glm)

add_library(glm INTERFACE glm.natvis)

target_include_directories(glm INTERFACE ${PROJECT_SOURCE_DIR})