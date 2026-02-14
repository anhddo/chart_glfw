#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define NOMINMAX
#include <windows.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cpr/cpr.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>                  // Core GLM types
#include <glm/gtc/matrix_transform.hpp> // For glm::ortho
#include <glm/gtc/type_ptr.hpp>         // For glm::value_ptr
using json = nlohmann::json;

#include <algorithm>
#include <vector>

struct CandleVertex {
    float x, y;
    float r, g, b;
};
struct ChartView {
    GLuint fbo = 0;
	GLuint shaderProgram = 0;
	GLuint vao = 0;
    GLuint numCandles;
    GLuint colorTex = 0;
    int width = 0;
    int height = 0;
	//std::string title;
    const char* title;

	bool isVisible = true;
    
};

struct Candle {
    float open, high, low, close;
};
class Renderer
{
public:
    Renderer();
    ~Renderer();
    void init(GLFWwindow* window);
    int draw(class DataManager& dataManager);
    void CreateChartView(ChartView& aaplChart);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);


private:
    GLuint VAO, VBO;
    std::vector<CandleVertex> vertices;


    // 2. Zooming Variables
    float zoomLevel = 20.0f;    // Number of candles visible (lower = zoomed in)
    float minZoom = 5.0f;       // Maximum zoom in
    float maxZoom = 100.0f;     // Maximum zoom out (adjust based on data size)

    // We use this to tell OpenGL which candle should be at the right edge
    int lastCandleIndex = 0;
    float minPrice = 1e9, maxPrice = -1e9;


    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    std::vector<float> prepareCandleData(const std::string& filename);
    std::pair<GLuint, int>  initCandleData(std::string jsonFile);
    unsigned int createShaderProgram();

    void onScroll(double xoffset, double yoffset);
    void createChartFramebuffer(ChartView& chart, int w, int h);
    void renderChartToFBO(ChartView& chart, GLuint shaderProgram, GLuint VAO, int numCandles);


    // process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
    // ---------------------------------------------------------------------------------------------------------
    // glfw: whenever the window size changed (by OS or user resize) this callback function executes
    // ---------------------------------------------------------------------------------------------
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void processInput(GLFWwindow* window);


};
