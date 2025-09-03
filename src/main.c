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
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader,1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    //check for compiler errors
    int sucess;
    char infolog[32];

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &sucess);
    if(!sucess){
        glGetShaderInfoLog(vertexShader, 512, NULL, infolog);
        fprintf(stderr, "vertex compiler error: %s\n",infolog);
    }

    //compile fragment shaders
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1 , &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    //check for compiler errors
    if(!sucess){
        glGetShaderInfoLog(fragmentShader, 512, NULL, infolog);
        fprintf(stderr, "fragment compiler error; %s\n", infolog);
    }

    //link shaders into program
    unsigned int shaderProgram = glCreateProgram();
    glad_glAttachShader(shaderProgram, vertexShader);
    glad_glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    //check for linking error
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &sucess);
    if(!sucess){
        glGetProgramInfoLog(shaderProgram, 512, NULL, infolog);
        fprintf(stderr, "shader linking error; %s\n", infolog);
    }

    // 4. Clean up shaders (no longer needed after linking)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
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


