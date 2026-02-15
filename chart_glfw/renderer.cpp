#include "renderer.h"
#include "DataManager.h"
#include "event.h"

#include <unordered_map>



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







std::vector<float> Renderer::prepareCandleData(const std::string& filename) {
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

// New function: Prepare candle data from CandleData vector (from DataManager)
std::vector<float> Renderer::prepareCandleDataFromVector(const std::vector<CandleData>& candles) {
    if (candles.empty()) return {};

    std::vector<float> wickData;
    std::vector<float> bodyData;

    // Calculate price range
    minPrice = 1e9; maxPrice = -1e9;
    for (const auto& candle : candles) {
        minPrice = (std::min)(minPrice, (float)candle.low);
        maxPrice = (std::max)(maxPrice, (float)candle.high);
    }

    int i = 0;
    for (const auto& candle : candles) {
        float o = (float)candle.open;
        float h = (float)candle.high;
        float l = (float)candle.low;
        float c = (float)candle.close;
        float x = (float)i;

        // Color: green if close >= open, red otherwise
        float r = (c >= o) ? 0.0f : 1.0f;
        float g = (c >= o) ? 1.0f : 0.0f;

        // Wick vertices (2 vertices per wick line)
        wickData.insert(wickData.end(), { x, h, r, g, 0.0f, x, l, r, g, 0.0f });

        // Body vertices (6 vertices for 2 triangles = rectangle)
        float top = (std::max)(o, c);
        float bot = (std::min)(o, c);
        float w = 0.3f; // body width
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

unsigned int Renderer::createShaderProgram() {
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


void Renderer::createChartFramebuffer(ChartView& chart, int w, int h)
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

std::pair<GLuint, int>  Renderer::initCandleData(std::string jsonFile) {
    std::vector<float> candleVertices = this->prepareCandleData(jsonFile);
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

// New function: Initialize candle data from pre-prepared vertex vector
std::pair<GLuint, int> Renderer::initCandleDataFromVector(const std::vector<float>& candleVertices) {
    if (candleVertices.empty()) return { 0, 0 };

    // Setup VAO/VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, candleVertices.size() * sizeof(float), candleVertices.data(), GL_STATIC_DRAW);

    // Position attribute (2 floats: x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (3 floats: r, g, b)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    int numCandles = candleVertices.size() / ((2 + 6) * 5); // 8 vertices total per candle (2 wick + 6 body)

    return { VAO, numCandles };
}

void Renderer::renderChartToFBO(ChartView& chart, GLuint shaderProgram, GLuint VAO, int numCandles)
{
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
void Renderer::onScroll(double xoffset, double yoffset)
{
    zoomLevel -= static_cast<float>(yoffset) * 4.0f;

    if (zoomLevel < minZoom) zoomLevel = minZoom;
    if (zoomLevel > maxZoom) zoomLevel = maxZoom;
}


void Renderer::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto* renderer =
        static_cast<Renderer*>(glfwGetWindowUserPointer(window));

    if (renderer) {
        renderer->onScroll(xoffset, yoffset);
    }
}
std::atomic<bool> scanning(false);
std::atomic<bool> done(false);
std::vector<float> scan_results;

// Simulated long-running task
void scan_market()
{
    scanning = true;
    done = false;
    scan_results.clear();

    // Simulate work
    for (int i = 0; i < 10; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        scan_results.push_back(float(i) * 10.0f);  // fake data
    }

    scanning = false;
    done = true;
}


void ScannerUI() {
    ImGui::Begin("Market Scanner");

    if (!scanning && ImGui::Button("Start Scan"))
    {
        // Launch background task
        std::thread(scan_market).detach();
    }

    if (scanning)
    {
        ImGui::Text("Scanning in progress...");
        ImGui::ProgressBar(float(scan_results.size()) / 10.0f);
    }
    else if (done)
    {
        ImGui::Text("Scan complete!");
        for (auto val : scan_results)
        {
            ImGui::Text("Value: %.1f", val);
        }
    }

    ImGui::End();
}

#include "DataManager.h"

int Renderer::draw(DataManager& dataManager)
{
    // --- Start ImGui frame ---
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // --- ImGui UI ---
    //ImGui::Begin("Controls"); 
    //ImGui::Text("Use mouse scroll to zoom in/out");
    //ImGui::End();
//      //ImGui::ShowDemoWindow();

    ImGuiIO& io = ImGui::GetIO();

    // Only capture if no widget has focus and no text input is active
    if (!io.WantCaptureKeyboard || !ImGui::IsAnyItemActive()) {
        // Listen for alphanumeric keys
        for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
            if (ImGui::IsKeyPressed((ImGuiKey)key)) {
                m_isCapturingSymbol = true;
                char c = 'A' + (key - ImGuiKey_A);
                m_symbolBuffer += c;
            }
        }
        for (int key = ImGuiKey_0; key <= ImGuiKey_9; key++) {
            if (ImGui::IsKeyPressed((ImGuiKey)key)) {
                m_isCapturingSymbol = true;
                char c = '0' + (key - ImGuiKey_0);
                m_symbolBuffer += c;
            }
        }

        // Backspace to delete
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !m_symbolBuffer.empty()) {
            m_symbolBuffer.pop_back();
        }

        // Enter to submit
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !m_symbolBuffer.empty()) {
            if (onSymbolEntered) {
                onSymbolEntered(m_symbolBuffer);
            }
            m_symbolBuffer.clear();
            //m_isCapturingSymbol = false;
        }

        // Escape to cancel
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_symbolBuffer.clear();
            //m_isCapturingSymbol = false;
        }
    }

    // Show symbol input overlay (like TC2000's ticker box)
    if (m_isCapturingSymbol && !m_symbolBuffer.empty()) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 workPos = viewport->WorkPos;
        ImVec2 workSize = viewport->WorkSize;

        // Position at top-center
        ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x * 0.5f, workPos.y + 50), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.8f);

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        ImGui::Begin("##SymbolOverlay", nullptr, windowFlags);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green text
        ImGui::SetWindowFontScale(3.0f);
        ImGui::Text("%s", m_symbolBuffer.c_str());
        ImGui::PopStyleColor();
        ImGui::End();
    }

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

    //for (auto& chart : charts) {
    //    CreateChartView(chart);
    //}

    //ImGui::Begin("Mini Chart");

    //ImGui::InputText("Symbol", symbolInput, IM_ARRAYSIZE(symbolInput));
    //if (ImGui::IsItemDeactivatedAfterEdit()) { // Enter pressed
    //    requestData = true;
    //    currentSymbol = symbolInput;
    //}

    //ImGui::SameLine();
    //if (ImGui::Button("Load")) {
    //    requestData = true;
    //    currentSymbol = symbolInput;
    //}



    //ImGui::Separator();

    //// Chart placeholder
    //ImGui::Text("Symbol: %s", currentSymbol.c_str());
    //ImGui::Text("Chart goes here...");

    //ScannerUI();


	// Scanner Results Window
	ImGui::Begin("Market Scanner Results");

	const auto& scanResults = dataManager.currentScannerResult;

	ImGui::Text("Request ID: %d", scanResults.reqId);
	ImGui::Text("Total Results: %zu", scanResults.items.size());
	ImGui::Separator();

	if (ImGui::BeginTable("ScannerTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
		// Setup columns
		ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Currency", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Contract ID", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		// Display each result
		for (const auto& item : scanResults.items) {
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", item.rank);

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", item.symbol.c_str());

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s", item.secType.c_str());

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%s", item.currency.c_str());

			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%ld", item.conId);
		}

		ImGui::EndTable();
	}

	ImGui::End();

	// Display charts from DataManager
	for (auto& [symbol, chartData] : dataManager.charts) {
		// Create chart if it doesn't exist
		if (m_chartViews.find(symbol) == m_chartViews.end()) {
			m_chartViews[symbol] = createChartFromData(symbol, chartData.candles);
		}

		// Display the chart window
		CreateChartView(m_chartViews[symbol]);
	}



	// 2. Create the UI Window and Button
	ImGui::Begin("Control Panel");

	// TC2000-style symbol input
	static char symbolBuffer[32] = "";
	ImGui::Text("Enter Symbol:");
	ImGui::SameLine();

	// Input text with enter detection
	if (ImGui::InputText("##symbol", symbolBuffer, IM_ARRAYSIZE(symbolBuffer), 
						 ImGuiInputTextFlags_EnterReturnsTrue)) {
		// User pressed Enter!
		if (strlen(symbolBuffer) > 0 && onSymbolEntered) {
			std::string symbol = symbolBuffer;
			// Convert to uppercase
			std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
			onSymbolEntered(symbol);

			// Clear input
			symbolBuffer[0] = '\0';
		}
	}

	// Keep focus on input for quick typing
	if (ImGui::IsWindowAppearing()) {
		ImGui::SetKeyboardFocusHere(-1);
	}

	if (ImGui::Button("Click Me!")) {
		std::cout << "Button was clicked!" << std::endl;
	}

	ImGui::End();



    ImGui::Render();



    // --- Render ImGui LAST ---

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return 0;
}

