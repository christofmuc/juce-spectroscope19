#
#  Copyright (c) 2019 Christof Ruch. All rights reserved.
#
#  Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
#

project(juce-spectroscope19)

# Find JUCE, and add the components we need
SET(JUCE_DIR_HINT "${CMAKE_CURRENT_LIST_DIR}/../third_party")
find_package(JUCE REQUIRED 
	COMPONENTS 
		juce_core		
		juce_events
		juce_audio_basics
		juce_audio_devices
		juce_data_structures
		juce_opengl
		juce_dsp
)

# Define the sources for the static library
set(Sources
	Fifo.h
	OpenGLFloatTexture.cpp
	OpenGLFloatTexture.h
	OpenGLHelpers.cpp
	OpenGLHelpers.h
	ShaderBasedComponent.cpp
	ShaderBasedComponent.h
	Spectrogram.cpp
	Spectrogram.h
	SpectrogramWidget.cpp
	SpectrogramWidget.h
	oscilloscope.frag.glsl
	oscilloscope.vert.glsl
)

# Setup library
add_library(juce-spectroscope19 ${Sources})
target_include_directories(juce-spectroscope19 INTERFACE ${CMAKE_CURRENT_LIST_DIR} PUBLIC ${GLEW_DIRECTORY} "${asiosdk_SOURCE_DIR}/common")
target_link_libraries(juce-spectroscope19 PRIVATE ${JUCE_LIBRARIES})

# Pedantic about warnings
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(juce-spectroscope19 PRIVATE /W4 /WX)
else()
    # lots of warnings and all warnings as errors
    target_compile_options(juce-spectroscope19 PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
