juce_add_gui_app(JuceSpectroscopeDemo
	PRODUCT_NAME "JUCE Spectroscope Demo"
	COMPANY_NAME "Christof Ruch"
	BUNDLE_ID "de.christofruch.juce-spectroscope-demo"
	MICROPHONE_PERMISSION_ENABLED TRUE
	MICROPHONE_PERMISSION_TEXT "The spectroscope displays the spectrum of the selected audio input."
)

target_sources(JuceSpectroscopeDemo PRIVATE
	Main.cpp
	MainComponent.cpp
	MainComponent.h
)

target_link_libraries(JuceSpectroscopeDemo PRIVATE juce-spectroscope19)
target_compile_features(JuceSpectroscopeDemo PRIVATE cxx_std_17)

if(MSVC)
	target_compile_options(JuceSpectroscopeDemo PRIVATE /W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	target_compile_options(JuceSpectroscopeDemo PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
