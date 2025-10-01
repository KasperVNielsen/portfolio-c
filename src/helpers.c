#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>

unsigned int createRectangle(float x, float y, float w, float h) {
    // clamp to avoid degenerate geometry
    if (w <= 0.0f) w = 1e-6f;
    if (h <= 0.0f) h = 1e-6f;

    const float z = -0.5f;
    float l = x, r = x + w;
    float t = y, b = y - h;

    float verts[] = {
        l, t, z,   // 0 top-left
        r, t, z,   // 1 top-right
        r, b, z,   // 2 bottom-right
        l, b, z    // 3 bottom-left
    };
    unsigned int idx[] = { 0,1,2,  2,3,0 };

    unsigned int VAO=0, VBO=0, EBO=0;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    glCheckErrorDbg("createRectangle");
    return VAO;
}

unsigned int createTriangle(float x1,float y1,float x2,float y2,float x3,float y3, float z) {
    float vertices[] = { x1,y1,z,  x2,y2,z,  x3,y3,z };

    unsigned int VAO=0, VBO=0;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    glCheckErrorDbg("createTriangle");
    return VAO;
}

unsigned int createLineVAO(int components, int capacity_floats, unsigned int* outVBO) {
    if (components < 2) components = 2;
    if (components > 3) components = 3;
    if (capacity_floats < components*2) capacity_floats = components*2;

    unsigned int VAO=0, VBO=0;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, (size_t)capacity_floats * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, components, GL_FLOAT, GL_FALSE, components*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    if (outVBO) *outVBO = VBO;

    glCheckErrorDbg("createLineVAO");
    return VAO;
}

void updateLineVBO(unsigned int vbo, const float* data, int data_count_floats) {
    if (!data || data_count_floats <= 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)data_count_floats * sizeof(float), data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glCheckErrorDbg("updateLineVBO");
}

unsigned int buildShader(const char* vsrc, const char* fsrc) {
    GLint ok = 0;
    char log[1024];

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrc, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(vs, sizeof(log), NULL, log); fprintf(stderr,"VS error: %s\n", log); }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrc, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(fs, sizeof(log), NULL, log); fprintf(stderr,"FS error: %s\n", log); }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, sizeof(log), NULL, log); fprintf(stderr,"Link error: %s\n", log); }

    glDeleteShader(vs);
    glDeleteShader(fs);

    glCheckErrorDbg("buildShader");
    return prog;
}

void glCheckErrorDbg(const char* where) {
#ifndef NDEBUG
    for (GLenum err; (err = glGetError()) != GL_NO_ERROR; ) {
        const char* msg = "UNKNOWN";
        switch (err) {
            case GL_INVALID_ENUM: msg = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: msg = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: msg = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: msg = "GL_OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: msg = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        fprintf(stderr, "[GL] %s: %s\n", where ? where : "(unknown)", msg);
    }
#else
    (void)where;
#endif
}
