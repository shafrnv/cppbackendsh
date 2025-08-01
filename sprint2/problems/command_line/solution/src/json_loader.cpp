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
Building ParseBuildingFromJson(const json::object& rect_obj) {
    Point position{Dimension(rect_obj.at("x").as_int64()), Dimension(rect_obj.at("y").as_int64())};
    Size size{Dimension(rect_obj.at("w").as_int64()), Dimension(rect_obj.at("h").as_int64())};

    Rectangle bounds = Rectangle{position, size};
    return Building(bounds);
}

Office ParseOfficeFromJson(const json::object& office_obj) {
    Office::Id id{office_obj.at("id").as_string().c_str()};
    Point position{Dimension(office_obj.at("x").as_int64()), Dimension(office_obj.at("y").as_int64())};
    Offset offset{Dimension(office_obj.at("offsetX").as_int64()), Dimension(office_obj.at("offsetY").as_int64())};
    return Office(id, position, offset);
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    
    model::Game game;
    // Загрузить содержимое файла json_path, например, в виде строки
    
    // Открываем файл
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Не удалось открыть файл " + json_path.string());
    }
    
    // Читаем содержимое файла в строку
    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    //Закрываем файл
    file.close();
    
    // Распарсить строку как JSON, используя boost::json::parse

    //Парсим JSON строку
    json::value json_value = json::parse(json_str);

    // Получить объект JSON
    json::object json_object = json_value.as_object();
    Speed defaultdogSpeed_;
    if (json_object.contains("defaultDogSpeed")){
        defaultdogSpeed_ = Speed(json_object["defaultDogSpeed"].as_double(), json_object["defaultDogSpeed"].as_double());
    }
    if (json_object.contains("maps")){
        json::array json_maps = json_object["maps"].as_array();
        for (const json::value& json_map : json_maps) {
            json::object json_map_object = json_map.as_object();
            model::Map::Id id{json_map_object.at("id").as_string().c_str()};
            std::string name = json_map_object.at("name").as_string().c_str();
            model::Map map(id, name);
            if (json_map_object.contains("dogSpeed")){
                map.SetSpeed(Speed(json_map_object["dogSpeed"].as_double(),json_map_object["dogSpeed"].as_double()));            
            } else {
                map.SetSpeed(defaultdogSpeed_);
            }
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
