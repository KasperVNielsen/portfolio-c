#include <glad/glad.h>
#include "helpers.h"

// Creates a rectangle VAO at (x, y) with width w and height h in NDC.
// (x, y) is TOP-LEFT corner. z = -0.5f so rectangles render behind text.
unsigned int createRectangle(float x, float y, float w, float h) {
    const float z = -0.5f;

    // Vertex order: top-left, top-right, bottom-right, bottom-left
    float vertices[] = {
        x,     y,      z,  // 0 top-left
        x+w,   y,      z,  // 1 top-right
        x+w,   y-h,    z,  // 2 bottom-right
        x,     y-h,    z   // 3 bottom-left
    };

    unsigned int indices[] = { 0, 1, 2,  0, 2, 3 };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // aPos = vec3 in your rect vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return VAO;
}
