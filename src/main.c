#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "helpers.h"
#include <stdbool.h>
#include "stb_easy_font/stb_easy_font.h"

#define STB_EASY_FONT_IMPLEMENTATION

int windowWidth = 800;
int windowHeight = 600;

bool searchBarActive = false;
char searchText[256];
int searchLen = 0;


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
"   FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
"}\0";

// Forward declare callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);


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

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Candlestick Chart", NULL, NULL);
        if (!window) {
            fprintf(stderr, "Failed to create window\n");
            glfwTerminate();
            return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCharCallback(window, char_callback);

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

    //Clean up shaders (no longer needed after linking)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    //Search bar: top-center
    float sbW = 1.40f; 
    float sbH = 0.12f; 
    float sbX = -sbW * 0.5f; 
    float sbY = 0.95f; 
    unsigned int searchBarVAO = createRectangle(sbX, sbY, sbW, sbH);

    // Chart area: centered
    float chW = 1.60f;   
    float chH = 1.20f;   
    float chX = -chW * 0.5f; 
    float chY =  chH * 0.5f; 
    unsigned int chartVAO = createRectangle(chX, chY, chW, chH);

    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Clear the screen with a white color
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // search bar (top)
        glBindVertexArray(searchBarVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // chart area (middle)
        glBindVertexArray(chartVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Swap buffers and poll input events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        float ndcX = (2.0f * xpos) / windowWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * ypos) / windowHeight;

        float sbLeft   = -0.7f;
        float sbRight  =  0.7f;
        float sbTop    =  0.95f;
        float sbBottom =  0.83f;

        if (ndcX >= sbLeft && ndcX <= sbRight &&
            ndcY <= sbTop && ndcY >= sbBottom) {
            searchBarActive = true;
            searchLen = 0;
            searchText[0] = '\0';
            printf("Search bar activated!\n");
        } else {
            searchBarActive = false;
        }

    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
}

void char_callback(GLFWwindow* window, unsigned int codepoint) {
    if (searchBarActive) {
        if (searchLen < 255) {
            searchText[searchLen++] = (char)codepoint;
            searchText[searchLen] = '\0';
            printf("Search text: %s\n", searchText);
        }
    }
}

