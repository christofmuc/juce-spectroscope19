#
#  Copyright (c) 2019 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.22)

project(juce-spectroscope19 VERSION 1.0.0 LANGUAGES C CXX)

include(CTest)

set(JUCE_SPECTROSCOPE_IS_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	set(JUCE_SPECTROSCOPE_IS_TOP_LEVEL ON)
endif()

option(JUCE_SPECTROSCOPE_VALIDATE_SHADERS "Validate shaders with an installed glslangValidator" OFF)
option(JUCE_SPECTROSCOPE_BUILD_TESTS "Build the headless analyzer tests" ${JUCE_SPECTROSCOPE_IS_TOP_LEVEL})
option(JUCE_SPECTROSCOPE_BUILD_DEMO "Build the standalone microphone-input demo" ${JUCE_SPECTROSCOPE_IS_TOP_LEVEL})
option(JUCE_SPECTROSCOPE_BUILD_GUI_TESTS
	"Register tests that require an interactive desktop and working OpenGL driver" OFF)
option(JUCE_SPECTROSCOPE_FETCH_JUCE "Fetch the pinned JUCE dependency for standalone builds" ${JUCE_SPECTROSCOPE_IS_TOP_LEVEL})

# Keep standalone MSVC builds runnable without the Visual C++ debug runtime DLLs.
# When embedded, the parent project remains responsible for choosing its runtime.
if(MSVC AND JUCE_SPECTROSCOPE_IS_TOP_LEVEL)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT TARGET juce-static)
	if(NOT TARGET juce::juce_core AND JUCE_SPECTROSCOPE_FETCH_JUCE)
		include(FetchContent)
		set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
		set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
		FetchContent_Declare(
			JUCE
			URL https://github.com/juce-framework/JUCE/archive/f8f8864172464b9adf9eba6101e1f784838d1597.tar.gz
			URL_HASH SHA256=0bef95e4d4e6e8e1aaa98dfa9aae7529235152e140a7710525bc38b03f8c93a9
			DOWNLOAD_EXTRACT_TIMESTAMP TRUE
		)
		FetchContent_MakeAvailable(JUCE)
	endif()

	if(NOT TARGET juce::juce_core)
		message(FATAL_ERROR
			"juce-spectroscope19 needs either a parent-provided juce-static target, "
			"JUCE CMake targets, or JUCE_SPECTROSCOPE_FETCH_JUCE=ON")
	endif()

	# Match the umbrella target supplied by JammerNetz. Keeping this target local
	# means consumers never download or compile a second JUCE checkout.
	add_library(juce-static STATIC)
	target_link_libraries(juce-static
		PRIVATE
			juce::juce_audio_basics
			juce::juce_audio_devices
			juce::juce_audio_utils
			juce::juce_core
			juce::juce_data_structures
			juce::juce_dsp
			juce::juce_events
			juce::juce_graphics
			juce::juce_gui_basics
			juce::juce_opengl
		PUBLIC
			juce::juce_recommended_config_flags
			juce::juce_recommended_warning_flags
	)
	target_compile_definitions(juce-static
		PUBLIC
			DONT_SET_USING_JUCE_NAMESPACE=1
			JUCE_CATCH_UNHANDLED_EXCEPTIONS=0
			JUCE_CHECK_MEMORY_LEAKS=0
			JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
			JUCE_JACK=0
			JUCE_REPORT_APP_USAGE=0
			JUCE_USE_CURL=0
			JUCE_WEB_BROWSER=0
	)
	target_include_directories(juce-static
		INTERFACE $<TARGET_PROPERTY:juce-static,INCLUDE_DIRECTORIES>
	)
	set_target_properties(juce-static PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

set(SHADER_FILES
	"${CMAKE_CURRENT_LIST_DIR}/shaders/oscilloscope.frag.glsl"
	"${CMAKE_CURRENT_LIST_DIR}/shaders/oscilloscope.vert.glsl"
)

set(GENERATED_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_RESOURCES "${GENERATED_DIRECTORY}/BinaryResources.h")
file(MAKE_DIRECTORY "${GENERATED_DIRECTORY}")

add_custom_command(
	OUTPUT "${GENERATED_RESOURCES}"
	COMMAND "${CMAKE_COMMAND}"
		"-DOUTPUT_FILE=${GENERATED_RESOURCES}"
		"-DINPUT_FILES=${SHADER_FILES}"
		-P "${CMAKE_CURRENT_LIST_DIR}/createResources.cmake"
	DEPENDS ${SHADER_FILES} "${CMAKE_CURRENT_LIST_DIR}/createResources.cmake"
	COMMENT "Embedding juce-spectroscope19 shaders"
	VERBATIM
)

add_library(juce-spectroscope-analysis STATIC
	FrequencyAxis.h
	PitchTracker.cpp
	PitchTracker.h
	Spectrogram.cpp
	Spectrogram.h
	TrackedPitch.h
)
target_include_directories(juce-spectroscope-analysis PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(juce-spectroscope-analysis PUBLIC juce-static)
target_compile_features(juce-spectroscope-analysis PUBLIC cxx_std_17)

add_library(juce-spectroscope-ui STATIC
	"${GENERATED_RESOURCES}"
	OpenGLFloatTexture.cpp
	OpenGLFloatTexture.h
	OpenGLHelpers.cpp
	OpenGLHelpers.h
	ShaderBasedComponent.cpp
	ShaderBasedComponent.h
	SpectrogramWidget.cpp
	SpectrogramWidget.h
	WaterfallTimeline.h
	${SHADER_FILES}
)
target_include_directories(juce-spectroscope-ui
	PUBLIC "${CMAKE_CURRENT_LIST_DIR}"
	PRIVATE "${GENERATED_DIRECTORY}"
)
target_link_libraries(juce-spectroscope-ui PUBLIC juce-spectroscope-analysis juce-static)
target_compile_features(juce-spectroscope-ui PUBLIC cxx_std_17)

# Compatibility target for existing parent projects.
add_library(juce-spectroscope19 INTERFACE)
target_link_libraries(juce-spectroscope19 INTERFACE juce-spectroscope-analysis juce-spectroscope-ui)

if(JUCE_SPECTROSCOPE_VALIDATE_SHADERS)
	find_program(GLSLANG_VALIDATOR NAMES glslangValidator REQUIRED)
	add_custom_target(juce-spectroscope-validate-shaders)
	foreach(SHADER IN LISTS SHADER_FILES)
		add_custom_command(TARGET juce-spectroscope-validate-shaders
			COMMAND "${GLSLANG_VALIDATOR}" "${SHADER}"
			COMMENT "Validating ${SHADER}"
			VERBATIM
		)
	endforeach()
	add_dependencies(juce-spectroscope-ui juce-spectroscope-validate-shaders)
endif()

if(JUCE_SPECTROSCOPE_BUILD_TESTS)
	add_executable(juce-spectroscope-analysis-tests tests/SpectrogramTests.cpp)
	target_link_libraries(juce-spectroscope-analysis-tests PRIVATE juce-spectroscope-analysis)
	target_compile_features(juce-spectroscope-analysis-tests PRIVATE cxx_std_17)
	add_test(NAME juce-spectroscope-analysis-tests COMMAND juce-spectroscope-analysis-tests)
endif()

if(JUCE_SPECTROSCOPE_BUILD_DEMO)
	if(NOT COMMAND juce_add_gui_app)
		message(FATAL_ERROR "The standalone demo requires JUCE's juce_add_gui_app CMake function")
	endif()
	add_subdirectory(examples/standalone)
endif()

foreach(TARGET_NAME IN ITEMS juce-spectroscope-analysis juce-spectroscope-ui)
	if(MSVC)
		target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
	endif()
endforeach()
