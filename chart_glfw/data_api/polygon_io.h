#pragma once
#include <fstream>
#include <iomanip>

#include <iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class Polygon_io
{

    private:
        std::string formatTimestamp(long long ms); 
    public:
        void readfile();
};

