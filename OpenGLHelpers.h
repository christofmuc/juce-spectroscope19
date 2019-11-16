/*
* Useful code normally only available inside a JUCE module, but as we need this outside, we had to extract it 
*/

#pragma once

#include "JuceHeader.h"

void checkGLError(const char* file, const int line);
int getAllowedTextureSize(int x);

#if JUCE_DEBUG && ! defined (JUCE_CHECK_OPENGL_ERROR)
#define JUCE_CHECK_OPENGL_ERROR checkGLError (__FILE__, __LINE__);
#else
#define JUCE_CHECK_OPENGL_ERROR ;
#endif
