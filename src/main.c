#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font/stb_easy_font.h"

#include "helpers.h"

// ---------------- Globals ----------------
int windowWidth = 800;
int windowHeight = 600;

bool searchBarActive = false;
char searchText[256] = {0};
int  searchLen = 0;

unsigned int rectShader, textShader;
unsigned int searchBarVAO, chartVAO;
unsigned int textVAO, textVBO;

// Simple blink state for caret
double blinkLast = 0.0;
bool   blinkOn   = true;

// ---------------- Shaders ----------------
// Rectangles: positions are vec3 in NDC (we create them already in NDC)
static const char* rectVS = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"void main(){ gl_Position = vec4(aPos, 1.0); }\n";

static const char* rectFS = "#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec3 uColor;\n"
"void main(){ FragColor = vec4(uColor, 1.0); }\n";

// Text: positions are in pixels; shader converts to NDC
static const char* textVS = "#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2 uResolution;\n"
"void main(){\n"
"  vec2 pos = aPos / uResolution * 2.0 - 1.0;\n"
"  pos.y = -pos.y; // flip Y so +Y is down in pixel space\n"
"  gl_Position = vec4(pos, 0.0, 1.0);\n"
"}\n";

static const char* textFS = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main(){ FragColor = vec4(0.0, 0.0, 0.0, 1.0); }\n";

// --------------- Forwards ----------------
static void glfwErrorCallback(int code, const char* desc);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void char_callback(GLFWwindow* window, unsigned int codepoint);

unsigned int buildShader(const char* vsrc, const char* fsrc);
void initTextRendering(void);
void renderText(float x, float y, const char* text);
float approxTextWidth(const char* text);

