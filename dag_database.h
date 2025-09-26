#pragma once
#include <string>
#include <vector>

struct Dag
{
    std::vector<std::vector<std::string>> adjacency_list;
};

std::vector<Dag> LoadDAG();
