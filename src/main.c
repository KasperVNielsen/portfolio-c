// main.c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font/stb_easy_font.h"

#include "helpers.h"

// - Globals -
int windowWidth = 800;
int windowHeight = 600;

// Tabs
typedef enum { TAB_HOME = 0, TAB_STOCKS = 1 } Tab;
static Tab currentTab = TAB_HOME;

// Search bar state
static bool  searchBarActive = false;
static char  searchText[256] = {0};
static int   searchLen = 0;

// Shaders
static unsigned int rectShader = 0, textShader = 0;

// VAOs (static UI)
static unsigned int searchBarVAO = 0, chartVAO = 0, navBarVAO = 0;

// - Portfolio / Stocks -
typedef struct {
    const char* symbol;
    float price;
    int   qty;

    // tracking for returns
    float avgCost;     // average cost per share of current position
    float totalCost;   // cost basis for shares currently held (avgCost * qty)
} Stock;

static Stock stocks[3] = {
    {"AAPL", 180.0f, 0, 0.0f, 0.0f},
    {"MSFT", 330.0f, 0, 0.0f, 0.0f},
    {"NVDA", 900.0f, 0, 0.0f, 0.0f}
};

static int   selectedStock = 0;
static float cashBalance   = 10000.0f;
static float realizedPnL   = 0.0f;    // realized across all sells

// STOCKS layout (NDC)
static unsigned int stockVAO[3];
static const float stockX = -0.75f, stockW = 0.55f, stockH = 0.14f;
static const float stockY[3] = { 0.70f, 0.48f, 0.26f };

static unsigned int buyBtnVAO = 0, sellBtnVAO = 0;
static const float buyX = -0.75f, buyY = -0.05f, buyW = 0.28f, buyH = 0.10f;
static const float sellX = -0.43f, sellY = -0.05f, sellW = 0.28f, sellH = 0.10f;

// Bottom navbar + icons
static const float navX = -1.0f, navY = -0.85f, navW = 2.0f, navH = 0.15f;
static unsigned int homeBodyVAO=0, homeRoofVAO=0;
static unsigned int stockBar1VAO=0, stockBar2VAO=0, stockBar3VAO=0;

// Text rendering
static unsigned int textVAO=0, textVBO=0;

// Caret blink (search)
static double blinkLast = 0.0;
static bool   blinkOn   = true;

// -- Price update timers --
static const double PRICE_UPDATE_DT = 0.25; // sec
static double lastPriceUpdate = 0.0;

// -- Portfolio candlestick series (HOME chart) --
typedef struct { float open, high, low, close; bool valid; } Candle;
#define MAX_CANDLES 120
static Candle candles[MAX_CANDLES];
static int    candleCount     = 0;
static int    currentCandle   = -1;
static const double CANDLE_DT = 1.0;  // one candle per second
static double lastCandleTime  = 0.0;

// dynamic buffers for candle geometry
static unsigned int candleUpVAO=0, candleUpVBO=0;     // triangles (green bodies)
static unsigned int candleDnVAO=0, candleDnVBO=0;     // triangles (red bodies)
static unsigned int wickVAO=0, wickVBO=0;             // lines (black wicks)
static int upVertCount = 0, dnVertCount = 0, wickVertCount = 0; // vertex counts

// Chart area (match chartVAO rectangle)
static const float chartLeftNDC   = -0.8f;
static const float chartTopNDC    =  0.60f;
static const float chartWidthNDC  =  1.6f;
static const float chartHeightNDC =  1.20f; // bottom = top - height => -0.60

// - Shaders -
static const char* rectVS =
"#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"void main(){ gl_Position = vec4(aPos, 1.0); }\n";

static const char* rectFS =
"#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec3 uColor;\n"
"void main(){ FragColor = vec4(uColor, 1.0); }\n";

static const char* textVS =
"#version 330 core\n"
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

static const char* textFS =
"#version 330 core\n"
"out vec4 FragColor;\n"
"void main(){ FragColor = vec4(0.0,0.0,0.0,1.0); }\n";

// - Forwards -
static void glfwErrorCallback(int code, const char *desc);
static void framebuffer_size_callback(GLFWwindow *window, int width, int height);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void key_callback(GLFWwindow *window, int key, int sc, int action, int mods);
static void char_callback(GLFWwindow *window, unsigned int codepoint);

