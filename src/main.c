// main.c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <time.h>   
#include <math.h>   

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font/stb_easy_font.h"

#include "helpers.h"

//  Globals 
int windowWidth = 800;
int windowHeight = 600;

// how often to change prices (seconds)
static const double PRICE_UPDATE_DT = 1;
static double lastPriceUpdate = 0.0;

// Tabs
typedef enum { TAB_HOME = 0, TAB_STOCKS = 1 } Tab;
static Tab currentTab = TAB_HOME;

// Search bar state
static bool  searchBarActive = false;
static char  searchText[256] = {0};
static int   searchLen = 0;

// Shaders
static unsigned int rectShader, textShader;

// VAOs
static unsigned int searchBarVAO, chartVAO, navBarVAO;

// Stocks
typedef struct { const char* symbol; float price; int qty; } Stock;
static Stock stocks[3] = {
    {"AAPL", 180.0f, 0},
    {"MSFT", 330.0f, 0},
    {"NVDA", 900.0f, 0}
};
static int selectedStock = 0;
static float cashBalance = 10000.0f;

// Stock tiles + buttons geometry (NDC)
static unsigned int stockVAO[3];
static float stockX = -0.75f, stockW = 0.55f, stockH = 0.14f;
static float stockY[3] = { 0.70f, 0.48f, 0.26f }; // farther apart

static unsigned int buyBtnVAO, sellBtnVAO;
static float buyX = -0.75f, buyY = -0.05f, buyW = 0.28f, buyH = 0.10f;
static float sellX = -0.43f, sellY = -0.05f, sellW = 0.28f, sellH = 0.10f;


// Bottom navbar + icons
static float navX = -1.0f, navY = -0.85f, navW = 2.0f, navH = 0.15f;
static unsigned int homeBodyVAO, homeRoofVAO;
static unsigned int stockBar1VAO, stockBar2VAO, stockBar3VAO;

// Text rendering
static unsigned int textVAO, textVBO;

// Caret blink
static double blinkLast = 0.0;
static bool   blinkOn   = true;

//  Shaders 
static const char* rectVS = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"void main(){ gl_Position = vec4(aPos, 1.0); }\n";

static const char* rectFS = "#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec3 uColor;\n"
"void main(){ FragColor = vec4(uColor, 1.0); }\n";

static const char* textVS = "#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n"
"uniform vec2  uResolution;\n"
"uniform vec2  uOrigin;\n"
"uniform float uScale;\n"
"void main(){\n"
"  vec2 p = uOrigin + (aPos - uOrigin) * uScale;\n"
"  vec2 pos = p / uResolution * 2.0 - 1.0;\n"
"  pos.y = -pos.y;\n"
"  gl_Position = vec4(pos, 0.0, 1.0);\n"
"}\n";

static const char* textFS = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main(){ FragColor = vec4(0.0,0.0,0.0,1.0); }\n";

//  Forwards 
static void glfwErrorCallback(int code, const char *desc);
static void framebuffer_size_callback(GLFWwindow *window, int width, int height);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void key_callback(GLFWwindow *window, int key, int sc, int action, int mods);
static void char_callback(GLFWwindow *window, unsigned int codepoint);
static void updatePricesRandomWalk(void);

static unsigned int buildShader(const char *vsrc, const char *fsrc);
static void initTextRendering(void);
static void renderText(float x, float y, const char *text);
static float measureTextWidthRaw(const char *text);
static unsigned int createTriangle(float x1,float y1,float x2,float y2,float x3,float y3, float z);
static float ndcToPixelX(float ndcX);
static float ndcToPixelY(float ndcY);

