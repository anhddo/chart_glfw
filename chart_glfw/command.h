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

using CommandData = std::variant<
    StartScannerCommand,
    CancelScannerCommand
>;

struct Command {
    CommandData data;
};