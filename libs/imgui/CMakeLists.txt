project(imgui)

set(SRCS 
    imconfig.h
    imgui.cpp
    imgui.h
    ImGuizmo.cpp
    ImGuizmo.h
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_impl_glfw.cpp
    imgui_impl_glfw.h
    imgui_impl_opengl3.cpp
    imgui_impl_opengl3.h
    imgui_impl_opengl3_loader.h
    imgui_internal.h
    imgui_tables.cpp
    imgui_widgets.cpp
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
)

add_library(imgui STATIC ${SRCS})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imgui glfw glad glm)
source_group("" FILES ${SRCS})