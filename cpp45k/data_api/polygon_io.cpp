#include "polygon_io.h"
std::string Polygon_io::formatTimestamp(long long ms)
{
    time_t seconds = ms / 1000;
    struct tm lt;
    localtime_s(&lt, &seconds); // Use localtime_s on Windows for safety

    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &lt);
    return std::string(buffer);
}
void Polygon_io::readfile() {
    // 1. Open the file
    std::ifstream inFile("aapl_data.json");
    if (!inFile.is_open()) {
        std::cerr << "Could not open aapl_data.json" << std::endl;
    }

    // 2. Parse the JSON
    json data;
    inFile >> data;
    inFile.close();

    // 3. Navigate to the 'results' array (Common structure for Polygon/Massive APIs)
    if (data.contains("results") && data["results"].is_array()) {

        std::cout << std::left << std::setw(22) << "Time"
            << std::setw(10) << "Open"
            << std::setw(10) << "High"
            << std::setw(10) << "Low"
            << std::setw(10) << "Close" << std::endl;
        std::cout << std::string(62, '-') << std::endl;

        for (const auto& bar : data["results"]) {
            // Mapping: t=time, o=open, h=high, l=low, c=close
            long long timestamp = bar["t"];
            double o = bar["o"];
            double h = bar["h"];
            double l = bar["l"];
            double c = bar["c"];

            std::cout << std::left << std::setw(22) << formatTimestamp(timestamp)
                << std::setw(10) << o
                << std::setw(10) << h
                << std::setw(10) << l
                << std::setw(10) << c << std::endl;
        }
    }
    else {
        std::cout << "No OHLC data found in the 'results' field." << std::endl;
    }
}

