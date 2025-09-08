#ifndef HELPERS_H
#define HELPERS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Prototypes
unsigned int createRectangle(float x, float y, float width, float height);


// Input callbacks
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void char_callback(GLFWwindow* window, unsigned int codepoint);

#endif
