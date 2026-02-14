#pragma once
#include <variant>
#include <string>
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


using EventData = std::variant<
	ScannerResult,
	TickPrice,
	OrderStatus
>;

struct Event
{
	EventData data;
};
