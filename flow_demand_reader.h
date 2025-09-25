#pragma once
#include <string>
#include <vector>

struct FlowDemand
{
    std::string src;
    std::string dst;
    double rateMbps;
};

std::vector<std::vector<FlowDemand>> LoadAllMatrices();
