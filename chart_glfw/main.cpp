#ifdef _WIN32
// This tells the linker to use main() even if we are in Windows Subsystem mode
#pragma comment(linker, "/entry:mainCRTStartup")
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // first

#include "renderer.h"
#include "event.h"
#include "data_api/ikbr/ibkr.h"
#include "DataManager.h"

//#include "data_api/ikbr/MainClientDemo.h"

std::queue<std::string> messageQueue;
std::queue<std::string> localQueue;
std::mutex queueMutex;

void enqueueMessage() {
	int count = 0;
	while(true) {
		std::lock_guard<std::mutex> lock(queueMutex);
		count++;

		std::string msg = "Message " + std::to_string(count);
		messageQueue.push(msg);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	
}
void processMessages() {

	while (true) {
		std::string message;
		std::lock_guard<std::mutex> lock(queueMutex);
		std::swap(localQueue, messageQueue);
		
		if (!localQueue.empty()) {
			message = localQueue.front();
			localQueue.pop();
			std::printf("Processed: %s\n", message.c_str());
		}
		
		
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}


class App {
	public:
	App() {}
	~App() {}
	int option = 0;

	std::mutex mtx; 
	std::vector<ScannerResultItem> m_latestScannerResults;
	int m_scannerReqId = 0;

	std::unique_ptr<IbkrClient> m_ibClient;

    void start() {
		m_ibClient = std::make_unique<IbkrClient>();
        std::thread ibThread([this]() {
			m_ibClient->processLoop();
            });
		ibThread.detach();
		std::this_thread::sleep_for(std::chrono::seconds(1)); // Give the IB client time to start
		startScanner(1, "TOP_PERC_GAIN");

    }


    void stop()
    {
        if (m_ibClient)
            m_ibClient->stop();
    }
    void update(){
        // Process events
        
        std::queue<Event> eventQueue= m_ibClient->consumeEvents();;

        

        while (!eventQueue.empty())
        {
            handleEvent(eventQueue.front());
            eventQueue.pop();
        }
    
        //std::queue<Command> localQueueCommand;

        //// Process Command
        //{
        //    std::lock_guard<std::mutex> lock(mtx);
        //    while (!m_eventQueue.empty()) {
        //        Event event = m_eventQueue.front();
        //        m_eventQueue.pop();
        //        updateAppState(event);
        //        // Handle event (e.g., update UI with new scanner results, tick prices, order status)
        //    }
        //}
	}
	DataManager dataManager;
private:
    
    void startScanner(int reqId, const std::string& scanCode, double priceAbove = 5.0) {
        StartScannerCommand cmd;
        cmd.reqId = reqId;
        cmd.scanCode = scanCode;
        cmd.locationCode = "STK.US";
        cmd.priceAbove = priceAbove;

        Command command;
        command.data = cmd;
        m_ibClient->pushCommand(std::move(command));

        printf("UI: Scanner command sent (reqId=%d, scanCode=%s)\n", reqId, scanCode.c_str());
    }



    void handleEvent(const Event& event) {
        std::visit([this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, ScannerResult>) {
				dataManager.currentScannerResult = arg; // Update the current scanner result in the data manager

 
                CancelScannerCommand cancelCmd;
                cancelCmd.reqId = arg.reqId;
                Command command;
                command.data = cancelCmd;
                m_ibClient->pushCommand(std::move(command));
            }
            // Add more event types here as needed


            }, event.data);

    }

};

const unsigned int SCR_WIDTH = 2560;
const unsigned int SCR_HEIGHT = 1280;
int main() {
	


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

    //glfwSetWindowUserPointer(window, this);
    //glfwMaximizeWindow(window);

    //glfwMakeContextCurrent(window);
    //glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwSetScrollCallback(window, Renderer::ScrollCallback);

    glfwSwapInterval(1);// Enable V-Sync for smoother rendering

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    // Setup Dear ImGui context
	App appController;
	appController.start();

    Renderer renderer;
    renderer.init(window);


    //GLuint shaderProgram = createShaderProgram();


    //std::vector<float> candleVertices = prepareCandleData("be_data.json");
    //int candleCount = candleVertices.size() / (5 * 8); // 8 vertices per candle (2 wick + 6 body)
    //auto [VAO, numCandles] = initCandleData("be_data.json");

    //// Build Shader (use the same source from previous message)
    //bool show_nvda = true;
    //bool show_aapl = true;
    //std::vector<ChartView> charts(2);  // contains fbo & colorTex
    //charts[0].title = "NVDA Chart";
    //charts[1].title = "AAPL Chart";

    //for (auto& chart : charts) {
    //    chart.shaderProgram = shaderProgram;
    //    chart.vao = VAO;
    //    chart.numCandles = numCandles;
    //}

    //char symbolInput[32] = "";
    //bool requestData = false;
    //std::string currentSymbol = "";


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        appController.update();
        renderer.draw(appController.dataManager);



        glfwSwapBuffers(window);

    }
    //glDeleteVertexArrays(1, &VAO);
    //glDeleteBuffers(1, &VBO);
    //glDeleteProgram(shaderProgram);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    //std::thread producer(enqueueMessage);
    //

    //producer.detach();
    //processMessages();
	return 0;

};