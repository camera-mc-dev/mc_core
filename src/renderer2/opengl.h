#ifndef ME_OPENGL_H
#define ME_OPENGL_H

#ifdef __APPLE__
    #include <OpenGL/gl3.h>
#else
    #include <GL/glew.h>
    #include <SFML/OpenGL.hpp>
#endif
#include <SFML/Window.hpp>

#endif
