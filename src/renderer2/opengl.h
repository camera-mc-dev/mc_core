#ifndef ME_OPENGL_H
#define ME_OPENGL_H

#if defined __APPLE__
    #include <OpenGL/gl3.h>
#elif defined _WIN32
    #define WGL_WGLEXT_PROTOTYPES
    #include "windows.h"

    // **** THE FIX FOR WINDOWS MACRO COLLISIONS ****
    #ifdef max
    #undef max
    #endif

    #ifdef min
    #undef min
    #endif

    #ifdef near
    #undef near
    #endif

    #ifdef far
    #undef far
    #endif
    // **********************************************
    #include <GL/glew.h>
    #include "GL/wglext.h"

    #include <SFML/OpenGL.hpp>
#else
    #include <GL/glew.h>
    #include <SFML/OpenGL.hpp>

#endif

#include <SFML/Window.hpp>



#endif
