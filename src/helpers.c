#include <glad/glad.h>
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glad/glad.h>
#include "helpers.h"
#include <GLFW/glfw3.h>

// Creates a rectangle VAO at (x, y) with width w and height h
// (x,y) = top-left corner in NDC, z = -0.5f to ensure behind text
unsigned int createRectangle(float x, float y, float w, float h) {
    float z = -0.5f;
    float vertices[] = {
        x,     y - h, z,   // bottom-left
        x + w, y - h, z,   // bottom-right
        x + w, y,     z,   // top-right
        x,     y,     z    // top-left
    };

    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // vertices
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // attribute pointer: 3 floats per vertex
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

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
