#include "App.h"
#include "data_api/ikbr/ibkr.h"
#include <chrono>
#include <iostream>
#include "renderer.h"

App::App() 
    : m_scannerReqId(0)
{
}

App::~App() 
{
    stop();
}

void App::init(GLFWwindow* window)
{
    m_ibClient = std::make_unique<IbkrClient>();
    std::thread ibThread([this]() {
        m_ibClient->processLoop();
    });
    ibThread.detach();
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    startScanner(1, "TOP_PERC_GAIN");
	m_renderer = std::make_unique<Renderer>();

	m_renderer->init(window);

	// Set window user pointer so scroll callback can access renderer
	glfwSetWindowUserPointer(window, m_renderer.get());

	// Wire up symbol input callback
	m_renderer->onSymbolEntered = [this](const std::string& symbol) {
		requestChart(symbol);
	};

}

void App::stop()
{
    if (m_ibClient)
        m_ibClient->stop();
}

void App::update()
{
    std::queue<Event> eventQueue = m_ibClient->consumeEvents();

    while (!eventQueue.empty())
    {
        handleEvent(eventQueue.front());
        eventQueue.pop();
    }
    m_renderer->draw(dataManager);
}

void App::startScanner(int reqId, const std::string& scanCode, double priceAbove)
{
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

void App::handleEvent(const Event& event)
{
    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, ScannerResult>) {
            dataManager.currentScannerResult = arg;

            CancelScannerCommand cancelCmd;
            cancelCmd.reqId = arg.reqId;
            Command command;
            command.data = cancelCmd;
            m_ibClient->pushCommand(std::move(command));
        }
        else if constexpr (std::is_same_v<T, HistoricalDataEvent>) {
            // Store chart data
            ChartData chartData;
            chartData.symbol = arg.symbol;
            chartData.candles = arg.candles;
            chartData.reqId = arg.reqId;

            dataManager.charts[arg.symbol] = chartData;
            dataManager.activeSymbol = arg.symbol;

            printf("Chart data received for %s: %zu candles\n", 
                   arg.symbol.c_str(), arg.candles.size());
        }
    }, event.data);
}

void App::requestChart(const std::string& symbol)
{
    RequestHistoricalDataCommand cmd;
    cmd.reqId = m_nextReqId++;
    cmd.symbol = symbol;
    cmd.endDateTime = "";           // Empty = now
    cmd.durationStr = "1 Y";        // 1 year of data (maximum for daily bars)
    cmd.barSizeSetting = "1 day";   // Daily candles
    cmd.whatToShow = "TRADES";
    cmd.useRTH = 1;                 // Regular trading hours only

    Command command;
    command.data = cmd;
    m_ibClient->pushCommand(std::move(command));

    printf("Requesting daily chart for %s (reqId=%d, duration=%s)\n", 
           symbol.c_str(), cmd.reqId, cmd.durationStr.c_str());
}
