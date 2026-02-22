#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct IBKRConfig {
    std::string account = "";
    std::string host = "127.0.0.1";
    int port = 7497;  // 7497 = TWS paper trading, 7496 = TWS live
    int clientId = 0;
};

struct ScannerConfig {
    std::string defaultScanCode = "TOP_PERC_GAIN";
    double priceAbove = 5.0;
};

class Config {
public:
    IBKRConfig ibkr;
    ScannerConfig scanner;

    bool load(const std::string& filename = "config.json") {
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            std::cerr << "ERROR: Config file '" << filename << "' not found!\n";
            std::cerr << "Please copy 'config.json.template' to 'config.json' and fill in your account details.\n";
            return false;
        }

        try {
            json j;
            file >> j;

            // Load IBKR config
            if (j.contains("ibkr")) {
                auto ibkrJson = j["ibkr"];
                if (ibkrJson.contains("account")) {
                    ibkr.account = ibkrJson["account"].get<std::string>();
                }
                if (ibkrJson.contains("host")) {
                    ibkr.host = ibkrJson["host"].get<std::string>();
                }
                if (ibkrJson.contains("port")) {
                    ibkr.port = ibkrJson["port"].get<int>();
                }
                if (ibkrJson.contains("clientId")) {
                    ibkr.clientId = ibkrJson["clientId"].get<int>();
                }
            }

            // Load scanner config
            if (j.contains("scanner")) {
                auto scannerJson = j["scanner"];
                if (scannerJson.contains("defaultScanCode")) {
                    scanner.defaultScanCode = scannerJson["defaultScanCode"].get<std::string>();
                }
                if (scannerJson.contains("priceAbove")) {
                    scanner.priceAbove = scannerJson["priceAbove"].get<double>();
                }
            }

            // Validate required fields
            if (ibkr.account.empty() || ibkr.account == "YOUR_ACCOUNT_NUMBER_HERE") {
                std::cerr << "ERROR: Account number not configured in config.json!\n";
                std::cerr << "Please edit config.json and set your IBKR account number.\n";
                return false;
            }

            std::cout << "✓ Configuration loaded successfully\n";
            std::cout << "  Account: " << maskAccount(ibkr.account) << "\n";
            std::cout << "  Host: " << ibkr.host << ":" << ibkr.port << "\n";
            return true;

        } catch (const json::exception& e) {
            std::cerr << "ERROR: Failed to parse config.json: " << e.what() << "\n";
            return false;
        }
    }

    // Create template config file if it doesn't exist
    static bool createTemplate(const std::string& filename = "config.json.template") {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        json j;
        j["ibkr"]["account"] = "YOUR_ACCOUNT_NUMBER_HERE";
        j["ibkr"]["host"] = "127.0.0.1";
        j["ibkr"]["port"] = 7497;
        j["ibkr"]["clientId"] = 0;
        j["scanner"]["defaultScanCode"] = "TOP_PERC_GAIN";
        j["scanner"]["priceAbove"] = 5.0;

        file << j.dump(2);
        return true;
    }

private:
    // Mask account number for logging (show only last 4 digits)
    std::string maskAccount(const std::string& account) const {
        if (account.length() <= 4) {
            return "****";
        }
        return std::string(account.length() - 4, '*') + account.substr(account.length() - 4);
    }
};
