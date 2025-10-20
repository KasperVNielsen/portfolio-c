// main.c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h> // <-- added

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font/stb_easy_font.h"

#include "helpers.h"

static int windowWidth = 800;
static int windowHeight = 600;

typedef enum { TAB_HOME = 0, TAB_STOCKS = 1 } Tab;
static Tab currentTab = TAB_HOME;

static bool  searchBarActive = false;
static char  searchText[256] = {0};
static int   searchLen = 0;

static unsigned int rectShader = 0, textShader = 0;

static unsigned int searchBarVAO = 0, chartVAO = 0, navBarVAO = 0;

typedef struct {
    const char* symbol;
    float price;
    int   qty;
    float avgCost;
    float totalCost;
} Stock;

static Stock stocks[3] = {
    {"AAPL", 180.0f, 0, 0.0f, 0.0f},
    {"MSFT", 330.0f, 0, 0.0f, 0.0f},
    {"NVDA", 900.0f, 0, 0.0f, 0.0f}
};

static int   selectedStock = 0;
static float cashBalance   = 10000.0f;
static float realizedPnL   = 0.0f;

static unsigned int stockVAO[3];
static const float stockX = -0.75f, stockW = 0.55f, stockH = 0.14f;
static const float stockY[3] = { 0.70f, 0.48f, 0.26f };

static unsigned int buyBtnVAO = 0, sellBtnVAO = 0;
static const float buyX = -0.75f, buyY = -0.05f, buyW = 0.28f, buyH = 0.10f;
static const float sellX = -0.43f, sellY = -0.05f, sellW = 0.28f, sellH = 0.10f;

static const float navX = -1.0f, navY = -0.85f, navW = 2.0f, navH = 0.15f;
static unsigned int homeBodyVAO=0, homeRoofVAO=0;
static unsigned int stockBar1VAO=0, stockBar2VAO=0, stockBar3VAO=0;

// Active-tab underline (left/right halves)
static unsigned int navActiveLeftVAO = 0, navActiveRightVAO = 0;

// NEW: Center "Add Balance" navbar button
static unsigned int navAddBtnVAO = 0;
static const float addBtnX = -0.20f, addBtnY = -0.86f, addBtnW = 0.40f, addBtnH = 0.10f;

// NEW: Add-Balance modal UI
static bool addModalOpen = false;
static bool addInputActive = false;
static char addInputText[64] = {0};
static int  addInputLen = 0;
static unsigned int addPanelVAO = 0, addInputVAO = 0, addConfirmBtnVAO = 0;
// panel rect: centered box
static const float panelX = -0.40f, panelY = 0.30f, panelW = 0.80f, panelH = 0.60f;
// input rect inside panel
static const float inX = -0.30f, inY = 0.15f, inW = 0.60f, inH = 0.10f;
// confirm button inside panel
static const float confX = -0.10f, confY = -0.05f, confW = 0.20f, confH = 0.12f;

static unsigned int textVAO=0, textVBO=0;

static double blinkLast = 0.0;
static bool   blinkOn   = true;

static const double PRICE_UPDATE_DT = 0.25;
static double lastPriceUpdate = 0.0;

typedef struct { float open, high, low, close; bool valid; } Candle;
#define MAX_CANDLES 240
static Candle candles[MAX_CANDLES];
static int    candleCount     = 0;
static int    currentCandle   = -1;
static const double CANDLE_DT = 1.0;
static double lastCandleTime  = 0.0;

static unsigned int candleUpVAO=0, candleUpVBO=0;
static unsigned int candleDnVAO=0, candleDnVBO=0;
static unsigned int wickVAO=0, wickVBO=0;
static int upVertCount = 0, dnVertCount = 0, wickVertCount = 0;

static const float chartLeftNDC   = -0.8f;
static const float chartTopNDC    =  0.60f;
static const float chartWidthNDC  =  1.6f;
static const float chartHeightNDC =  1.20f;

// --- Search results (rendered on Home under the search bar) ---
static unsigned int searchResVAO[3] = {0,0,0};
static const float resX = -0.70f, resW = 1.40f, resH = 0.12f;
static const float resYBase = 0.80f;   // first row starts just under the search bar
static const float resYStep = 0.14f;   // spacing between rows

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

