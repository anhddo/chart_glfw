#include "App.h"
#include "data_api/ikbr/ibkr.h"
#include <chrono>
#include <iostream>

App::App() 
    : m_scannerReqId(0)
{
}

App::~App() 
{
    stop();
}

void App::start() 
{
    m_ibClient = std::make_unique<IbkrClient>();
    std::thread ibThread([this]() {
        m_ibClient->processLoop();
    });
    ibThread.detach();
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    startScanner(1, "TOP_PERC_GAIN");
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
    }, event.data);
}
