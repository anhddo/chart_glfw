#include "App.h"
#include "ibkr.h"
#include <chrono>
#include <iostream>
#include "renderer.h"
#include "command.h"

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
	// Load configuration from file
	if (!m_config.load("config.json")) {
		printf("ERROR: Failed to load configuration. Application cannot start.\n");
		printf("Please create config.json from config.json.template\n");
		throw std::runtime_error("Configuration not loaded");
	}

	// Create IbkrClient with config values (host, port, clientId)
	m_ibClient = std::make_unique<IbkrClient>(
		m_config.ibkr.host,
		m_config.ibkr.port,
		m_config.ibkr.clientId
	);
	m_ibThread = std::thread([this]() {
		m_ibClient->processLoop();
	});


	std::this_thread::sleep_for(std::chrono::seconds(1));

	// Use config values instead of hardcoded ones
	startScanner(1, m_config.scanner.defaultScanCode, m_config.scanner.priceAbove);

	m_renderer = std::make_unique<Renderer>();

	m_renderer->init(window);

	// Set window user pointer so scroll callback can access renderer
	glfwSetWindowUserPointer(window, m_renderer.get());

	// Wire up symbol input callback
	m_renderer->onSymbolEntered = [this](const std::string& symbol) {
		if (dataManager.charts.find(symbol) != dataManager.charts.end()) {
			dataManager.activeSymbol = symbol;
			printf("Switched active chart to %s\n", symbol.c_str());
		} else {
			printf("No chart data for %s, requesting...\n", symbol.c_str());
			requestChart(symbol);
		}
	};

	m_renderer->onScannerRowClicked = [this](const std::string& symbol) {
		if (dataManager.charts.find(symbol) != dataManager.charts.end()) {
			dataManager.activeSymbol = symbol;
			printf("Switched active chart to %s\n", symbol.c_str());
		}
		else {
			printf("No chart data for %s, requesting...\n", symbol.c_str());
			requestChart(symbol);
		}
	};

	// Request account data using account from config
	RequestAccountDataCommand cmd;
	cmd.accountCode = m_config.ibkr.account;
	m_ibClient->pushCommand(cmd);

	printf("✓ Account data requested for: %s\n", 
		   m_config.ibkr.account.substr(m_config.ibkr.account.length() - 4).c_str());
}

void App::stop()
{
    DiscounnectCommand disconnectCmd;

    m_ibClient->pushCommand(disconnectCmd);
    // 3. Wait for thread to finish
    if (m_ibThread.joinable()) {
        printf("Waiting for IB thread to finish...\n");
        m_ibThread.join();
        printf("IB thread joined\n");
    }

    //printf("App::stop() finished\n");
    //if (m_ibClient)
    //    m_ibClient->stop();
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
    StartScannerCommand command;
    command.reqId = reqId;
    command.scanCode = scanCode;
    command.locationCode = "STK.US";
    command.priceAbove = priceAbove;

    
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

            m_ibClient->pushCommand(std::move(cancelCmd));
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
        else if constexpr (std::is_same_v<T, AccountSummaryEvent>) {
            // Update account values (NetLiquidation, BuyingPower, etc.)
            for (const auto& [key, accountValue] : arg.accountValues) {
                dataManager.accountData.accountValues[key] = accountValue;

                // Update summary fields for quick access
                if (key == "NetLiquidation") {
                    try {
                        dataManager.accountData.totalValue = std::stod(accountValue.value);
                    } catch (...) {
                        dataManager.accountData.totalValue = 0.0;
                    }
                }
                else if (key == "AvailableFunds") {
                    try {
                        dataManager.accountData.availableFunds = std::stod(accountValue.value);
                    } catch (...) {
                        dataManager.accountData.availableFunds = 0.0;
                    }
                }
                else if (key == "BuyingPower") {
                    try {
                        dataManager.accountData.buyingPower = std::stod(accountValue.value);
                    } catch (...) {
                        dataManager.accountData.buyingPower = 0.0;
                    }
                }
            }

            // Update positions (merge with existing or add new)
            for (const auto& newPos : arg.positions) {
                bool found = false;

                // Look for existing position with same symbol and account
                for (auto& existingPos : dataManager.accountData.positions) {
                    if (existingPos.symbol == newPos.symbol && 
                        existingPos.account == newPos.account) {
                        // Update existing position
                        existingPos = newPos;
                        found = true;
                        break;
                    }
                }

                // If not found, add as new position
                if (!found) {
                    dataManager.accountData.positions.push_back(newPos);
                }
            }

            printf("Account data updated: %zu account values, %zu positions\n",
                   arg.accountValues.size(), arg.positions.size());
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

    
    m_ibClient->pushCommand(std::move(cmd));

    printf("Requesting daily chart for %s (reqId=%d, duration=%s)\n", 
           symbol.c_str(), cmd.reqId, cmd.durationStr.c_str());
}
