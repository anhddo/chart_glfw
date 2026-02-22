#include "renderer.h"
#include "DataManager.h"
#include "event.h"

#include <thread>
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

Renderer::Renderer() {}

Renderer::~Renderer() {
    for (auto& [symbol, chart] : m_chartViews) {
        chart.cleanup();
    }
    m_chartViews.clear();
}

void Renderer::DisableTitleFocusColors() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_TitleBg];
    style.Colors[ImGuiCol_TitleBgCollapsed] = style.Colors[ImGuiCol_TitleBg];
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


std::vector<float> Renderer::prepareCandleDataFromJson(const std::string& filename) {
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

std::pair<GLuint, int>  Renderer::initCandleDataFromJson(std::string jsonFile) {
    std::vector<float> candleVertices = this->prepareCandleDataFromJson(jsonFile);
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


// New function: Prepare candle data from CandleData vector (from DataManager)
// Returns both vertex data AND price range as a pair
std::pair<std::vector<float>, std::pair<float, float>> Renderer::prepareCandleDataFromVector(const std::vector<CandleData>& candles) {
    if (candles.empty()) return {{}, {1e9f, -1e9f}};

    std::vector<float> wickData;
    std::vector<float> bodyData;

    // Calculate price range (local variables, not global!)
    float localMinPrice = 1e9f;
    float localMaxPrice = -1e9f;
    for (const auto& candle : candles) {
        localMinPrice = (std::min)(localMinPrice, (float)candle.low);
        localMaxPrice = (std::max)(localMaxPrice, (float)candle.high);
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
        //wickData.insert(wickData.end(), { x, h, r, g, 0.0f, x, l, r, g, 0.0f });//wick has the same color with bar
        wickData.insert(wickData.end(), { x, h, 1.0f, 1.0f, 1.0f, x, l, 1.0f, 1.0f, 1.0f});// white wick

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
    return {totalData, {localMinPrice, localMaxPrice}};
}



void Renderer::createChartFrameBuffer(ChartView& chart, int w, int h)
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

    //// Depth/stencil buffer (optional for 3D)
    //GLuint rbo;
    //glGenRenderbuffers(1, &rbo);
    //glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// New function: Initialize candle data from pre-prepared vertex vector
std::tuple<GLuint, GLuint, int> Renderer::initCandleDataFromVector(const std::vector<float>& candleVertices) {
    if (candleVertices.empty()) return { 0, 0,0 };

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

    return { VAO, VBO, numCandles };
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
    // Use THIS chart's price range, not global!
    glm::mat4 projection = glm::ortho(leftEdge, rightEdge, chart.minPrice - 2.0f, chart.maxPrice + 2.0f);

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

void Renderer::ScannerGUI(const ScannerResult& scanResults)
{
    ImGui::Begin("Market Scanner Results");

    DisableTitleFocusColors();
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

            // Invisible selectable spanning all columns for row-level hover/click
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(item.rank);  // Unique ID per row
            bool rowClicked = ImGui::Selectable("##row", false, 
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
            ImGui::PopID();

            // Render actual content over the selectable
            ImGui::SameLine();
            ImGui::Text("%d", item.rank);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", item.symbol.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", item.secType.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", item.currency.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%ld", item.conId);

            // Optional: Handle row click
            if (rowClicked) {
                printf("Clicked row: %s\n", item.symbol.c_str());
                if (onScannerRowClicked) {
                    onScannerRowClicked(item.symbol);
				}
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void Renderer::OverlayTickerGUI()
{
    ImGuiIO& io = ImGui::GetIO();

    // Global ticker capture - only skip if we're actively editing the Control Panel text input
    // Check if Control Panel input is focused (we'll need to track this)
    bool shouldCapture = !io.WantTextInput;  // Don't capture only when ImGui wants text input

    if (shouldCapture) {
        // Listen for alphanumeric keys
        for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
            if (ImGui::IsKeyPressed((ImGuiKey)key)) {
                m_isCapturingSymbol = true;
                char c = 'A' + (key - ImGuiKey_A);
                m_symbolBuffer += c;
                printf("Captured key: %c, buffer now: %s\n", c, m_symbolBuffer.c_str());
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
            m_isCapturingSymbol = false;
        }

        // Escape to cancel
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_symbolBuffer.clear();
            m_isCapturingSymbol = false;
        }
    }

    // Show symbol input overlay (like TC2000's ticker box)
    if (m_isCapturingSymbol && !m_symbolBuffer.empty()) {

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 workPos = viewport->WorkPos;
        ImVec2 workSize = viewport->WorkSize;

        // Position at top-center - ALWAYS force position
        ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x * 0.5f, workPos.y + 50), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);  // Force not collapsed
        ImGui::SetNextWindowFocus();  // Force focus to ensure visibility

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoDocking;  // Prevent docking which can hide it

        // Use a bool to prevent ImGui from closing the window
        bool isOpen = true;
        if (ImGui::Begin("##SymbolOverlay", &isOpen, windowFlags)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green text
            ImGui::SetWindowFontScale(3.0f);
            ImGui::Text("%s", m_symbolBuffer.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::End();

        printf("Overlay rendered: buffer='%s', isCapturing=%d\n", m_symbolBuffer.c_str(), m_isCapturingSymbol);
    }
}

void Renderer::DrawChartGUI(DataManager& dataManager)
{
    // Display charts from DataManager
    for (auto& [symbol, chartData] : dataManager.charts) {
        // Create chart if it doesn't exist
        if (m_chartViews.find(symbol) == m_chartViews.end()) {
            m_chartViews[symbol] = createChartFromData(symbol, chartData.candles);
        }


        // Only display if visible
        if (m_chartViews[symbol].isVisible) {
            CreateChartView(m_chartViews[symbol]);
        }
    }
}

void Renderer::DockSetting()
{
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("RootDock", nullptr, flags);
    ImGui::DockSpace(ImGui::GetID("DockSpace"));
    ImGui::End();
}

void Renderer::TabTest(DataManager& dataManager)
{
    enum LayoutType { MARKET_OVERVIEW, TRADING_VIEW, PORTFOLIO };
    static LayoutType currentLayout = MARKET_OVERVIEW;

    // 2. Main Loop

    // --- Main Workspace Area ---
    ImGui::Begin("MainWorkspace", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    if (currentLayout == MARKET_OVERVIEW) {
        //RenderMarketLayout(); // Your charts/scanners
        ScannerGUI(dataManager.currentScannerResult);
    }
    else if (currentLayout == TRADING_VIEW) {
        DrawChartGUI(dataManager); // Your charts
    }
    ImGui::End();

    // --- The Bottom Tab Bar (TC2000 Style) ---
    ImGui::Begin("BottomTabs", nullptr, ImGuiWindowFlags_NoTitleBar);
    if (ImGui::BeginTabBar("LayoutTabs")) {
        if (ImGui::BeginTabItem("Market")) {
            currentLayout = MARKET_OVERVIEW;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Trading")) {
            currentLayout = TRADING_VIEW;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void Renderer::RenderTradingWindows(DataManager& dataManager)
{
	// ========== IMPORTANT: How windows dock into tabs ==========
	// When you call ImGui::Begin() INSIDE a tab's content area (after DockSpace()),
	// those windows automatically become dockable ONLY within that tab's dockspace.
	// 
	// The "##Trading" suffix creates unique IDs, so you can have windows with
	// the same base name in different tabs without conflicts.

	// Chart Window - display active symbol
	std::string symbol = dataManager.activeSymbol;
	if (!symbol.empty() && dataManager.charts.find(symbol) != dataManager.charts.end()) {
		if (m_chartViews.find(symbol) == m_chartViews.end()) {
			m_chartViews[symbol] = createChartFromData(symbol, dataManager.charts[symbol].candles);
		}
		if (m_chartViews[symbol].isVisible) {
			CreateChartView(m_chartViews[symbol]);  // This window will dock in Trading tab
		}
	}

	// Order Entry Window
	// This window will ONLY be visible and dockable in the Trading tab
	static int quantity = 100;
	static char orderSymbol[64] = "";
	ImGui::Begin("Order Entry##Trading");  // ##Trading makes ID unique to this tab
	ImGui::Text("Quick Trade");
	ImGui::InputText("Symbol", orderSymbol, 64);
	ImGui::InputInt("Quantity", &quantity);
	if (ImGui::Button("Buy")) { /* TODO: Implement order */ }
	ImGui::SameLine();
	if (ImGui::Button("Sell")) { /* TODO: Implement order */ }
	ImGui::End();

	// Market Depth Window
	// Another window exclusive to the Trading tab
	ImGui::Begin("Market Depth##Trading");
	ImGui::Text("Market depth data will be displayed here");
	// TODO: Display market depth from dataManager
	ImGui::End();
}
void Renderer::RenderAnalysisWindows(DataManager& dataManager)
{
    // ========== Analysis Tab Windows ==========
    // These windows are independent from the Trading tab windows
    // They have their own docking layout within the "AnalysisDockSpace"

    // Scanner Results - shows market scanner data
    ScannerGUI(dataManager.currentScannerResult);

    // Technical Indicators Window
    ImGui::Begin("Technical Indicators##Analysis");  // ##Analysis for unique ID
    ImGui::Text("Technical analysis tools");
    // TODO: RSI, MACD, Bollinger Bands, etc.
    ImGui::End();

    // Backtest Results Window
    ImGui::Begin("Backtest Results##Analysis");
    ImGui::Text("Backtest statistics");
    // TODO: Display backtest performance metrics
    ImGui::End();

    // Strategy Editor Window
    ImGui::Begin("Strategy Editor##Analysis");
    ImGui::Text("Strategy development");
    // TODO: Code editor for creating trading strategies
    ImGui::End();

    // ========== KEY POINT ==========
    // Even though we're calling ImGui::Begin() just like in RenderTradingWindows(),
    // these windows will dock into the ANALYSIS tab's dockspace, not the Trading tab!
    // This is because they're created INSIDE the "Analysis" BeginTabItem() block.
}
void Renderer::Portfolio(DataManager& dataManager)
{
    // ========== Portfolio Tab Windows ==========
    // Display account information and positions

    // Account Summary Window
    ImGui::Begin("Account Summary##Portfolio");
    ImGui::Text("Account Information");
    ImGui::Separator();

    // Display key account values
    if (dataManager.accountData.accountValues.find("NetLiquidation") != dataManager.accountData.accountValues.end()) {
        const auto& netLiq = dataManager.accountData.accountValues["NetLiquidation"];
        ImGui::Text("Net Liquidation: %s %s", netLiq.value.c_str(), netLiq.currency.c_str());
    }
    if (dataManager.accountData.accountValues.find("AvailableFunds") != dataManager.accountData.accountValues.end()) {
        const auto& availFunds = dataManager.accountData.accountValues["AvailableFunds"];
        ImGui::Text("Available Funds: %s %s", availFunds.value.c_str(), availFunds.currency.c_str());
    }
    if (dataManager.accountData.accountValues.find("BuyingPower") != dataManager.accountData.accountValues.end()) {
        const auto& buyPower = dataManager.accountData.accountValues["BuyingPower"];
        ImGui::Text("Buying Power: %s %s", buyPower.value.c_str(), buyPower.currency.c_str());
    }

    ImGui::Separator();

    // Display all account values in a table
    if (ImGui::CollapsingHeader("All Account Values")) {
        if (ImGui::BeginTable("AccountValuesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Currency", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();

            for (const auto& [key, val] : dataManager.accountData.accountValues) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", key.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", val.value.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", val.currency.c_str());
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();

    // Positions Window
    ImGui::Begin("Positions##Portfolio");
    ImGui::Text("Current Positions (%zu)", dataManager.accountData.positions.size());
    ImGui::Separator();

    if (ImGui::BeginTable("PositionsTable", 7, 
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        // Setup columns
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Avg Cost", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Mkt Price", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Mkt Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Unreal P&L", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Real P&L", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (const auto& pos : dataManager.accountData.positions) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", pos.symbol.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.0f", pos.position);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", pos.averageCost);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", pos.marketPrice);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", pos.marketValue);

            ImGui::TableSetColumnIndex(5);
            // Color code P&L: green for profit, red for loss
            if (pos.unrealizedPNL >= 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            }
            ImGui::Text("%.2f", pos.unrealizedPNL);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(6);
            if (pos.realizedPNL >= 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            }
            ImGui::Text("%.2f", pos.realizedPNL);
            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }
    ImGui::End();
}
void Renderer::newGUI(DataManager& dataManager) {
	// ========== STEP 1: Create a fullscreen "host" window ==========
	// This window will contain our tab bar and dockspaces.
	// Think of this as the main Excel window frame.
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);        // Position at top-left of screen
	ImGui::SetNextWindowSize(viewport->Size);      // Fill entire screen
	ImGui::SetNextWindowViewport(viewport->ID);    // Attach to main viewport

	// Window flags to make it behave like a background container:
	// - NoDocking: Prevents THIS window from being docked (it's the host!)
	// - NoTitleBar/NoCollapse/NoResize/NoMove: Makes it look like part of the app, not a floating window
	// - NoBringToFrontOnFocus/NoNavFocus: Keeps it in the background
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
									ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
									ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
									ImGuiWindowFlags_NoNavFocus;

	// Remove padding so content fills entire window
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("MainDockSpaceWindow", nullptr, window_flags);
	ImGui::PopStyleVar();

	// ========== STEP 2: Create Excel-like sheet tabs ==========
	// TabBar creates the horizontal tab strip (like Excel sheet tabs at bottom)
	// Reorderable: Users can drag tabs to reorder them
	// AutoSelectNewTabs: New tabs are automatically selected when created
	if (ImGui::BeginTabBar("WorkbookTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
	{
		// ========== STEP 3: Create individual sheet tabs ==========
		// Each BeginTabItem() creates one clickable tab
		// When a tab is selected, the code inside its block runs

		// Sheet 1: Trading View
		if (ImGui::BeginTabItem("Trading"))
		{
			// ========== STEP 4: Create a unique DockSpace for this tab ==========
			// KEY CONCEPT: Each tab needs its OWN dockspace with a UNIQUE ID
			// This is what allows independent docking layouts per sheet!
			// 
			// Without unique IDs, all tabs would share the same dock layout,
			// and moving a window in one tab would affect all other tabs.
			ImGuiID dockspace_id = ImGui::GetID("TradingDockSpace");  // Unique ID for this sheet

			// DockSpace creates an invisible docking area that fills available space
			// ImVec2(0,0) means "use all available space in this tab"
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

			// ========== STEP 5: Render windows for this sheet ==========
			// Any ImGui::Begin() windows created here will:
			// 1. Be dockable within THIS tab's dockspace
			// 2. Persist their position when switching between tabs
			// 3. NOT appear in other tabs' dockspaces
			RenderTradingWindows(dataManager);

			ImGui::EndTabItem();  // Close this tab's content area
		}

		// Sheet 2: Analysis View
		if (ImGui::BeginTabItem("Analysis"))
		{
			// Same structure as Trading tab, but with DIFFERENT dockspace ID
			// This creates a completely separate docking environment
			ImGuiID dockspace_id = ImGui::GetID("AnalysisDockSpace");  // Different ID!
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
			RenderAnalysisWindows(dataManager);
			ImGui::EndTabItem();
		}
        if (ImGui::BeginTabItem("Portfolio"))
        {
            // Same structure as Trading tab, but with DIFFERENT dockspace ID
            // This creates a completely separate docking environment
            ImGuiID dockspace_id = ImGui::GetID("PortfolioDockSpace");  // Different ID!
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
            Portfolio(dataManager);  // Call Portfolio instead of RenderAnalysisWindows
            ImGui::EndTabItem();
        }

		// ========== STEP 6: Optional "+" button to add new sheets ==========
		// TabItemButton creates a small button in the tab bar
		// Trailing: Places it at the end of the tab bar
		if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
		{
			// TODO: Add logic to create new sheet dynamically
			// Would need to store tab names and dockspace IDs in a vector
		}

		ImGui::EndTabBar();  // Close the tab bar
	}

	ImGui::End();  // Close the main host window

	// ========== HOW IT ALL WORKS TOGETHER ==========
	// 1. Main window provides the container
	// 2. TabBar creates the tab strip UI
	// 3. Each BeginTabItem() defines one tab's content area
	// 4. Each DockSpace (with unique ID) provides independent docking for that tab
	// 5. Windows created inside RenderXXXWindows() automatically dock to the active tab's dockspace
	// 6. When you switch tabs, ImGui remembers each dockspace's layout independently
	//
	// This mimics Excel's sheet system where each sheet has its own independent workspace!
}

int Renderer::draw(DataManager& dataManager)
{
	// --- Start ImGui frame ---
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	newGUI(dataManager);  // Enable the new GUI with Portfolio tab
	//ImGui::ShowDemoWindow();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	return 0;
}

void Renderer::oldGUI(DataManager& dataManager)
{

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);
    DockSetting();

    // *** IMPORTANT: Process overlay ticker FIRST before any other UI ***
    // This ensures global keyboard capture works regardless of window focus
    OverlayTickerGUI();





    //// Scanner Results Window
    const auto& scanResults = dataManager.currentScannerResult;
    ScannerGUI(scanResults);

    //DrawChartGUI(dataManager);
    std::string symbol = dataManager.activeSymbol;
    if (dataManager.charts.find(symbol) != dataManager.charts.end()) {
        // Check if chart exists
        if (m_chartViews.find(symbol) == m_chartViews.end()) {
            // Create new chart if it doesn't exist
            m_chartViews[symbol] = createChartFromData(symbol, dataManager.charts[symbol].candles);
        }
        CreateChartView(m_chartViews[symbol]);
    }
}

ChartView Renderer::createChartFromData(const std::string& symbol, const std::vector<CandleData>& candles) {
    printf("Creating new chart view for symbol: %s with %zu candles\n", 
           symbol.c_str(), candles.size());

    ChartView newChart;
    newChart.title = symbol.c_str();
    newChart.isVisible = true;

    // Prepare vertex data from candles and get price range
    auto [vertexData, priceRange] = prepareCandleDataFromVector(candles);
    newChart.minPrice = priceRange.first;
    newChart.maxPrice = priceRange.second;

    // Initialize OpenGL objects
    auto [vao, vbo, numCandles] = initCandleDataFromVector(vertexData);
    newChart.vao = vao;
	newChart.vbo = vbo;
    newChart.numCandles = numCandles;
    newChart.shaderProgram = createShaderProgram();

    // Create initial FBO (will be resized in CreateChartView if needed)
    createChartFrameBuffer(newChart, 800, 600);

    return newChart;
}

void Renderer::CreateChartView(ChartView& chart)
{
    ImGui::Begin(chart.title, &chart.isVisible);
    DisableTitleFocusColors();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    
    if (avail.x > 0 && avail.y > 0)
    {
        // Optional: resize FBO if ImGui window resized
        if ((int)avail.x != chart.width || (int)avail.y != chart.height)
        {
            glDeleteTextures(1, &chart.colorTex);
            glDeleteFramebuffers(1, &chart.fbo);
            // Delete OLD resources including RBO!
            createChartFrameBuffer(chart, (int)avail.x, (int)avail.y);
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

