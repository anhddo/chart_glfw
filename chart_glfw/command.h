#pragma once
#include <variant>
#include <string>

struct StartScannerCommand {
    int reqId;
    std::string scanCode;
    std::string locationCode;
    double priceAbove;
};

struct CancelScannerCommand {
    int reqId;
};

struct RequestHistoricalDataCommand {
    int reqId;
    std::string symbol;
    std::string endDateTime;    // Format: "20230101 23:59:59" or empty for now
    std::string durationStr;    // "1 D", "1 W", "1 M", "1 Y"
    std::string barSizeSetting; // "1 min", "5 mins", "1 hour", "1 day"
    std::string whatToShow;     // "TRADES", "MIDPOINT", "BID", "ASK"
    int useRTH;                 // 1 = regular trading hours only, 0 = all hours
};

using CommandData = std::variant<
    StartScannerCommand,
    CancelScannerCommand,
    RequestHistoricalDataCommand
>;

struct Command {
    CommandData data;
};