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

# Load the macro to process binary resources
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
include(BinaryResources)

# Create our binary resources - the shaders
create_resources(shaders BinaryResources.h)

# Define the sources for the static library
set(Sources
	Fifo.h
	ManagedOpenGLComponent.h
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
	shaders/oscilloscope.frag.glsl
	shaders/oscilloscope.vert.glsl
)

# Setup library
add_library(juce-spectroscope19 ${Sources})
target_include_directories(juce-spectroscope19 INTERFACE ${CMAKE_CURRENT_LIST_DIR} PUBLIC ${GLEW_DIRECTORY} "${asiosdk_SOURCE_DIR}/common")
target_link_libraries(juce-spectroscope19 PRIVATE ${JUCE_LIBRARIES})

# Pedantic about warnings
IF(MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(juce-spectroscope19 PRIVATE /W4 /WX)
ELSEIF(UNIX)
    # lots of warnings and all warnings as errors
    #target_compile_options(juce-spectroscope19 PRIVATE -Wall -Wextra -pedantic -Werror)
    target_compile_options(juce-spectroscope19 PRIVATE -pthread -I/usr/include/webkitgtk-4.0 -I/usr/include/gtk-3.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/gtk-3.0 -I/usr/include/gio-unix-2.0/ -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/harfbuzz -I/usr/include/pango-1.0 -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/libpng16 -I/usr/include/libsoup-2.4 -I/usr/include/libxml2 -I/usr/include/webkitgtk-4.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include  -DDEBUG -DJUCE_DEBUG)
ENDIF()
