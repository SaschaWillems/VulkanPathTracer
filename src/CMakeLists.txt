SET(EXAMPLE_NAME "VulkanPathTracer")
file(GLOB SHADERS "../data/shaders/*.rahit" "../data/shaders/*.rchit" "../data/shaders/*.rmiss" "../data/shaders/*.rgen")
file(GLOB SHADER_INCLUDES "../data/shaders/includes/*.glsl")
file(GLOB CLASSES_SOURCE "classes/*.cpp")
file(GLOB CLASSES_HEADERS "classes/*.h")
source_group("Shaders" FILES ${SHADERS})
source_group("Shaders\\Includes" FILES ${SHADER_INCLUDES})
source_group("Classes" FILES ${CLASSES_SOURCE})
source_group("Classes" FILES ${CLASSES_HEADERS})

# volk
set(VOLK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/volk)
set(VOLK_SOURCES ${VOLK_DIR}/volk.c ${VOLK_DIR}/volk.h)
source_group("External\\volk" FILES ${VOLK_SOURCES})

# imgui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui)
file(GLOB IMGUI_SRC ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_widgets.cpp ${IMGUI_DIR}/imgui_tables.cpp)
source_group("External\\imgui" FILES ${IMGUI_SRC})

SET(MAIN_CPP main.cpp)
SET(MAIN_HEADER main.h)

if(WIN32)
	add_executable(${EXAMPLE_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${SHADER_INCLUDES} ${CLASSES_SOURCE} ${CLASSES_HEADERS} ${IMGUI_SRC})
	target_link_libraries(${EXAMPLE_NAME} ${Vulkan_LIBRARY} ${WINLIBS})
else(WIN32)
	add_executable(${EXAMPLE_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS} ${SHADER_INCLUDES} ${CLASSES_SOURCE} ${CLASSES_HEADERS} ${IMGUI_SRC})
	target_link_libraries(${EXAMPLE_NAME})
endif(WIN32)
if(RESOURCE_INSTALL_DIR)
	install(TARGETS ${EXAMPLE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