static void glfwErrorCallback(int code, const char *desc);
static void framebuffer_size_callback(GLFWwindow *window, int width, int height);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void key_callback(GLFWwindow *window, int key, int sc, int action, int mods);
static void char_callback(GLFWwindow *window, unsigned int codepoint);

static void initTextRendering(void);
static void renderText(float x, float y, const char *text);
static float measureTextWidthRaw(const char *text);
static float ndcToPixelX(float ndcX);
static float ndcToPixelY(float ndcY);

static void initCandleSeries(float initialValue);
static void updateCandleSeries(double now, float valueToChart);
static float mapYValue(float val, float vmin, float vmax, float chartBottom, float height);
static void rebuildCandleMeshes(void);
static void updatePricesRandomWalk(void);

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
static inline float portfolioTotalReturn(void) {
    return realizedPnL + portfolioUnrealizedPnL();
}
static inline bool hasAnyPosition(void) {
    for (int i = 0; i < 3; ++i) if (stocks[i].qty > 0) return true;
    return false;
}
// Equity = cash + live holdings
static inline float portfolioEquity(void) {
    return cashBalance + portfolioHoldingsValue();
}

// -------- Search helpers --------
static bool icontains(const char* hay, const char* needle) {
    if (!hay || !needle || !*needle) return false;
    size_t nlen = strlen(needle);
    size_t hlen = strlen(hay);
    for (size_t i = 0; i + nlen <= hlen; ++i) {
        bool ok = true;
        for (size_t j = 0; j < nlen; ++j) {
            char a = (char)toupper((unsigned char)hay[i+j]);
            char b = (char)toupper((unsigned char)needle[j]);
            if (a != b) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}
// Fill outIdx with indices of matching stocks; returns match count (0..3)
static int computeSearchMatches(int* outIdx /* size >= 3 */) {
    int m = 0;
    if (searchLen == 0) return 0;
    for (int i = 0; i < 3; ++i) {
        if (icontains(stocks[i].symbol, searchText)) {
            outIdx[m++] = i;
        }
    }
    return m;
}

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
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1.0f);

    rectShader = buildShader(rectVS, rectFS);
    textShader = buildShader(textVS, textFS);

    searchBarVAO = createRectangle(-0.7f, 0.95f, 1.4f, 0.12f);
    chartVAO     = createRectangle(chartLeftNDC, chartTopNDC, chartWidthNDC, chartHeightNDC);

    // Prebuild up to 3 result rows below the search bar (Y descends in NDC)
    for (int i = 0; i < 3; ++i) {
        float y = resYBase - i * resYStep;
        searchResVAO[i] = createRectangle(resX, y, resW, resH);
    }

    for (int i = 0; i < 3; ++i) stockVAO[i] = createRectangle(stockX, stockY[i], stockW, stockH);
    buyBtnVAO  = createRectangle(buyX,  buyY,  buyW,  buyH);
    sellBtnVAO = createRectangle(sellX, sellY, sellW, sellH);

    navBarVAO   = createRectangle(navX, navY, navW, navH);
    homeBodyVAO = createRectangle(-0.78f, -0.90f, 0.12f, 0.07f);
    homeRoofVAO = createTriangle( -0.84f, -0.90f,  -0.72f, -0.98f,  -0.60f, -0.90f, -0.45f);
    stockBar1VAO = createRectangle( 0.62f, -0.90f, 0.05f, 0.05f);
    stockBar2VAO = createRectangle( 0.69f, -0.90f, 0.05f, 0.08f);
    stockBar3VAO = createRectangle( 0.76f, -0.90f, 0.05f, 0.12f);

    // Active-tab underline strips
    navActiveLeftVAO  = createRectangle(-1.0f, navY, 1.0f, 0.02f);
    navActiveRightVAO = createRectangle( 0.0f, navY, 1.0f, 0.02f);

    // NEW: Center navbar "Add Balance" button
    navAddBtnVAO = createRectangle(addBtnX, addBtnY, addBtnW, addBtnH);

    // NEW: Modal UI elements
    addPanelVAO      = createRectangle(panelX, panelY, panelW, panelH);
    addInputVAO      = createRectangle(inX,    inY,    inW,    inH);
    addConfirmBtnVAO = createRectangle(confX,  confY,  confW,  confH);

    initTextRendering();

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

    srand((unsigned)time(NULL));
    lastPriceUpdate = glfwGetTime();
    lastCandleTime  = lastPriceUpdate;

    for (int i=0;i<MAX_CANDLES;++i) candles[i].valid = false;
    candleCount = 0; currentCandle = -1;

    blinkLast = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        if (now - lastPriceUpdate >= PRICE_UPDATE_DT) {
            updatePricesRandomWalk();
            lastPriceUpdate = now;
        }

        if (hasAnyPosition()) {
            if (candleCount == 0 || currentCandle < 0) {
                initCandleSeries(realizedPnL + (portfolioHoldingsValue() - portfolioInvested()));
                lastCandleTime = now;
            }
            updateCandleSeries(now, realizedPnL + (portfolioHoldingsValue() - portfolioInvested()));
            rebuildCandleMeshes();
        } else {
            upVertCount = dnVertCount = wickVertCount = 0;
        }

        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(rectShader);

        if (currentTab == TAB_HOME) {
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.88f, 0.88f, 0.88f);
            glBindVertexArray(chartVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            if (wickVertCount > 0 || upVertCount > 0 || dnVertCount > 0) {
                glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.10f, 0.10f, 0.10f);
                glBindVertexArray(wickVAO);
                glDrawArrays(GL_LINES, 0, wickVertCount);

                glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.20f, 0.70f, 0.30f);
                glBindVertexArray(candleUpVAO);
                glDrawArrays(GL_TRIANGLES, 0, upVertCount);

                glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.85f, 0.25f, 0.25f);
                glBindVertexArray(candleDnVAO);
                glDrawArrays(GL_TRIANGLES, 0, dnVertCount);
            } else {
                glUseProgram(textShader);
                glUniform2f(glGetUniformLocation(textShader, "uResolution"),
                            (float)windowWidth, (float)windowHeight);
                float px = ndcToPixelX(chartLeftNDC) + 12.0f;
                float py = ndcToPixelY(chartTopNDC) - 28.0f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"), 1.4f);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
                renderText(px, py, "Buy a stock to start charting return");
                glUseProgram(rectShader);
            }

            // Search bar background
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
                glUseProgram(rectShader);
            }

            // Render search results under the search bar
            if (searchLen > 0) {
                int idx[3]; int count = computeSearchMatches(idx);

                for (int i = 0; i < count && i < 3; ++i) {
                    // Row background
                    glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.92f, 0.92f, 0.95f);
                    glBindVertexArray(searchResVAO[i]);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

                    // Row text: SYMBOL  $price  xqty  avg $avgCost
                    glUseProgram(textShader);
                    glUniform2f(glGetUniformLocation(textShader, "uResolution"),
                                (float)windowWidth, (float)windowHeight);
                    float topPx  = ndcToPixelY(resYBase - i*resYStep);
                    float botPx  = ndcToPixelY((resYBase - i*resYStep) - resH);
                    float midPx  = 0.5f * (topPx + botPx);

                    float tx = ndcToPixelX(resX) + 12.0f;
                    float ty = midPx - 2.0f;
                    glUniform1f(glGetUniformLocation(textShader, "uScale"), 1.3f);
                    glUniform2f(glGetUniformLocation(textShader, "uOrigin"), tx, ty);

                    char line2[128];
                    int s = idx[i];
                    snprintf(line2, sizeof(line2), "%s  $%.2f  x%d  avg $%.2f",
                             stocks[s].symbol, stocks[s].price, stocks[s].qty, stocks[s].avgCost);
                    renderText(tx, ty, line2);
                    glUseProgram(rectShader);
                }

                // No matches message
                if (count == 0) {
                    glUseProgram(textShader);
                    glUniform2f(glGetUniformLocation(textShader, "uResolution"),
                                (float)windowWidth, (float)windowHeight);
                    float tx = ndcToPixelX(resX) + 12.0f;
                    float ty = ndcToPixelY(resYBase) + 14.0f;
                    glUniform1f(glGetUniformLocation(textShader, "uScale"), 1.0f);
                    glUniform2f(glGetUniformLocation(textShader, "uOrigin"), tx, ty);
                    renderText(tx, ty, "No matching stocks");
                    glUseProgram(rectShader);
                }
            }

            // Portfolio stats on Home
            glUseProgram(textShader);
            glUniform2f(glGetUniformLocation(textShader, "uResolution"),
                        (float)windowWidth, (float)windowHeight);
            float px = ndcToPixelX(chartLeftNDC) + 10.0f;
            float py = ndcToPixelY(chartTopNDC)  + 16.0f;
            char line[160];
            glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.2f);

            snprintf(line, sizeof(line), "Cash: $%.2f", cashBalance);
            glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px, py);
            renderText(px, py, line);

            py += 16.0f;
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
            glUseProgram(rectShader);

        } else {
            for (int i = 0; i < 3; ++i) {
                if (i == selectedStock) glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.75f, 0.75f, 0.90f);
                else                    glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.88f, 0.88f, 0.88f);
                glBindVertexArray(stockVAO[i]);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }

            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.60f, 0.85f, 0.60f);
            glBindVertexArray(buyBtnVAO);  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.90f, 0.60f, 0.60f);
            glBindVertexArray(sellBtnVAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            glUseProgram(textShader);
            glUniform2f(glGetUniformLocation(textShader, "uResolution"),
                        (float)windowWidth, (float)windowHeight);

            for (int i = 0; i < 3; ++i) {
                float leftPx = ndcToPixelX(stockX) + 12.0f;
                float topPx  = ndcToPixelY(stockY[i]);
                float botPx  = ndcToPixelY(stockY[i] - stockH);
                float midPx  = 0.5f * (topPx + botPx);
                float px2 = leftPx, py2 = midPx - 2.0f;
                float scale = 1.3f;
                char line2[128];
                snprintf(line2, sizeof(line2), "%s  $%.2f  x%d  avg $%.2f",
                         stocks[i].symbol, stocks[i].price, stocks[i].qty, stocks[i].avgCost);
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  scale);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px2, py2);
                renderText(px2, py2, line2);
            }

            {
                float px2 = ndcToPixelX(buyX) + 20.0f;
                float py2 = ndcToPixelY(buyY) - 10.0f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.6f);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px2, py2);
                renderText(px2, py2, "BUY");

                px2 = ndcToPixelX(sellX) + 16.0f;
                py2 = ndcToPixelY(sellY) - 10.0f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.6f);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px2, py2);
                renderText(px2, py2, "SELL");
            }

            {
                float px2 = ndcToPixelX(buyX);
                float py2 = ndcToPixelY(buyY) - 40.0f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"), 1.3f);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), px2, py2);
                char line2[128];
                snprintf(line2, sizeof(line2), "Cash: $%.2f", cashBalance);
                renderText(px2, py2, line2);
                glUseProgram(rectShader);
            }
        }

        // NAVBAR BACKGROUND
        glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.80f, 0.80f, 0.80f);
        glBindVertexArray(navBarVAO);  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // NAVBAR ICONS
        glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.30f, 0.30f, 0.30f);
        glBindVertexArray(homeBodyVAO);  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(homeRoofVAO);  glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(stockBar1VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(stockBar2VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(stockBar3VAO); glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Active tab underline
        glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.20f, 0.45f, 0.85f);
        if (currentTab == TAB_HOME) glBindVertexArray(navActiveLeftVAO);
        else                        glBindVertexArray(navActiveRightVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // NEW: Center "Add Balance" button on navbar
        glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.35f, 0.65f, 0.95f);
        glBindVertexArray(navAddBtnVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // NAVBAR TEXT (labels + balance + add balance text)
        glUseProgram(textShader);
        glUniform2f(glGetUniformLocation(textShader, "uResolution"), (float)windowWidth, (float)windowHeight);

        glUniform1f(glGetUniformLocation(textShader, "uScale"),  1.2f);
        float npx = ndcToPixelX(-0.82f), npy = ndcToPixelY(-0.86f) + 12.0f;
        glUniform2f(glGetUniformLocation(textShader, "uOrigin"), npx, npy);
        renderText(npx, npy, "Home");

        npx = ndcToPixelX(0.62f); npy = ndcToPixelY(-0.86f) + 12.0f;
        glUniform2f(glGetUniformLocation(textShader, "uOrigin"), npx, npy);
        renderText(npx, npy, "Stocks");

        // Add Balance text (center button)
        {
            float scale = 1.2f;
            float xPx = ndcToPixelX(addBtnX) + 12.0f;
            float yPx = ndcToPixelY(addBtnY) + 12.0f;
            glUniform1f(glGetUniformLocation(textShader, "uScale"),  scale);
            glUniform2f(glGetUniformLocation(textShader, "uOrigin"), xPx, yPx);
            renderText(xPx, yPx, "Add Balance");
        }

        glUseProgram(rectShader);

        // NEW: Add Balance Modal
        if (addModalOpen) {
            // Panel
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.94f, 0.94f, 0.96f);
            glBindVertexArray(addPanelVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Input box
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.85f, 0.85f, 0.85f);
            glBindVertexArray(addInputVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Confirm button
            glUniform3f(glGetUniformLocation(rectShader, "uColor"), 0.40f, 0.80f, 0.50f);
            glBindVertexArray(addConfirmBtnVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Text on modal
            glUseProgram(textShader);
            glUniform2f(glGetUniformLocation(textShader, "uResolution"), (float)windowWidth, (float)windowHeight);

            // Title
            float tScale = 1.6f;
            float tx = ndcToPixelX(panelX) + 20.0f;
            float ty = ndcToPixelY(panelY) - 28.0f;
            glUniform1f(glGetUniformLocation(textShader, "uScale"),  tScale);
            glUniform2f(glGetUniformLocation(textShader, "uOrigin"), tx, ty);
            renderText(tx, ty, "Add Balance");

            // Label
            float lScale = 1.2f;
            float lx = ndcToPixelX(inX);
            float ly = ndcToPixelY(inY) - 14.0f;
            glUniform1f(glGetUniformLocation(textShader, "uScale"),  lScale);
            glUniform2f(glGetUniformLocation(textShader, "uOrigin"), lx, ly);
            renderText(lx, ly, "Amount (e.g. 250.00):");

            // Input text or placeholder
            {
                float scale = 1.4f;
                float ix = ndcToPixelX(inX) + 10.0f;
                float iy = ndcToPixelY(inY) + (ndcToPixelY(inY - inH) - ndcToPixelY(inY)) * 0.5f - 6.0f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  scale);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), ix, iy);
                const char* toShow = (addInputLen > 0) ? addInputText : "0.00";
                renderText(ix, iy, toShow);

                double n2 = glfwGetTime();
                if (n2 - blinkLast > 0.5) { blinkOn = !blinkOn; blinkLast = n2; }
                if (addInputActive && blinkOn) {
                    float rawW = measureTextWidthRaw(toShow) * scale;
                    renderText(ix + rawW, iy, "|");
                }
            }

            // Confirm button text
            {
                float scale = 1.3f;
                float bx = ndcToPixelX(confX) + 10.0f;
                float by = ndcToPixelY(confY) + 14.0f;
                glUniform1f(glGetUniformLocation(textShader, "uScale"),  scale);
                glUniform2f(glGetUniformLocation(textShader, "uOrigin"), bx, by);
                renderText(bx, by, "Add Balance");
            }

            glUseProgram(rectShader);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

static void glfwErrorCallback(int code, const char *desc) {
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}
static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    if (width < 1)  width = 1;
    if (height < 1) height = 1;
    glViewport(0,0,width,height);
    windowWidth  = width;
    windowHeight = height;
}

