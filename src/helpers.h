#ifndef HELPERS_H
#define HELPERS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Prototypes
unsigned int createRectangle(float x, float y, float width, float height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

#endif
