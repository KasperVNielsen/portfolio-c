#ifndef HELPERS_H
#define HELPERS_H

// Build a rectangle VAO at NDC top-left (x, y) with width and height.
// Uses vec3 vertices and z = -0.5f so it draws behind text (z = 0).
unsigned int createRectangle(float x, float y, float width, float height);

#endif // HELPERS_H