static bool pointInRectNDC(float x, float y, float rx, float ry, float rw, float rh) {
    return (x >= rx && x <= rx+rw && y <= ry && y >= ry-rh);
}

static void submitAddBalance(void) {
    // Parse the input as float and add to cash
    if (addInputLen > 0) {
        float amt = (float)atof(addInputText);
        if (amt > 0.0f && isfinite(amt)) {
            cashBalance += amt;
        }
    }
    addModalOpen = false;
    addInputActive = false;
    addInputLen = 0;
    addInputText[0] = '\0';
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

    double xp, yp; glfwGetCursorPos(window, &xp, &yp);
    float ndcX = (float)((2.0 * xp) / windowWidth - 1.0);
    float ndcY = (float)(1.0 - (2.0 * yp) / windowHeight);

    // If modal is open: handle modal interactions first
    if (addModalOpen) {
        // Click confirm button
        if (pointInRectNDC(ndcX, ndcY, confX, confY, confW, confH)) {
            submitAddBalance();
            return;
        }
        // Click input box
        if (pointInRectNDC(ndcX, ndcY, inX, inY, inW, inH)) {
            addInputActive = true;
            return;
        }
        // Click outside panel closes modal
        if (!pointInRectNDC(ndcX, ndcY, panelX, panelY, panelW, panelH)) {
            addModalOpen = false;
            addInputActive = false;
            return;
        }
        // Click inside panel but not on input/confirm: keep focus state
        return;
    }

    // Navbar interactions
    if (ndcY <= navY && ndcY >= navY - navH) {
        // Click on center "Add Balance" button?
        if (pointInRectNDC(ndcX, ndcY, addBtnX, addBtnY, addBtnW, addBtnH)) {
            addModalOpen = true;
            addInputActive = true;
            return;
        }
        // Otherwise, switch tab by halves
        currentTab = (ndcX < 0.0f) ? TAB_HOME : TAB_STOCKS;
        if (currentTab != TAB_HOME) searchBarActive = false;
        return;
    }

    if (currentTab == TAB_HOME) {
        // Click inside search bar focuses it
        if (ndcX >= -0.7f && ndcX <=  0.7f && ndcY <= 0.95f && ndcY >= 0.83f) {
            searchBarActive = true;
            return;
        }

        // If there is search text, allow clicking on result rows to select
        if (searchLen > 0) {
            int idx[3]; int count = computeSearchMatches(idx);
            for (int i = 0; i < count && i < 3; ++i) {
                float y = resYBase - i * resYStep;
                if (pointInRectNDC(ndcX, ndcY, resX, y, resW, resH)) {
                    selectedStock = idx[i];
                    currentTab = TAB_STOCKS;      // jump to Stocks tab
                    searchBarActive = false;      // blur search
                    return;
                }
            }
        }

        // Clicked elsewhere on Home
        searchBarActive = false;
        return;
    }

    if (currentTab == TAB_STOCKS) {
        for (int i = 0; i < 3; ++i) {
            float l = stockX, r = stockX + stockW;
            float t = stockY[i], b = stockY[i] - stockH;
            if (ndcX >= l && ndcX <= r && ndcY <= t && ndcY >= b) {
                selectedStock = i;
                return;
            }
        }
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
        if (ndcX >= sellX && ndcX <= sellX+sellW && ndcY <= sellY && ndcY >= sellY-sellH) {
            Stock* s = &stocks[selectedStock];
            if (s->qty > 0) {
                realizedPnL += (s->price - s->avgCost);
                cashBalance += s->price;
                s->qty -= 1;
                s->totalCost -= s->avgCost;
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

    if (key == GLFW_KEY_ESCAPE) {
        if (addModalOpen) {
            addModalOpen = false;
            addInputActive = false;
            return;
        }
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
        if (addModalOpen) {
            submitAddBalance();
            return;
        }
    }

    if (key == GLFW_KEY_BACKSPACE) {
        if (addModalOpen && addInputActive) {
            if (addInputLen > 0) { addInputText[--addInputLen] = '\0'; }
            return;
        }
        if (searchBarActive) {
            if (searchLen > 0) { searchText[--searchLen] = '\0'; }
            return;
        }
    }
}

static void char_callback(GLFWwindow *window, unsigned int codepoint) {
    // Numeric input for Add Balance modal
    if (addModalOpen && addInputActive) {
        if (codepoint == 8) { if (addInputLen > 0) { addInputText[--addInputLen] = '\0'; } return; }
        if ((codepoint >= '0' && codepoint <= '9') || codepoint == '.') {
            // allow only a single '.'
            if (codepoint == '.') {
                for (int i = 0; i < addInputLen; ++i) if (addInputText[i] == '.') return;
            }
            if (addInputLen < (int)sizeof(addInputText)-1) {
                addInputText[addInputLen++] = (char)codepoint;
                addInputText[addInputLen] = '\0';
            }
        }
        return;
    }

    // Search bar typing
    if (!searchBarActive) return;
    if (codepoint == 8) { if (searchLen > 0) { searchText[--searchLen] = '\0'; } return; }
    if (codepoint >= 32 && codepoint < 127) {
        if (searchLen < (int)sizeof(searchText)-1) {
            searchText[searchLen++] = (char)codepoint;
            searchText[searchLen] = '\0';
        }
    }
}

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

    float* src = (float*)buffer;
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

static float ndcToPixelX(float ndcX) { return (ndcX + 1.0f) * 0.5f * windowWidth; }
static float ndcToPixelY(float ndcY) { return (1.0f - ndcY) * 0.5f * windowHeight; }

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
static void updateCandleSeries(double now, float v) {
    if (currentCandle < 0) return;

    Candle* c = &candles[currentCandle];
    if (!c->valid) { c->open = c->high = c->low = c->close = v; c->valid = true; }
    c->close = v;
    if (v > c->high) c->high = v;
    if (v < c->low ) c->low  = v;

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
    if (windowWidth <= 0 || windowHeight <= 0) { upVertCount=dnVertCount=wickVertCount=0; return; }

    int count = candleCount;
    int first = (candleCount == MAX_CANDLES) ? (currentCandle + 1) % MAX_CANDLES : 0;

    float vmin =  1e30f, vmax = -1e30f;
    for (int i = 0; i < count; ++i) {
        int idx = (first + i) % MAX_CANDLES;
        if (!candles[idx].valid) continue;
        Candle c = candles[idx];
        if (c.low  < vmin) vmin = c.low;
        if (c.high > vmax) vmax = c.high;
    }
    if (!(vmax > vmin)) { upVertCount=dnVertCount=wickVertCount=0; return; }
    float pad = 0.05f * (vmax - vmin);
    vmin -= pad; vmax += pad;

    const float left   = chartLeftNDC;
    const float top    = chartTopNDC;
    const float width  = chartWidthNDC;
    const float height = chartHeightNDC;
    const float step   = width / (float)count;

    const float minBodyPx  = 2.0f;
    const float minBodyNDC = (minBodyPx * 2.0f) / (float)windowHeight;

    const float fill = 0.55f;
    float bodyW = step * fill;
    if (bodyW < minBodyNDC)  bodyW = minBodyNDC;
    if (bodyW > step * 0.95f) bodyW = step * 0.95f;

    int maxUpVerts   = count * 6;
    int maxDnVerts   = count * 6;
    int maxWickVerts = count * 2;

    float* up  = (float*)malloc(sizeof(float)*3*maxUpVerts);
    float* dn  = (float*)malloc(sizeof(float)*3*maxDnVerts);
    float* wck = (float*)malloc(sizeof(float)*3*maxWickVerts);
    if (!up || !dn || !wck) { free(up); free(dn); free(wck); upVertCount=dnVertCount=wickVertCount=0; return; }
    int upV=0, dnV=0, wkV=0;

    const float chartBottom = top - height;
    const float zBody = -0.25f;
    const float zWick = -0.20f;

    for (int i = 0; i < count; ++i) {
        int idx = (first + i) % MAX_CANDLES;
        if (!candles[idx].valid) continue;
        Candle c = candles[idx];

        float yO = mapYValue(c.open,  vmin, vmax, chartBottom, height);
        float yC = mapYValue(c.close, vmin, vmax, chartBottom, height);
        float yH = mapYValue(c.high,  vmin, vmax, chartBottom, height);
        float yL = mapYValue(c.low,   vmin, vmax, chartBottom, height);

        float xCenter = left + (float)i * step + step * 0.5f;
        float x0 = xCenter - bodyW * 0.5f;
        float x1 = xCenter + bodyW * 0.5f;

        if (wkV + 2 <= maxWickVerts) {
            wck[wkV*3+0] = xCenter; wck[wkV*3+1] = yL;  wck[wkV*3+2] = zWick; wkV++;
            wck[wkV*3+0] = xCenter; wck[wkV*3+1] = yH;  wck[wkV*3+2] = zWick; wkV++;
        }

        float yB = fminf(yO, yC);
        float yT = fmaxf(yO, yC);
        if ((yT - yB) < minBodyNDC) {
            float mid = 0.5f * (yT + yB);
            yB = mid - 0.5f * minBodyNDC;
            yT = mid + 0.5f * minBodyNDC;
        }

        float tri[18] = {
            x0,yB,zBody,  x1,yB,zBody,  x1,yT,zBody,
            x0,yB,zBody,  x1,yT,zBody,  x0,yT,zBody
        };

        if (c.close >= c.open) {
            if (upV + 6 <= maxUpVerts) { memcpy(up + upV*3, tri, sizeof(tri)); upV += 6; }
        } else {
            if (dnV + 6 <= maxDnVerts) { memcpy(dn + dnV*3, tri, sizeof(tri)); dnV += 6; }
        }
    }

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
