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
#include <functional>
#include <unordered_map>
#include <string>

// Forward declarations
struct CandleData;
struct ScannerResult;
struct DataManager;

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

	// Per-chart price range (not global!)
	float minPrice = 1e9f;
	float maxPrice = -1e9f;

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
    void ScannerGUI(const ScannerResult& scanResults);
    void OverlayTickerGUI();
    void DrawChartGUI(DataManager& dataManager);
    int draw(class DataManager& dataManager);
    void CreateChartView(ChartView& aaplChart);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Callback for symbol input
    std::function<void(const std::string&)> onSymbolEntered;

private:
    // TC2000-style global symbol capture
    std::string m_symbolBuffer;
    bool m_isCapturingSymbol = false;
    GLuint VAO, VBO;
    std::vector<CandleVertex> vertices;

    // Chart management
    std::unordered_map<std::string, ChartView> m_chartViews;


    // 2. Zooming Variables
    float zoomLevel = 20.0f;    // Number of candles visible (lower = zoomed in)
    float minZoom = 5.0f;       // Maximum zoom in
    float maxZoom = 100.0f;     // Maximum zoom out (adjust based on data size)

    // We use this to tell OpenGL which candle should be at the right edge
    int lastCandleIndex = 0;
    // Note: minPrice/maxPrice are now per-chart in ChartView struct, not global


    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    std::vector<float> prepareCandleDataFromJson(const std::string& filename);
    std::pair<std::vector<float>, std::pair<float, float>> prepareCandleDataFromVector(const std::vector<struct CandleData>& candles);
    std::pair<GLuint, int> initCandleDataFromJson(std::string jsonFile);
    std::pair<GLuint, int> initCandleDataFromVector(const std::vector<float>& candleVertices);
    unsigned int createShaderProgram();

    void onScroll(double xoffset, double yoffset);
    void createChartFrameBuffer(ChartView& chart, int w, int h);
    void renderChartToFBO(ChartView& chart, GLuint shaderProgram, GLuint VAO, int numCandles);
    ChartView createChartFromData(const std::string& symbol, const std::vector<struct CandleData>& candles);


    // process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
    // ---------------------------------------------------------------------------------------------------------
    // glfw: whenever the window size changed (by OS or user resize) this callback function executes
    // ---------------------------------------------------------------------------------------------
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void processInput(GLFWwindow* window);


};
