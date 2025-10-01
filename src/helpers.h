#ifndef HELPERS_H
#define HELPERS_H

#include <glad/glad.h>

// Create an axis-aligned rectangle in NDC at z = -0.5
// Top-left corner (x, y), width w (>0), height h (>0).
// Returns a VAO with an ELEMENT_ARRAY_BUFFER bound (6 indices).
// Usage: glBindVertexArray(vao); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
unsigned int createRectangle(float x, float y, float w, float h);

// Create a single triangle in NDC at the given z.
// Returns a VAO with no EBO (use glDrawArrays(GL_TRIANGLES, 0, 3)).
unsigned int createTriangle(float x1,float y1,float x2,float y2,float x3,float y3, float z);

// Creates a VAO+VBO for GL_LINES with 'components' per vertex (2 for xy, 3 for xyz).
// Returns the VAO and writes the VBO id to *outVBO. The buffer has initial capacity
// of 'capacity_floats' floats and is GL_DYNAMIC_DRAW.
unsigned int createLineVAO(int components, int capacity_floats, unsigned int* outVBO);

// Replace data in a dynamic line VBO created by createLineVAO.
// 'data_count_floats' is the number of floats in 'data'.
void updateLineVBO(unsigned int vbo, const float* data, int data_count_floats);

// Simple shader builder (vertex+fragment). Returns program id or 0 on failure.
unsigned int buildShader(const char* vsrc, const char* fsrc);

// Convenience: check for GL errors in debug builds.
void glCheckErrorDbg(const char* where);

#endif // HELPERS_H
