#pragma once
#include <iostream>
#include <fstream>
#include <filesystem>

#include "model.h"

namespace json = boost::json;
using namespace model;
namespace json_loader {

Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
