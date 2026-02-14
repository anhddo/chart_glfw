#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>

#include "event.h"
#include "command.h"
#include "DataManager.h"

// Forward declaration
class IbkrClient;

class App {
public:
    App();
    ~App();

    void start();
    void stop();
    void update();

    DataManager dataManager;

private:
    std::mutex mtx;
    std::vector<ScannerResultItem> m_latestScannerResults;
    int m_scannerReqId = 0;
    std::unique_ptr<IbkrClient> m_ibClient;

    void startScanner(int reqId, const std::string& scanCode, double priceAbove = 5.0);
    void handleEvent(const Event& event);
};
