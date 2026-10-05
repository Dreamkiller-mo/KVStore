#pragma once
#include <filesystem>

namespace kvstore
{

struct Config
{
    std::filesystem::path data_dir = "./data";
};

}