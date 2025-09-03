#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Vertex Shader source
const char* vertexShaderSource =
"#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos, 0.0, 1.0);\n"
"}\0";

// Fragment Shader source
const char* fragmentShaderSource =
"#version 330 core\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"   FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
"}\0";

int main(void) {

    
    // Init GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    // OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Candlestick Chart", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGL()) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }

    //vertex shader

      // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Clear the screen with a white color
        glClearColor(1.0, 1.0, 1.0, 1.0);        
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_BLEND);

        
        // Swap buffers and poll input events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }



    glfwTerminate();
    return 0;
}


