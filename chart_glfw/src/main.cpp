#ifdef _WIN32
// This tells the linker to use main() even if we are in Windows Subsystem mode
#pragma comment(linker, "/entry:mainCRTStartup")
#endif
//
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

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 2560;
const unsigned int SCR_HEIGHT = 1280;


GLuint VAO, VBO;

struct CandleVertex {
    float x, y;
    float r, g, b;
};
std::vector<CandleVertex> vertices;

struct Candle {
    float open, high, low, close;
};

// Global scaling variables
float minPrice = 1e9, maxPrice = -1e9;
// 1. Updated Shaders to support Scaling and Color
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec2 aPos;\n" // Position
"layout (location = 1) in vec3 aColor;\n" // Color
"uniform mat4 projection;\n"
"out vec3 ourColor;\n"
"void main() {\n"
"   gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
"   ourColor = aColor;\n"
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"in vec3 ourColor;\n"
"void main() {\n"
"   FragColor = vec4(ourColor, 1.0f);\n"
"}\n\0";




// 2. Zooming Variables
float zoomLevel = 20.0f;    // Number of candles visible (lower = zoomed in)
float minZoom = 5.0f;       // Maximum zoom in
float maxZoom = 100.0f;     // Maximum zoom out (adjust based on data size)

// We use this to tell OpenGL which candle should be at the right edge
int lastCandleIndex = 0;


void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    // Scroll up (yoffset > 0) to zoom in, down to zoom out
    zoomLevel -= (float)yoffset * 4.0f;

    // Clamp the zoom so it doesn't go below 1 or above total data
    if (zoomLevel < minZoom) zoomLevel = minZoom;
    if (zoomLevel > maxZoom) zoomLevel = maxZoom;
}

std::vector<float> prepareCandleData(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) return {};
    json fullData = json::parse(f);
    auto& data = fullData["results"];

    std::vector<float> wickData;
    std::vector<float> bodyData;

    // --- ADD THIS BACK ---
    minPrice = 1e9; maxPrice = -1e9;
    for (auto& item : data) {
        minPrice = (std::min)(minPrice, (float)item["l"]);
        maxPrice = (std::max)(maxPrice, (float)item["h"]);
    }
    // ---------------------

    int i = 0;
    for (auto& item : data) {
        float o = item["o"], h = item["h"], l = item["l"], c = item["c"];
        float x = (float)i;
        float r = (c >= o) ? 0.0f : 1.0f;
        float g = (c >= o) ? 1.0f : 0.0f;

        wickData.insert(wickData.end(), { x, h, r, g, 0.0f, x, l, r, g, 0.0f });

        float top = (std::max)(o, c), bot = (std::min)(o, c);
        float w = 0.3f;
        bodyData.insert(bodyData.end(), {
            x - w, bot, r, g, 0.0f,  x + w, bot, r, g, 0.0f,  x + w, top, r, g, 0.0f,
            x - w, bot, r, g, 0.0f,  x + w, top, r, g, 0.0f,  x - w, top, r, g, 0.0f
            });
        i++;
    }

    std::vector<float> totalData = wickData;
    totalData.insert(totalData.end(), bodyData.begin(), bodyData.end());
    return totalData;
}

unsigned int createShaderProgram() {
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertexShaderSource, NULL);
    glCompileShader(vertex);

    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragment);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

struct ChartView {
    GLuint fbo = 0;
    GLuint colorTex = 0;
    int width = 0;
    int height = 0;
};

void createChartFramebuffer(ChartView& chart, int w, int h)
{
    chart.width = w;
    chart.height = h;

    // Color texture
    glGenTextures(1, &chart.colorTex);
    glBindTexture(GL_TEXTURE_2D, chart.colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // FBO
    glGenFramebuffers(1, &chart.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, chart.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, chart.colorTex, 0);

    // Depth/stencil buffer (optional for 3D)
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

std::pair<GLuint, int>  initCandleData(std::string jsonFile) {
    std::vector<float> candleVertices = prepareCandleData(jsonFile);
    int candleCount = candleVertices.size() / (5 * 8); // 8 vertices per candle (2 wick + 6 body)

    // Setup VAO/VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, candleVertices.size() * sizeof(float), candleVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    int numCandles = candleVertices.size() / ((2 + 6) * 5); // 8 vertices total per candle
    
	return { VAO, candleCount };

}

void renderChartToFBO(ChartView& chart, GLuint VAO, int numCandles)
{


    // Build Shader (use the same source from previous message)
    unsigned int shaderProgram = createShaderProgram();


    glBindFramebuffer(GL_FRAMEBUFFER, chart.fbo);
    glViewport(0, 0, chart.width, chart.height);

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    int wickVertexCount = numCandles * 2;
    int bodyVertexCount = numCandles * 6;

    // lastCandleIndex should be the index of your newest data point
    float rightEdge = (float)numCandles;
    float leftEdge = rightEdge - zoomLevel;

    // Projection: [Left, Right, Bottom, Top]
    // Notice how rightEdge is constant, only leftEdge moves!
    glm::mat4 projection = glm::ortho(leftEdge, rightEdge, minPrice - 2.0f, maxPrice + 2.0f);

    int projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));





    // 1. Draw ALL wicks at once
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, wickVertexCount);

    // 2. Draw ALL bodies at once
    // The 'wickVertexCount' tells OpenGL to start drawing AFTER the wicks in the buffer
    glDrawArrays(GL_TRIANGLES, wickVertexCount, bodyVertexCount);



    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Done rendering to texture


}



