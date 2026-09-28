#pragma once
#if defined(WIN32) || defined(LINUX) // [kirikinux2] Linux desktop uses GLEW like win32
#if defined(_M_X64)
#define GLEW_STATIC
#endif
#include "GL/glew.h"
#ifndef EGLAPIENTRY // [kirikinux2] named by a typedef in RenderManager_ogl.cpp
#define EGLAPIENTRY
#endif
#else
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifdef __APPLE__
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#endif
#endif

bool TVPCheckGLExtension(const std::string &extname);