// ---------------- Main -------------------
int main(void) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to init GLFW\n");
        return -1;
    }
    glfwSetErrorCallback(glfwErrorCallback);

    // Request OpenGL 3.3 Core
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);        // <- the important fix
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Search Bar with Text", NULL, NULL);
    if (!window) {
        // Fallback tries if needed
        fprintf(stderr, "Primary context failed; retrying without profile hint...\n");
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        window = glfwCreateWindow(windowWidth, windowHeight, "Search Bar with Text", NULL, NULL);
    }
    if (!window) {
        fprintf(stderr, "Retrying with OpenGL 3.0...\n");
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(windowWidth, windowHeight, "Search Bar with Text", NULL, NULL);
    }
    if (!window) {
        fprintf(stderr, "Failed to create window (see errors above)\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCharCallback(window, char_callback);
    // Optional: if you have key_callback in helpers.c and want ESC handling, etc.
    // glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGL()) {
        fprintf(stderr, "Failed to init GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    rectShader = buildShader(rectVS, rectFS);
    textShader = buildShader(textVS, textFS);

    // Build rectangles in NDC:
    // Search bar (top center): x=-0.7 .. +0.7, y=0.95 (top) down to 0.83 (bottom)
    searchBarVAO = createRectangle(-0.7f, 0.95f, 1.4f, 0.12f);
    // Chart (center): x=-0.8 .. +0.8, y=+0.6 down to -0.6
    chartVAO    = createRectangle(-0.8f, 0.6f, 1.6f, 1.2f);

    initTextRendering();

    blinkLast = glfwGetTime();

    // ---------- Render loop ----------
    while (!glfwWindowShouldClose(window)) {
        glClearColor(1.0f,1.0f,1.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw chart (light grey)
        glUseProgram(rectShader);
        glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.85f, 0.85f, 0.85f);
        glBindVertexArray(chartVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Draw search bar (darker grey)
        glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.65f, 0.65f, 0.65f);
        glBindVertexArray(searchBarVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Draw text last (on top), in pixel coordinates
        if (searchBarActive || searchLen > 0) {
            glUseProgram(textShader);
            glUniform2f(glGetUniformLocation(textShader, "uResolution"), (float)windowWidth, (float)windowHeight);

            // Position inside the bar in pixels: a bit from the left, near the top
            float textX = 60.0f;
            float textY = 40.0f;           // 40px from top (remember shader flips Y)

            // Current string
            renderText(textX, textY, (searchLen > 0) ? searchText : "Type to search...");

            // Blinking caret
            double now = glfwGetTime();
            if (now - blinkLast > 0.5) { blinkOn = !blinkOn; blinkLast = now; }
            if (blinkOn) {
                float caretX = textX + approxTextWidth((searchLen > 0) ? searchText : "");
                renderText(caretX, textY, "|");
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// ------------- Callbacks --------------
static void glfwErrorCallback(int code, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0,0,width,height);
    windowWidth  = width;
    windowHeight = height;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xp, yp;
        glfwGetCursorPos(window, &xp, &yp);

        // Convert mouse to NDC
        float ndcX = (float)((2.0 * xp) / windowWidth  - 1.0);
        float ndcY = (float)(1.0 - (2.0 * yp) / windowHeight);

        // Must match the createRectangle for search bar
        const float sbLeft   = -0.7f;
        const float sbRight  =  0.7f;
        const float sbTop    =  0.95f;
        const float sbBottom =  0.83f;

        if (ndcX >= sbLeft && ndcX <= sbRight && ndcY <= sbTop && ndcY >= sbBottom) {
            searchBarActive = true;
            // Don’t wipe existing text; keep whatever user typed
            printf("Search bar activated\n");
        } else {
            searchBarActive = false;
        }
    }
}

void char_callback(GLFWwindow* window, unsigned int codepoint) {
    if (!searchBarActive) return;

    // Backspace (GLFW may deliver it via key callback; some platforms send 8 here too)
    if (codepoint == 8) {
        if (searchLen > 0) {
            searchText[--searchLen] = '\0';
            printf("Backspace: %s\n", searchText);
        }
        return;
    }
    // Filter printable ASCII
    if (codepoint >= 32 && codepoint < 127) {
        if (searchLen < (int)sizeof(searchText)-1) {
            searchText[searchLen++] = (char)codepoint;
            searchText[searchLen] = '\0';
            printf("Typed: %s\n", searchText);
        }
    }
}

// ------------- Text Rendering --------------
void initTextRendering(void) {
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    // 2 floats per vertex (x,y) in pixels
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// Convert stb_easy_font quads to triangles and draw
void renderText(float x, float y, const char* text) {
    char buffer[100000];
    int num_quads = stb_easy_font_print((int)x, (int)y, (char*)text, NULL, buffer, sizeof(buffer));
    if (num_quads <= 0) return;

    // Expand quads (4 verts) to triangles (6 verts)
    float* src = (float*)buffer;               // per quad: 4 vertices, each (x,y,s,t) => 16 floats
    int triFloats = num_quads * 6 * 2;         // 6 verts * 2 floats each
    float* tri = (float*)malloc(triFloats * sizeof(float));
    if (!tri) return;

    int t = 0;
    for (int i = 0; i < num_quads; ++i) {
        float* q = src + i*16; // x0,y0, s0,t0, x1,y1, s1,t1, x2,y2, s2,t2, x3,y3, s3,t3
        float x0=q[0], y0=q[1];
        float x1=q[4], y1=q[5];
        float x2=q[8], y2=q[9];
        float x3=q[12],y3=q[13];

        // tri 1: 0-1-2
        tri[t++] = x0; tri[t++] = y0;
        tri[t++] = x1; tri[t++] = y1;
        tri[t++] = x2; tri[t++] = y2;
        // tri 2: 0-2-3
        tri[t++] = x0; tri[t++] = y0;
        tri[t++] = x2; tri[t++] = y2;
        tri[t++] = x3; tri[t++] = y3;
    }

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, triFloats * sizeof(float), tri, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, triFloats / 2);

    free(tri);
}

// crude monospace width estimate (good enough for caret placement)
float approxTextWidth(const char* text) {
    // stb_easy_font uses ~7-8 px width per glyph; 8 is a safe round number
    return (float)strlen(text) * 8.0f;
}

// ------------- Shader helper --------------
unsigned int buildShader(const char* vsrc, const char* fsrc) {
    int ok;
    char log[1024];

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrc, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(vs, sizeof(log), NULL, log); fprintf(stderr,"VS error: %s\n", log); }

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrc, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(fs, sizeof(log), NULL, log); fprintf(stderr,"FS error: %s\n", log); }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, sizeof(log), NULL, log); fprintf(stderr,"Link error: %s\n", log); }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}
