#pragma once
#include <variant>
#include <string>
#include <vector>
#include <unordered_map>


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

// Account value update (e.g., NetLiquidation, AvailableFunds, etc.)
struct AccountValueUpdate {
	std::string key;        // "NetLiquidation", "TotalCashValue", etc.
	std::string value;      // The value as string
	std::string currency;   // "USD", "EUR", etc.
	std::string accountName;
};

// Position update (holdings in account)
struct PositionUpdate {
	std::string account;
	std::string symbol;
	std::string secType;
	double position;        // Number of shares/contracts
	double marketPrice;     // Current market price
	double marketValue;     // position * marketPrice
	double averageCost;     // Average cost basis
	double unrealizedPNL;   // Unrealized profit/loss
	double realizedPNL;     // Realized profit/loss
};

// Account summary event
struct AccountSummaryEvent {
	std::unordered_map<std::string, AccountValueUpdate> accountValues;
	std::vector<PositionUpdate> positions;
};

using EventData = std::variant<
	ScannerResult,
	TickPrice,
	OrderStatus,
	HistoricalDataEvent,
	AccountSummaryEvent
>;

struct Event
{
	EventData data;
};
