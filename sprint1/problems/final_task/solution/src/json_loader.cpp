#include "json_loader.h"

namespace json_loader {

Road ParseRoadFromJson(const json::object& road_obj) {
    Point start{Dimension(road_obj.at("x0").as_int64()), Dimension(road_obj.at("y0").as_int64())};
    if (road_obj.contains("x1")) {
        // Горизонтальная дорога
        Coord end_x = Dimension(road_obj.at("x1").as_int64());
        return Road(Road::HORIZONTAL, start, end_x);
    } else if (road_obj.contains("y1")) {
        // Вертикальная дорога
        Coord end_y = Dimension(road_obj.at("y1").as_int64());
        return Road(Road::VERTICAL, start, end_y);
    } else {
        throw std::runtime_error("Недопустимый формат дороги в JSON");
    }
}
const std::string kX = "x";
const std::string kY = "y";
const std::string kW = "w";
const std::string kH = "h";
const std::string kId = "id";
const std::string kOffsetX = "offsetX";
const std::string kOffsetY = "offsetY";

Building ParseBuildingFromJson(const json::object& rect_obj) {
    Point position{Dimension(rect_obj.at(kX).as_int64()), Dimension(rect_obj.at(kY).as_int64())};
    Size size{Dimension(rect_obj.at(kW).as_int64()), Dimension(rect_obj.at(kH).as_int64())};

    Rectangle bounds = Rectangle{position, size};
    return Building(bounds);
}

Office ParseOfficeFromJson(const json::object& office_obj) {
    Office::Id id{office_obj.at(kId).as_string().c_str()};
    Point position{Dimension(office_obj.at(kX).as_int64()), Dimension(office_obj.at(kY).as_int64())};
    Offset offset{Dimension(office_obj.at(kOffsetX).as_int64()), Dimension(office_obj.at(kOffsetY).as_int64())};
    return Office(id, position, offset);
}


model::Game LoadGame(const std::filesystem::path& json_path) {
    
    model::Game game;
    
    // Открываем файл, с конфигом игры
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Не удалось открыть файл " + json_path.string());
    }
    
    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    json::value json_value = json::parse(json_str);

    json::object json_object = json_value.as_object();
    
    // Загрузить модель игры из json
    if (json_object.contains("maps")){
        json::array json_maps = json_object["maps"].as_array();
        for (const json::value& json_map : json_maps) {
            json::object json_map_object = json_map.as_object();
            model::Map::Id id{json_map_object.at("id").as_string().c_str()};
            std::string name = json_map_object.at("name").as_string().c_str();
            model::Map map(id, name);
            if (json_map_object.contains("roads")){
                json::array json_roads = json_map_object["roads"].as_array();
                for (const auto& json_road : json_roads) {
                    const auto& road_obj = json_road.as_object();
                    model::Road road = ParseRoadFromJson(road_obj);
                    map.AddRoad(road);
                }
            }
            if (json_map_object.contains("buildings")){
                json::array json_buildings = json_map_object["buildings"].as_array();
                for (const auto& json_building : json_buildings) {
                    const auto& building_obj = json_building.as_object();
                    model::Building building = ParseBuildingFromJson(building_obj);
                    map.AddBuilding(building);
                }
            }
            if (json_map_object.contains("offices")){
                json::array json_offices = json_map_object["offices"].as_array();
                for (const auto& json_office : json_offices) {
                    const auto& office_obj = json_office.as_object();
                    model::Office office = ParseOfficeFromJson(office_obj);
                    map.AddOffice(office);
                }
            }
            game.AddMap(map);
}
        }
    // Загрузить модель игры из файла
    return game;
}

}  // namespace json_loader
