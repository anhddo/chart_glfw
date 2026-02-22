#pragma once
#include "event.h"
#include <unordered_map>
#include <string>

struct ChartData {
	std::string symbol;
	std::vector<CandleData> candles;
	int reqId;
};

// Account data storage
struct AccountData {
	std::unordered_map<std::string, AccountValueUpdate> accountValues;
	std::vector<PositionUpdate> positions;
	double totalValue = 0.0;
	double availableFunds = 0.0;
	double buyingPower = 0.0;
};

class DataManager {
public:
	ScannerResult currentScannerResult;

	// Store charts by symbol
	std::unordered_map<std::string, ChartData> charts;

	// Active chart symbol (what's currently displayed)
	std::string activeSymbol;

	// Account information
	AccountData accountData;
};