//  Main 
int main(void) {
    
    if (!glfwInit()) { fprintf(stderr, "Failed to init GLFW\n"); return -1; }
    glfwSetErrorCallback(glfwErrorCallback);

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Portfolio UI", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Primary context failed; retry 3.3 no profile\n");
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        window = glfwCreateWindow(windowWidth, windowHeight, "Portfolio UI", NULL, NULL);
    }
    if (!window) {
        fprintf(stderr, "Retry 3.0\n");
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(windowWidth, windowHeight, "Portfolio UI", NULL, NULL);
    }
    if (!window) { fprintf(stderr, "Failed to create window\n"); glfwTerminate(); return -1; }

    srand((unsigned)time(NULL));
    lastPriceUpdate = glfwGetTime();
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGL()) { fprintf(stderr, "Failed to init GLAD\n"); glfwDestroyWindow(window); glfwTerminate(); return -1; }

    rectShader = buildShader(rectVS, rectFS);
    textShader = buildShader(textVS, textFS);

    // HOME: search bar + chart
    searchBarVAO = createRectangle(-0.7f, 0.95f, 1.4f, 0.12f);  // top center
    chartVAO     = createRectangle(-0.8f, 0.60f, 1.6f, 1.20f);  // center area

    // STOCKS: tiles + buttons
    for (int i = 0; i < 3; ++i) stockVAO[i] = createRectangle(stockX, stockY[i], stockW, stockH);
    buyBtnVAO  = createRectangle(buyX,  buyY,  buyW,  buyH);
    sellBtnVAO = createRectangle(sellX, sellY, sellW, sellH);

    // NAVBAR
    navBarVAO   = createRectangle(navX, navY, navW, navH);
    homeBodyVAO = createRectangle(-0.78f, -0.90f, 0.12f, 0.07f);
    homeRoofVAO = createTriangle( -0.84f, -0.90f,  -0.72f, -0.98f,  -0.60f, -0.90f, -0.45f);
    stockBar1VAO = createRectangle( 0.62f, -0.90f, 0.05f, 0.05f);
    stockBar2VAO = createRectangle( 0.69f, -0.90f, 0.05f, 0.08f);
    stockBar3VAO = createRectangle( 0.76f, -0.90f, 0.05f, 0.12f);

    initTextRendering();
    blinkLast = glfwGetTime();

    //  Render loop 
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        if (now - lastPriceUpdate >= PRICE_UPDATE_DT) {
            updatePricesRandomWalk();
            lastPriceUpdate = now;
        }

        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(rectShader);

        if (currentTab == TAB_HOME) {
            // Chart
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.85f, 0.85f, 0.85f);
            glBindVertexArray(chartVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Search bar
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.65f, 0.65f, 0.65f);
            glBindVertexArray(searchBarVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Search text
            if (searchBarActive || searchLen > 0) {
                glUseProgram(textShader);
                glUniform2f(glGetUniformLocation(textShader, "uResolution"), (float)windowWidth, (float)windowHeight);

                // center vertically inside the search bar
                const float sbTopNDC = 0.95f, sbBotNDC = 0.83f;
                float sbTopPx = ndcToPixelY(sbTopNDC);
                float sbBotPx = ndcToPixelY(sbBotNDC);
                float sbMidPx = 0.5f*(sbTopPx + sbBotPx);

                float textX = 120.0f;        // left padding in pixels
                float textY = sbMidPx - 4.0f; // slight nudge up
                float scale = 1.8f;          // bigger text

                glUniform1f(glGetUniformLocation(textShader, "uScale"),  scale);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), textX, textY);

                const char* toShow = (searchLen > 0) ? searchText : "Type to search...";
                renderText(textX, textY, toShow);

                // Blinking caret
                double now = glfwGetTime();
                if (now - blinkLast > 0.5) { blinkOn = !blinkOn; blinkLast = now; }
                if (blinkOn) {
                    float rawW = measureTextWidthRaw(toShow); // raw pixel width (unscaled)
                    renderText(textX + rawW, textY, "|");     // shader scales caret, too
                }
            }
        } else { // TAB_STOCKS
            // Stock tiles (selected highlighted)
            for (int i = 0; i < 3; ++i) {
                if (i == selectedStock) glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.75f, 0.75f, 0.90f);
                else                    glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.88f, 0.88f, 0.88f);
                glBindVertexArray(stockVAO[i]);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }

            // BUY / SELL buttons
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.60f, 0.85f, 0.60f);
            glBindVertexArray(buyBtnVAO);  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.90f, 0.60f, 0.60f);
            glBindVertexArray(sellBtnVAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // STOCKS text: labels, prices, qty, buttons, cash
            glUseProgram(textShader);
            glUniform2f(glGetUniformLocation(textShader, "uResolution"), (float)windowWidth, (float)windowHeight);

           // Tiles labels (vertically centered in each tile)
            for (int i = 0; i < 3; ++i)
            {
                float leftPx = ndcToPixelX(stockX) + 12.0f; 
                float topPx = ndcToPixelY(stockY[i]);
                float botPx = ndcToPixelY(stockY[i] - stockH);
                float midPx = 0.5f * (topPx + botPx);

                float px = leftPx;
                float py = midPx - 2.0f; 
                float scale = 1.3f;   

                char line[64];
                snprintf(line, sizeof(line), "%s  $%.2f  x%d",
                         stocks[i].symbol, stocks[i].price, stocks[i].qty);

                glUniform1f(glGetUniformLocation(textShader, "uScale"), scale);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, line);
            }

            // Button labels
            {
                float px = ndcToPixelX(buyX) + 20.0f;
                float py = ndcToPixelY(buyY) - 10.0f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.6f);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, "BUY");

                px = ndcToPixelX(sellX) + 16.0f;
                py = ndcToPixelY(sellY) - 10.0f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.6f);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, "SELL");
            }

            // Cash 
            {
                float cashNdcY = buyY + 0.14f; // above buttons
                float centerNdcX = (buyX + (sellX + sellW)) * 0.5f;

                float px = ndcToPixelX(centerNdcX) - 60.0f; // small centering tweak
                float py = ndcToPixelY(cashNdcY);

                char cash[64];
                snprintf(cash, sizeof(cash), "Cash: $%.2f", cashBalance);
                glUniform1f(glGetUniformLocation(textShader, "uScale"), 1.3f);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, cash);
            }
        }

        //  Navbar (always visible) 
        glUseProgram(rectShader);
        glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.80f, 0.80f, 0.80f);
        glBindVertexArray(navBarVAO);  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Icons (dark)
        glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.30f, 0.30f, 0.30f);
        glBindVertexArray(homeBodyVAO);  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(homeRoofVAO);  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
        glBindVertexArray(stockBar1VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(stockBar2VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(stockBar3VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Optional navbar text
        glUseProgram(textShader);
        glUniform2f(glGetUniformLocation(textShader, "uResolution"), (float)windowWidth, (float)windowHeight);
        glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.2f);

        float npx = ndcToPixelX(-0.82f), npy = ndcToPixelY(-0.86f) + 12.0f;
        glUniform2f(glGetUniformLocation(textShader, "uOrigin"), npx, npy);
        renderText(npx, npy, "Home");

        npx = ndcToPixelX(0.62f); npy = ndcToPixelY(-0.86f) + 12.0f;
        glUniform2f(glGetUniformLocation(textShader, "uOrigin"), npx, npy);
        renderText(npx, npy, "Stocks");

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

//  Callbacks 
static void glfwErrorCallback(int code, const char *desc) {
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0,0,width,height);
    windowWidth  = width;
    windowHeight = height;
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

    double xp, yp; glfwGetCursorPos(window, &xp, &yp);
    float ndcX = (float)((2.0 * xp) / windowWidth - 1.0);
    float ndcY = (float)(1.0 - (2.0 * yp) / windowHeight);

    // Navbar hit-test (always visible)
    if (ndcY <= navY && ndcY >= navY - navH) {
        currentTab = (ndcX < 0.0f) ? TAB_HOME : TAB_STOCKS;
        if (currentTab != TAB_HOME) searchBarActive = false;
        return;
    }

    // HOME tab: search bar activation / deactivation
    if (currentTab == TAB_HOME) {
        if (ndcX >= -0.7f && ndcX <=  0.7f && ndcY <= 0.95f && ndcY >= 0.83f) {
            searchBarActive = true;
            return;
        }
        searchBarActive = false;
        return;
    }

    // STOCKS tab: tiles + buttons
    if (currentTab == TAB_STOCKS) {
        for (int i = 0; i < 3; ++i) {
            float l = stockX, r = stockX + stockW;
            float t = stockY[i], b = stockY[i] - stockH;
            if (ndcX >= l && ndcX <= r && ndcY <= t && ndcY >= b) {
                selectedStock = i;
                return;
            }
        }
        // BUY
        if (ndcX >= buyX && ndcX <= buyX+buyW && ndcY <= buyY && ndcY >= buyY-buyH) {
            Stock* s = &stocks[selectedStock];
            if (cashBalance >= s->price) { s->qty += 1; cashBalance -= s->price; }
            return;
        }
        // SELL
        if (ndcX >= sellX && ndcX <= sellX+sellW && ndcY <= sellY && ndcY >= sellY-sellH) {
            Stock* s = &stocks[selectedStock];
            if (s->qty > 0) { s->qty -= 1; cashBalance += s->price; }
            return;
        }
    }
}

static void key_callback(GLFWwindow *window, int key, int sc, int action, int mods) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) { glfwSetWindowShouldClose(window, GLFW_TRUE); return; }
    if (key == GLFW_KEY_BACKSPACE && searchBarActive) {
        if (searchLen > 0) { searchText[--searchLen] = '\0'; }
    }
}

