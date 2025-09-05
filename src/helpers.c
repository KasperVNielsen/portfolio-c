#include <glad/glad.h>
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Creates a rectangle VAO at (x, y) with width w and height h
unsigned int createRectangle(float x, float y, float w, float h) {
    float vertices[] = {
        x,       y - h,   // bottom-left
        x + w,   y - h,   // bottom-right
        x + w,   y,       // top-right
        x,       y        // top-left
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Vertex data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Attribute pointer
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbind VAO
    glBindVertexArray(0);

    return VAO;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
        // maybe submit login or search
    }
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        // buy
    }
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        // sell
    }
}