static unsigned int buildShader(const char *vsrc, const char *fsrc);
static void initTextRendering(void);
static void renderText(float x, float y, const char *text);
static float measureTextWidthRaw(const char *text);
static unsigned int createTriangle(float x1,float y1,float x2,float y2,float x3,float y3, float z);
static float ndcToPixelX(float ndcX);
static float ndcToPixelY(float ndcY);

// Candle series helpers
static void initCandleSeries(float initialValue);
static void updateCandleSeries(double now, float portfolioValue);
static float mapYValue(float val, float vmin, float vmax, float chartBottom, float height);
static void rebuildCandleMeshes(void);

// Price updater
static void updatePricesRandomWalk(void);

//  Utils 
static inline float portfolioHoldingsValue(void) {
    float v = 0.0f;
    for (int i = 0; i < 3; ++i) v += stocks[i].qty * stocks[i].price;
    return v;
}
static inline float portfolioInvested(void) {
    float v = 0.0f;
    for (int i = 0; i < 3; ++i) v += stocks[i].totalCost;
    return v;
}
static inline float portfolioUnrealizedPnL(void) {
    return portfolioHoldingsValue() - portfolioInvested();
}
static inline float portfolioTotalReturn(void) { // what we chart
    return realizedPnL + portfolioUnrealizedPnL();
}
static inline float portfolioTotal(void) { // info only
    return portfolioHoldingsValue() + cashBalance;
}