static void char_callback(GLFWwindow *window, unsigned int codepoint) {
    if (!searchBarActive) return;
    if (codepoint == 8) { if (searchLen > 0) { searchText[--searchLen] = '\0'; } return; }
    if (codepoint >= 32 && codepoint < 127) {
        if (searchLen < (int)sizeof(searchText)-1) {
            searchText[searchLen++] = (char)codepoint;
            searchText[searchLen] = '\0';
        }
    }
}

//  Text Rendering 
static void initTextRendering(void) {
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

static void renderText(float x, float y, const char *text) {
    char buffer[100000];
    int num_quads = stb_easy_font_print((int)x, (int)y, (char*)text, NULL, buffer, sizeof(buffer));
    if (num_quads <= 0) return;

    float* src = (float*)buffer;   // each quad = 16 floats (x,y,s,t per vertex)
    int triFloats = num_quads * 6 * 2;
    float* tri = (float*)malloc(triFloats * sizeof(float));
    if (!tri) return;

    int t = 0;
    for (int i = 0; i < num_quads; ++i) {
        float* q = src + i*16;
        float x0=q[0],  y0=q[1];
        float x1=q[4],  y1=q[5];
        float x2=q[8],  y2=q[9];
        float x3=q[12], y3=q[13];

        tri[t++] = x0; tri[t++] = y0;
        tri[t++] = x1; tri[t++] = y1;
        tri[t++] = x2; tri[t++] = y2;

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

// exact pixel width from stb_easy_font (unscaled)
static float measureTextWidthRaw(const char *text) {
    if (!text || !*text) return 0.0f;
    char buf[100000];
    int quads = stb_easy_font_print(0, 0, (char*)text, NULL, buf, sizeof(buf));
    if (quads <= 0) return 0.0f;
    float* v = (float*)buf;
    float maxx = 0.0f;
    for (int i = 0; i < quads; ++i) {
        float* q = v + i*16;
        if (q[0]  > maxx) maxx = q[0];
        if (q[4]  > maxx) maxx = q[4];
        if (q[8]  > maxx) maxx = q[8];
        if (q[12] > maxx) maxx = q[12];
    }
    return maxx;
}

//  Utilities 
static unsigned int buildShader(const char *vsrc, const char *fsrc) {
    int ok; char log[1024];

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
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, sizeof(log), NULL, log); fprintf(stderr,"Link error: %s\n", log); }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

static unsigned int createTriangle(float x1,float y1,float x2,float y2,float x3,float y3, float z) {
    float vertices[] = { x1,y1,z,  x2,y2,z,  x3,y3,z };
    unsigned int idx[] = {0,1,2};
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glGenBuffers(1,&EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return VAO;
}

static float ndcToPixelX(float ndcX) {
    return (ndcX + 1.0f) * 0.5f * windowWidth;
}
static float ndcToPixelY(float ndcY) {
    return (1.0f - ndcY) * 0.5f * windowHeight;
}

// random walk update: ±(0.1%..0.6%) each tick
static void updatePricesRandomWalk(void) {
    for (int i = 0; i < 3; ++i) {
        float p = stocks[i].price;
        float pct = ((rand() % 11) + 1) / 1000.0f;         
        int dir = (rand() & 1) ? +1 : -1;
        float delta = p * pct * dir;
        p = fmaxf(1.0f, p + delta);   
        stocks[i].price = p;
    }
}