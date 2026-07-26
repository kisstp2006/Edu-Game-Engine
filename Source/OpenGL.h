#ifndef __OPENGL_H__
#define __OPENGL_H__

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>
#include "GL/glew.h" // extension lib
#include "SDL_opengl.h"
#include <gl/GL.h>
#include <gl/GLU.h>

// Windows.h #defines CreateDirectory to CreateDirectoryA/W, which clashes with
// ModuleFileSystem::CreateDirectory() call sites in files included after this header
#ifdef CreateDirectory
#undef CreateDirectory
#endif

#endif // __OPENGL_H__