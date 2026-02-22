#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>

#include "event.h"
#include "command.h"
#include "DataManager.h"
#include "Config.h"

// Forward declarations
class IbkrClient;
class Renderer;
struct GLFWwindow;

class App {
public:
    App();
    ~App();

    void init(GLFWwindow* window);
    void stop();
    void update();

    // Request chart for a symbol
    void requestChart(const std::string& symbol);

    DataManager dataManager;

private:
    std::mutex mtx;
    std::vector<ScannerResultItem> m_latestScannerResults;
    std::thread m_ibThread;
    int m_scannerReqId = 0;
    int m_nextReqId = 2;  // Start from 2 (1 is used by scanner)
    std::unique_ptr<IbkrClient> m_ibClient;
    std::unique_ptr<Renderer> m_renderer;
    Config m_config;  // Configuration loaded from file

    void startScanner(int reqId, const std::string& scanCode, double priceAbove = 5.0);
    void handleEvent(const Event& event);
};
