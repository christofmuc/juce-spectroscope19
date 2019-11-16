/*
* Useful code normally only available inside a JUCE module, but as we need this outside, we had to extract it
*/


#include "OpenGLHelpers.h"

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

#if JUCE_MAC || JUCE_IOS

#ifndef JUCE_IOS_MAC_VIEW
#if JUCE_IOS
#define JUCE_IOS_MAC_VIEW    UIView
#define JUCE_IOS_MAC_WINDOW  UIWindow
#else
#define JUCE_IOS_MAC_VIEW    NSView
#define JUCE_IOS_MAC_WINDOW  NSWindow
#endif
#endif

#endif

bool checkPeerIsValid(OpenGLContext* context)
{
	jassert(context != nullptr);

	if (context != nullptr)
	{
		if (auto* comp = context->getTargetComponent())
		{
			if (auto* peer = comp->getPeer())
			{
#if JUCE_MAC || JUCE_IOS
				if (auto* nsView = (JUCE_IOS_MAC_VIEW*)peer->getNativeHandle())
				{
					if (auto nsWindow = [nsView window])
					{
#if JUCE_MAC
						return ([nsWindow isVisible]
							&& (![nsWindow hidesOnDeactivate] || [NSApp isActive]));
#else
						ignoreUnused(nsWindow);
						return true;
#endif
					}
				}
#else
				ignoreUnused(peer);
				return true;
#endif
			}
		}
	}

	return false;
}

void checkGLError(const char* file, const int line)
{
	ignoreUnused(file, line); // Release build will otherwise issue warning
	for (;;)
	{
		const GLenum e = glGetError();

		if (e == GL_NO_ERROR)
			break;

		// if the peer is not valid then ignore errors
		if (!checkPeerIsValid(OpenGLContext::getCurrentContext()))
			continue;

		DBG("***** " << getGLErrorMessage(e) << "  at " << file << " : " << line);
		jassertfalse;
	}
}

void clearGLError() noexcept
{
	while (glGetError() != GL_NO_ERROR) {}
}

int getAllowedTextureSize(int x)
{
#if JUCE_OPENGL_ALLOW_NON_POWER_OF_TWO_TEXTURES
	return x;
#else
	return nextPowerOfTwo(x);
#endif
}