int glfw_test()
{


    // Setup Platform/Renderer backends
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    //GLFWwindow* window = glfwCreateWindow(1280, 800, "Chart", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    //glfwMaximizeWindow(window);

    //glfwMakeContextCurrent(window);
    //glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwSetScrollCallback(window, scroll_callback);

	glfwSwapInterval(1);// Enable V-Sync for smoother rendering

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
	ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");


    std::vector<float> candleVertices = prepareCandleData("be_data.json");
    int candleCount = candleVertices.size() / (5 * 8); // 8 vertices per candle (2 wick + 6 body)
	auto [VAO, numCandles] = initCandleData("be_data.json");

    // Build Shader (use the same source from previous message)
    unsigned int shaderProgram = createShaderProgram();
	bool show_nvda = true;
	bool show_aapl = true;
    ChartView aaplChart;  // contains fbo & colorTex

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();


        // --- Start ImGui frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- ImGui UI ---
		//ImGui::Begin("Controls"); 
		//ImGui::Text("Use mouse scroll to zoom in/out");
		//ImGui::End();
  //      //ImGui::ShowDemoWindow();

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse;

        ImGui::Begin("RootDock", nullptr, flags);
        ImGui::DockSpace(ImGui::GetID("DockSpace"));
        ImGui::End();

        ImGui::Begin("NVDA Chart", &show_aapl);
        ImGui::Text("Another chart");
        ImGui::End();


        ImGui::Begin("AAPL Chart", &show_aapl);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x > 0 && avail.y > 0)
        {
            // Optional: resize FBO if ImGui window resized
            if ((int)avail.x != aaplChart.width || (int)avail.y != aaplChart.height)
            {
                glDeleteTextures(1, &aaplChart.colorTex);
                glDeleteFramebuffers(1, &aaplChart.fbo);
                createChartFramebuffer(aaplChart, (int)avail.x, (int)avail.y);
            }

            renderChartToFBO(aaplChart, VAO, numCandles);

            // Show FBO texture inside ImGui
            ImGui::Image((void*)(intptr_t)aaplChart.colorTex, avail, ImVec2(0, 1), ImVec2(1, 0));
        }

        ImGui::End();



        ImGui::Render();



        // --- Render ImGui LAST ---
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

    }
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}


// Helper function to convert Unix Milliseconds to a readable string
std::string formatTimestamp(long long ms) {
    time_t seconds = ms / 1000;
    struct tm lt;
    localtime_s(&lt, &seconds); // Use localtime_s on Windows for safety

    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &lt);
    return std::string(buffer);
}
int  readfile() {
    // 1. Open the file
    std::ifstream inFile("aapl_data.json");
    if (!inFile.is_open()) {
        std::cerr << "Could not open aapl_data.json" << std::endl;
        return 1;
    }

    // 2. Parse the JSON
    json data;
    inFile >> data;
    inFile.close();

    // 3. Navigate to the 'results' array (Common structure for Polygon/Massive APIs)
    if (data.contains("results") && data["results"].is_array()) {

        std::cout << std::left << std::setw(22) << "Time"
            << std::setw(10) << "Open"
            << std::setw(10) << "High"
            << std::setw(10) << "Low"
            << std::setw(10) << "Close" << std::endl;
        std::cout << std::string(62, '-') << std::endl;

        for (const auto& bar : data["results"]) {
            // Mapping: t=time, o=open, h=high, l=low, c=close
            long long timestamp = bar["t"];
            double o = bar["o"];
            double h = bar["h"];
            double l = bar["l"];
            double c = bar["c"];

            std::cout << std::left << std::setw(22) << formatTimestamp(timestamp)
                << std::setw(10) << o
                << std::setw(10) << h
                << std::setw(10) << l
                << std::setw(10) << c << std::endl;
        }
    }
    else {
        std::cout << "No OHLC data found in the 'results' field." << std::endl;
    }
}

int download_file() {
    // 1. Define the URL and your query parameters
    // The URL includes the ticker (AAPL) and the date range
    std::string url = "https://api.massive.com/v2/aggs/ticker/BE/range/1/day/2026-01-01/2026-02-06?adjusted=true&sort=asc&limit=120&apiKey=5l6xbNcmA4zoaRFnfuUNWjKEViL0n3Hf";

    cpr::Response r = cpr::Get(cpr::Url{ url });

    if (r.status_code == 200) {
        // 2. Open a file for writing
        // This will create the file if it doesn't exist, or overwrite it if it does
        std::ofstream outFile("be_data.json");

        if (outFile.is_open()) {
            // 3. Write the response text (JSON) to the file
            outFile << r.text;

            // 4. Close the file
            outFile.close();

            std::cout << "Success! Data saved to aapl_data.json" << std::endl;
        }
        else {
            std::cerr << "Error: Could not create or open the file for writing." << std::endl;
        }
    }
    else {
        std::cerr << "API Request failed. Status code: " << r.status_code << std::endl;
        std::cerr << "Error message: " << r.error.message << std::endl;
    }

    return 0;
}
int main() {
	//readfile();
	glfw_test();
}