/*
* Useful code normally only available inside a JUCE module, but as we need this outside, we had to extract it
*/


#include "OpenGLHelpers.h"

using namespace juce::gl;

namespace {
const char* getGLErrorMessage(const GLenum e) noexcept
{
	switch (e)
	{
	case GL_INVALID_ENUM:                   return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE:                  return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION:              return "GL_INVALID_OPERATION";
	case GL_OUT_OF_MEMORY:                  return "GL_OUT_OF_MEMORY";
#ifdef GL_STACK_OVERFLOW
	case GL_STACK_OVERFLOW:                 return "GL_STACK_OVERFLOW";
#endif
#ifdef GL_STACK_UNDERFLOW
	case GL_STACK_UNDERFLOW:                return "GL_STACK_UNDERFLOW";
#endif
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
	case GL_INVALID_FRAMEBUFFER_OPERATION:  return "GL_INVALID_FRAMEBUFFER_OPERATION";
#endif
	default: break;
	}

	return "Unknown error";
}

bool checkPeerIsValid(juce::OpenGLContext* context)
{
	jassert(context != nullptr);

	if (context != nullptr)
	{
		if (auto* comp = context->getTargetComponent())
		{
			if (auto* peer = comp->getPeer())
			{
				ignoreUnused(peer);
				return true;
			}
		}
	}

	return false;
}
}

void checkGLError(const char* file, const int line)
{
	juce::ignoreUnused(file, line); // Release build will otherwise issue warning
	for (;;)
	{
		const GLenum e = glGetError();

		if (e == GL_NO_ERROR)
			break;

		// if the peer is not valid then ignore errors
		if (!checkPeerIsValid(juce::OpenGLContext::getCurrentContext()))
			continue;

		auto errorMsg = getGLErrorMessage(e);
		DBG("***** " << errorMsg << "  at " << file << " : " << line);
		juce::ignoreUnused(errorMsg);
		jassertfalse;
	}
}

int getAllowedTextureSize(int x)
{
#if JUCE_OPENGL_ALLOW_NON_POWER_OF_TWO_TEXTURES
	return x;
#else
	return juce::nextPowerOfTwo(x);
#endif
}