ChartView Renderer::createChartFromData(const std::string& symbol, const std::vector<CandleData>& candles) {
    printf("Creating new chart view for symbol: %s with %zu candles\n", 
           symbol.c_str(), candles.size());

    ChartView newChart;
    newChart.title = symbol.c_str();
    newChart.isVisible = true;

    // Prepare vertex data from candles
    std::vector<float> vertexData = prepareCandleDataFromVector(candles);

    // Initialize OpenGL objects
    auto [vao, numCandles] = initCandleDataFromVector(vertexData);
    newChart.vao = vao;
    newChart.numCandles = numCandles;
    newChart.shaderProgram = createShaderProgram();

    // Create initial FBO (will be resized in CreateChartView if needed)
    createChartFramebuffer(newChart, 800, 600);

    return newChart;
}

void Renderer::CreateChartView(ChartView& chart)
{
    ImGui::Begin(chart.title, &chart.isVisible);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 0 && avail.y > 0)
    {
        // Optional: resize FBO if ImGui window resized
        if ((int)avail.x != chart.width || (int)avail.y != chart.height)
        {
            glDeleteTextures(1, &chart.colorTex);
            glDeleteFramebuffers(1, &chart.fbo);
            createChartFramebuffer(chart, (int)avail.x, (int)avail.y);
        }

        renderChartToFBO(chart, chart.shaderProgram, chart.vao, chart.numCandles);

        // Show FBO texture inside ImGui
        ImGui::Image((void*)(intptr_t)chart.colorTex, avail, ImVec2(0, 1), ImVec2(1, 0));
    }

    ImGui::End();
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void Renderer::processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void Renderer::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
void Renderer::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}
Renderer::Renderer(){}

Renderer::~Renderer() {
}

