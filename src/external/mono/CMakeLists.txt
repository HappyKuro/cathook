cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(mono LANGUAGES CXX)
endif()

add_library(mono STATIC
	bindings.cpp
	entity_esp.cpp
	menu_shell.cpp
	overlays.cpp
	runtime.cpp
	theme.cpp
	window_chrome.cpp
	widgets.cpp
)

set_target_properties(mono PROPERTIES
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_compile_features(mono PUBLIC cxx_std_20)
target_include_directories(mono PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(DEFINED MONO_IMGUI_INCLUDE_DIR)
	if(NOT DEFINED MONO_IMGUI_HEADER)
		set(MONO_IMGUI_HEADER "imgui/imgui.h")
	endif()
	if(NOT DEFINED MONO_IMGUI_INTERNAL_HEADER)
		set(MONO_IMGUI_INTERNAL_HEADER "imgui/imgui_internal.h")
	endif()
	target_include_directories(mono PUBLIC "${MONO_IMGUI_INCLUDE_DIR}")
	target_compile_definitions(mono PUBLIC MONO_IMGUI_HEADER="${MONO_IMGUI_HEADER}" MONO_IMGUI_INTERNAL_HEADER="${MONO_IMGUI_INTERNAL_HEADER}")
endif()
