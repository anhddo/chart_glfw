#pragma once
#include <variant>
#include <string>
#include <vector>

struct ScannerResultItem {
	int rank;
	std::string symbol;
	std::string secType;
	std::string currency;
	long conId;
};

struct ScannerResult
{
	int reqId;
	std::vector<ScannerResultItem> items;
};

struct TickPrice
{
	int tickerId;
	double price;
};

struct OrderStatus
{
	int orderId;
	std::string status;
};

struct CandleData {
	std::string date;  // Or use time_t
	double open;
	double high;
	double low;
	double close;
	long volume;
};

struct HistoricalDataEvent {
	int reqId;
	std::string symbol;
	std::vector<CandleData> candles;
};

using EventData = std::variant<
	ScannerResult,
	TickPrice,
	OrderStatus,
	HistoricalDataEvent
>;

struct Event
{
	EventData data;
};
