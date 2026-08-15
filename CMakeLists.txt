#
#  Copyright (c) 2019 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

cmake_minimum_required(VERSION 3.14)

project(juce-spectroscope19 LANGUAGES CXX)

option(JUCE_SPECTROSCOPE_VALIDATE_SHADERS "Validate shaders with an installed glslangValidator" OFF)
option(JUCE_SPECTROSCOPE_BUILD_TESTS "Build the headless analyzer tests" OFF)

if(NOT TARGET juce-static)
	message(FATAL_ERROR "juce-spectroscope19 expects the parent project to provide the juce-static target")
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
	Spectrogram.cpp
	Spectrogram.h
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

foreach(TARGET_NAME IN ITEMS juce-spectroscope-analysis juce-spectroscope-ui)
	if(MSVC)
		target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
	endif()
endforeach()
