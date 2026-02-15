#pragma once
#include "event.h"
#include <unordered_map>
#include <string>

struct ChartData {
	std::string symbol;
	std::vector<CandleData> candles;
	int reqId;
};

class DataManager {
public:
	ScannerResult currentScannerResult;

	// Store charts by symbol
	std::unordered_map<std::string, ChartData> charts;

	// Active chart symbol (what's currently displayed)
	std::string activeSymbol;
};
