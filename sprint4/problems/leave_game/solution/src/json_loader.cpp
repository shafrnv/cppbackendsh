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
    
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Не удалось открыть файл " + json_path.string());
    }
    
    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    //Парсим JSON строку
    json::value json_value = json::parse(json_str);

    // Получить объект JSON
    json::object json_object = json_value.as_object();
    Speed defaultdogSpeed_;
    uint default_bag_capacity_;

    if (json_object.contains("dogRetirementTime")){
        game.SetRetirementTime(json_object["dogRetirementTime"].as_double());
    }
    if (json_object.contains("defaultDogSpeed")){
        defaultdogSpeed_ = Speed(json_object["defaultDogSpeed"].as_double(), json_object["defaultDogSpeed"].as_double());
    }
    if (json_object.contains("defaultBagCapacity")){
        default_bag_capacity_ = json_object["defaultBagCapacity"].as_int64();
    }
    if (json_object.contains("lootGeneratorConfig")){
        game.SetLootPeriod(std::to_string(json_object["lootGeneratorConfig"].as_object().at("period").as_double()));
        game.SetLootProbability(json_object["lootGeneratorConfig"].as_object().at("probability").as_double());
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
            if (json_map_object.contains("bagCapacity")){
                map.SetBagCapacity(json_map_object["bagCapacity"].as_int64());
            } else {
                map.SetBagCapacity(default_bag_capacity_);
            }
            if (json_map_object.contains("lootTypes")){
                json::array json_lootTypes = json_map_object["lootTypes"].as_array();
                ExtraData extra_data_;
                extra_data_.loot_types_ = json_lootTypes;
                map.SetExtraData(extra_data_);
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
    return game;
}

std::string GetLogServerStart(const std::string timestamp,
                              const std::string srv_address,
                              const int port) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["port"] = port;
    data_obj["address"] = srv_address;

    res_obj["data"] = data_obj;
    res_obj["message"] = "server started";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string GetLogServerStop(const std::string timestamp,
                             const int return_code,
                             const std::string exception_what) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["code"] = return_code;
    if (!exception_what.empty()) {
        data_obj["exception"] = exception_what;
    }

    res_obj["data"] = data_obj;
    res_obj["message"] = "server exited";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}
std::string GetLogRequest(const std::string timestamp,
                          const std::string client_address,
                          const std::string uri,
                          const std::string http_method) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["ip"] = client_address;
    data_obj["URI"] = uri;
    data_obj["method"] = http_method;

    res_obj["data"] = data_obj;
    res_obj["message"] = "request received";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string GetLogResponse(const std::string timestamp,
                           const std::string client_address,
                           const int response_time_msec,
                           const int response_code,
                           const std::string content_type) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["ip"] = client_address;
    data_obj["response_time"] = response_time_msec;
    data_obj["code"] = response_code;
    data_obj["content_type"] = content_type;

    res_obj["data"] = data_obj;
    res_obj["message"] = "response sent";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

std::string GetLogError(const std::string timestamp,
                        const int error_code,
                        const std::string error_text,
                        const std::string where) {
    boost::json::object res_obj;
    res_obj["timestamp"] = timestamp;

    boost::json::object data_obj;
    data_obj["code"] = error_code;
    data_obj["text"] = error_text;
    data_obj["where"] = where;

    res_obj["data"] = data_obj;
    res_obj["message"] = "error";
    boost::json::value val_json(res_obj);
    return {boost::json::serialize(val_json)};
}

}  // namespace json_loader