//  Main -
int main(void) {
    if (!glfwInit()) { fprintf(stderr, "Failed to init GLFW\n"); return -1; }
    glfwSetErrorCallback(glfwErrorCallback);

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Portfolio UI", NULL, NULL);
    if (!window) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        window = glfwCreateWindow(windowWidth, windowHeight, "Portfolio UI", NULL, NULL);
    }
    if (!window) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(windowWidth, windowHeight, "Portfolio UI", NULL, NULL);
    }
    if (!window) { fprintf(stderr, "Failed to create window\n"); glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGL()) { fprintf(stderr, "Failed to init GLAD\n"); glfwDestroyWindow(window); glfwTerminate(); return -1; }

    rectShader = buildShader(rectVS, rectFS);
    textShader = buildShader(textVS, textFS);

    // HOME: search bar + chart bg
    searchBarVAO = createRectangle(-0.7f, 0.95f, 1.4f, 0.12f);  // top center
    chartVAO     = createRectangle(chartLeftNDC, chartTopNDC, chartWidthNDC, chartHeightNDC);

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

    // Text
    initTextRendering();

    // Candles dynamic buffers
    glGenVertexArrays(1, &candleUpVAO);  glGenBuffers(1, &candleUpVBO);
    glBindVertexArray(candleUpVAO);      glBindBuffer(GL_ARRAY_BUFFER, candleUpVBO);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &candleDnVAO);  glGenBuffers(1, &candleDnVBO);
    glBindVertexArray(candleDnVAO);      glBindBuffer(GL_ARRAY_BUFFER, candleDnVBO);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &wickVAO);      glGenBuffers(1, &wickVBO);
    glBindVertexArray(wickVAO);          glBindBuffer(GL_ARRAY_BUFFER, wickVBO);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Init prices/candles
    srand((unsigned)time(NULL));
    lastPriceUpdate = glfwGetTime();
    lastCandleTime  = lastPriceUpdate;
    initCandleSeries(portfolioTotalReturn());

    blinkLast = glfwGetTime();
    glLineWidth(1.0f);

    // - Render loop -
    while (!glfwWindowShouldClose(window)) {
        // update prices periodically
        double now = glfwGetTime();
        if (now - lastPriceUpdate >= PRICE_UPDATE_DT) {
            updatePricesRandomWalk();
            lastPriceUpdate = now;
        }
        // update candle series from TOTAL RETURN
        updateCandleSeries(now, portfolioTotalReturn());
        // rebuild candle meshes for drawing
        rebuildCandleMeshes();

        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(rectShader);

        if (currentTab == TAB_HOME) {
            // Chart background
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.88f, 0.88f, 0.88f);
            glBindVertexArray(chartVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Draw wicks (black) behind bodies
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.10f, 0.10f, 0.10f);
            glBindVertexArray(wickVAO);
            glDrawArrays(GL_LINES, 0, wickVertCount);

            // Bodies: up (green), down (red)
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.20f, 0.70f, 0.30f);
            glBindVertexArray(candleUpVAO);
            glDrawArrays(GL_TRIANGLES, 0, upVertCount);

            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.85f, 0.25f, 0.25f);
            glBindVertexArray(candleDnVAO);
            glDrawArrays(GL_TRIANGLES, 0, dnVertCount);

            // Search bar
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.65f, 0.65f, 0.65f);
            glBindVertexArray(searchBarVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Search text + caret
            if (searchBarActive || searchLen > 0) {
                glUseProgram(textShader);
                glUniform2f(glGetUniformLocation(textShader, "uResolution"),
                            (float)windowWidth, (float)windowHeight);
                const float sbTopNDC = 0.95f, sbBotNDC = 0.83f;
                float sbTopPx = ndcToPixelY(sbTopNDC);
                float sbBotPx = ndcToPixelY(sbBotNDC);
                float sbMidPx = 0.5f*(sbTopPx + sbBotPx);

                float textX = 120.0f;
                float textY = sbMidPx - 4.0f;
                float scale = 1.8f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  scale);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), textX, textY);
                const char* toShow = (searchLen > 0) ? searchText : "Type to search...";
                renderText(textX, textY, toShow);

                double n2 = glfwGetTime();
                if (n2 - blinkLast > 0.5) { blinkOn = !blinkOn; blinkLast = n2; }
                if (blinkOn) {
                    float rawW = measureTextWidthRaw(toShow);
                    renderText(textX + rawW, textY, "|");
                }
            }

            // Portfolio labels inside chart (top-left)
            {
                glUseProgram(textShader);
                glUniform2f(glGetUniformLocation(textShader, "uResolution"),
                            (float)windowWidth, (float)windowHeight);
                float px = ndcToPixelX(chartLeftNDC) + 10.0f;
                float py = ndcToPixelY(chartTopNDC)  + 16.0f;

                char line[160];
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.2f);

                snprintf(line, sizeof(line), "Holdings: $%.2f", portfolioHoldingsValue());
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, line);

                py += 16.0f;
                snprintf(line, sizeof(line), "Invested: $%.2f", portfolioInvested());
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, line);

                py += 16.0f;
                snprintf(line, sizeof(line), "Unrealized: $%.2f", portfolioUnrealizedPnL());
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, line);

                py += 16.0f;
                snprintf(line, sizeof(line), "Realized: $%.2f", realizedPnL);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, line);

                py += 16.0f;
                snprintf(line, sizeof(line), "Total Return: $%.2f", portfolioTotalReturn());
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, line);
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

            // STOCKS text
            glUseProgram(textShader);
            glUniform2f(glGetUniformLocation(textShader, "uResolution"),
                        (float)windowWidth, (float)windowHeight);

            // Labels centered in tiles
            for (int i = 0; i < 3; ++i) {
                float leftPx = ndcToPixelX(stockX) + 12.0f;
                float topPx  = ndcToPixelY(stockY[i]);
                float botPx  = ndcToPixelY(stockY[i] - stockH);
                float midPx  = 0.5f * (topPx + botPx);
                float px = leftPx, py = midPx - 2.0f;
                float scale = 1.3f;
                char line[128];
                snprintf(line, sizeof(line), "%s  $%.2f  x%d  avg $%.2f",
                         stocks[i].symbol, stocks[i].price, stocks[i].qty, stocks[i].avgCost);
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  scale);
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

            // Cash above buttons
            {
                float cashNdcY = buyY + 0.14f;
                float centerNdcX = (buyX + (sellX + sellW)) * 0.5f;
                float px = ndcToPixelX(centerNdcX) - 60.0f;
                float py = ndcToPixelY(cashNdcY);
                char cash[64]; snprintf(cash, sizeof(cash), "Cash: $%.2f", cashBalance);
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.3f);
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
        glBindVertexArray(homeRoofVAO);  glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(stockBar1VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(stockBar2VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(stockBar3VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Navbar text
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

// - Callbacks --
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
            if (cashBalance >= s->price) {
                cashBalance -= s->price;
                s->totalCost += s->price;
                s->qty += 1;
                s->avgCost = (s->qty > 0) ? (s->totalCost / (float)s->qty) : 0.0f;
            }
            return;
        }
        // SELL
        if (ndcX >= sellX && ndcX <= sellX+sellW && ndcY <= sellY && ndcY >= sellY-sellH) {
            Stock* s = &stocks[selectedStock];
            if (s->qty > 0) {
                // realize P&L for one share at current price vs avg cost
                realizedPnL += (s->price - s->avgCost);

                // receive cash
                cashBalance += s->price;

                // reduce position / cost basis
                s->qty -= 1;
                s->totalCost -= s->avgCost;         // remove one share of cost basis
                if (s->qty <= 0) {
                    s->qty = 0;
                    s->totalCost = 0.0f;
                    s->avgCost = 0.0f;
                } else {
                    s->avgCost = s->totalCost / (float)s->qty;
                }
            }
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

//  Text Rendering -
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

    // convert quads to triangles
    float* src = (float*)buffer;   // each quad = 16 floats (positions; we only use x,y)
    const int triFloats = num_quads * 6 * 2; // 2 floats per vertex, 6 vertices per quad
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
    glUseProgram(textShader);
    glDrawArrays(GL_TRIANGLES, 0, triFloats / 2);
    free(tri);
}
static float measureTextWidthRaw(const char *text) {
    if (!text || !*text) return 0.0f;
    char buf[100000];
    int quads = stb_easy_font_print(0, 0, (char*)text, NULL, buf, sizeof(buf));
    if (quads <= 0) return 0.0f;
    float* v = (float*)buf; float maxx = 0.0f;
    for (int i = 0; i < quads; ++i) {
        float* q = v + i*16;
        if (q[0]  > maxx) maxx = q[0];
        if (q[4]  > maxx) maxx = q[4];
        if (q[8]  > maxx) maxx = q[8];
        if (q[12] > maxx) maxx = q[12];
    }
    return maxx;
}

// -- Utilities --
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
    unsigned int VAO, VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return VAO;
}
static float ndcToPixelX(float ndcX) { return (ndcX + 1.0f) * 0.5f * windowWidth; }
static float ndcToPixelY(float ndcY) { return (1.0f - ndcY) * 0.5f * windowHeight; }

// -- Candlestick series (HOME) --
static void initCandleSeries(float initialValue) {
    for (int i=0;i<MAX_CANDLES;++i) {
        candles[i].open = candles[i].high = candles[i].low = candles[i].close = initialValue;
        candles[i].valid = false;
    }
    currentCandle = 0;
    candleCount   = 1;
    candles[0].open  = candles[0].high = candles[0].low = candles[0].close = initialValue;
    candles[0].valid = true;
}
static void updateCandleSeries(double now, float seriesValue) {
    if (currentCandle < 0) return;

    Candle* c = &candles[currentCandle];
    if (!c->valid) {
        c->open = c->high = c->low = c->close = seriesValue;
        c->valid = true;
    }
    // live update H/L/C
    c->close = seriesValue;
    if (seriesValue > c->high) c->high = seriesValue;
    if (seriesValue < c->low ) c->low  = seriesValue;

    // start a new candle each second
    if (now - lastCandleTime >= CANDLE_DT) {
        lastCandleTime = now;
        int next = (currentCandle + 1) % MAX_CANDLES;
        Candle* n = &candles[next];
        n->open = c->close;
        n->high = n->low = n->close = n->open;
        n->valid = true;
        currentCandle = next;
        if (candleCount < MAX_CANDLES) candleCount++;
    }
}
static float mapYValue(float val, float vmin, float vmax, float chartBottom, float height) {
    float t = (val - vmin) / (vmax - vmin);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return chartBottom + t * height;
}
static void rebuildCandleMeshes(void) {
    if (candleCount <= 0) { upVertCount=dnVertCount=wickVertCount=0; return; }

    // order oldest -> newest within ring
    int count = candleCount;
    int first = (candleCount == MAX_CANDLES) ? (currentCandle + 1) % MAX_CANDLES : 0;

    // find min/max for scaling
    float vmin =  1e30f, vmax = -1e30f;
    for (int i = 0; i < count; ++i) {
        int idx = (first + i) % MAX_CANDLES;
        if (!candles[idx].valid) continue;
        Candle c = candles[idx];
        if (c.low  < vmin) vmin = c.low;
        if (c.high > vmax) vmax = c.high;
    }
    if (!(vmax > vmin)) { vmax = vmin + 1.0f; } // avoid /0
    float pad = 0.05f * (vmax - vmin);
    vmin -= pad; vmax += pad;

    // x layout
    const float left   = chartLeftNDC;
    const float top    = chartTopNDC;
    const float width  = chartWidthNDC;
    const float height = chartHeightNDC; // chartBottom = top - height
    const float step   = width / (float)MAX_CANDLES;

    // Candle body width (slim) + minimum body px height
    const float bodyW      = step * 1.00f;              // tweak for thicker/thinner candles
    const float minBodyPx  = 2.0f;                      // at least 2 px tall
    const float minBodyNDC = (minBodyPx * 2.0f) / (float)windowHeight;

    // worst-case buffers
    int maxUpVerts   = count * 6; // 2 triangles per body * 3 verts
    int maxDnVerts   = count * 6;
    int maxWickVerts = count * 2; // 1 line = 2 verts

    float* up  = (float*)malloc(sizeof(float)*3*maxUpVerts);
    float* dn  = (float*)malloc(sizeof(float)*3*maxDnVerts);
    float* wck = (float*)malloc(sizeof(float)*3*maxWickVerts);
    if (!up || !dn || !wck) { free(up); free(dn); free(wck); upVertCount=dnVertCount=wickVertCount=0; return; }
    int upV=0, dnV=0, wkV=0;

    const float chartBottom = top - height;

    for (int i = 0; i < count; ++i) {
        int idx = (first + i) % MAX_CANDLES;
        if (!candles[idx].valid) continue;
        Candle c = candles[idx];

        // map value -> NDC Y
        float yOpen  = mapYValue(c.open,  vmin, vmax, chartBottom, height);
        float yClose = mapYValue(c.close, vmin, vmax, chartBottom, height);
        float yHigh  = mapYValue(c.high,  vmin, vmax, chartBottom, height);
        float yLow   = mapYValue(c.low,   vmin, vmax, chartBottom, height);

        // center of this candle slot
        float xCenter = left + (float)i * step + step * 0.5f;
        float x0 = xCenter - bodyW * 0.5f;
        float x1 = xCenter + bodyW * 0.5f;

        // z slightly above wicks/background
        float zBody = -0.25f;
        float zWick = -0.20f;

        // wick: vertical line
        if (wkV + 2 <= maxWickVerts) {
            wck[wkV*3+0] = xCenter; wck[wkV*3+1] = yLow;  wck[wkV*3+2] = zWick; wkV++;
            wck[wkV*3+0] = xCenter; wck[wkV*3+1] = yHigh; wck[wkV*3+2] = zWick; wkV++;
        }

        // body rectangle as 2 triangles
        float yB = fminf(yOpen, yClose);
        float yT = fmaxf(yOpen, yClose);

        // enforce minimum body height
        if ((yT - yB) < minBodyNDC) {
            float mid = 0.5f * (yT + yB);
            yB = mid - 0.5f * minBodyNDC;
            yT = mid + 0.5f * minBodyNDC;
        }

        float tri[18] = {
            x0,yB,zBody,  x1,yB,zBody,  x1,yT,zBody,
            x0,yB,zBody,  x1,yT,zBody,  x0,yT,zBody
        };

        bool isUp = (c.close >= c.open);
        if (isUp) {
            if (upV + 6 <= maxUpVerts) { memcpy(up + upV*3, tri, sizeof(tri)); upV += 6; }
        } else {
            if (dnV + 6 <= maxDnVerts) { memcpy(dn + dnV*3, tri, sizeof(tri)); dnV += 6; }
        }
    }

    // upload to GPU
    glBindVertexArray(candleUpVAO);
    glBindBuffer(GL_ARRAY_BUFFER, candleUpVBO);
    glBufferData(GL_ARRAY_BUFFER, upV * 3 * sizeof(float), up, GL_DYNAMIC_DRAW);
    upVertCount = upV;

    glBindVertexArray(candleDnVAO);
    glBindBuffer(GL_ARRAY_BUFFER, candleDnVBO);
    glBufferData(GL_ARRAY_BUFFER, dnV * 3 * sizeof(float), dn, GL_DYNAMIC_DRAW);
    dnVertCount = dnV;

    glBindVertexArray(wickVAO);
    glBindBuffer(GL_ARRAY_BUFFER, wickVBO);
    glBufferData(GL_ARRAY_BUFFER, wkV * 3 * sizeof(float), wck, GL_DYNAMIC_DRAW);
    wickVertCount = wkV;

    free(up); free(dn); free(wck);
}

//  Price simulation 
static void updatePricesRandomWalk(void) {
    for (int i = 0; i < 3; ++i) {
        float p = stocks[i].price;
        float pct = ((rand() % 11) + 1) / 1000.0f; // 0.1%..1.2%
        int dir = (rand() & 1) ? +1 : -1;
        float delta = p * pct * dir;
        p = fmaxf(1.0f, p + delta);
        stocks[i].price = p;
    }
}
