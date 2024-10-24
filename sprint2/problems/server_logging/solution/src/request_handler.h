#pragma once
#include "http_server.h"
#include "model.h"
#include <iostream>
#include <filesystem>
#include <cassert>
#include <locale>
#include <string>
#include <algorithm>
#include <boost/beast.hpp>
//#include <boost/filesystem.hpp>
namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace sys = boost::system;
using namespace std;
class RequestHandler {
public:
    
    explicit RequestHandler(model::Game& game, std::string path_static)
        : game_{game},
        path_{path_static} { }
    
    std::unordered_map<std::string, std::string> mime_types = {
    {".htm", "text/html"}, {".html", "text/html"}, 
    {".css", "text/css"}, 
    {".txt", "text/plain"},
    {".js", "text/javascript"}, 
    {".json", "application/json"}, 
    {".xml", "application/xml"}, 
    {".png", "image/png"},
    {".jpg", "image/jpeg"}, {".jpe", "image/jpeg"}, {".jpeg", "image/jpeg"}, 
    {".gif", "image/gif"},
    {".bmp", "image/bmp"}, 
    {".ico", "image/vnd.microsoft.icon"},
    {".tiff", "image/tiff"}, {".tif", "image/tiff"},
    {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"}, 
    {".mp3", "audio/mpeg"}
    };

    bool IsSubPath(fs::path path, fs::path base) {
        // Приводим оба пути к каноничному виду (без . и ..)    
        // Проверяем, что все компоненты base содержатся внутри path
        for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
            if (p == path.end() || *p != *b) {
                return false;
            }
        }
        return true;
    }

    std::string get_mime_type(std::string extension) {
        std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });
        auto it = mime_types.find(extension);
        if (it != mime_types.end()) {
            return it->second;
        }
        return "application/octet-stream";
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void handleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto target = req.target();
        std::string requested_path_in_str;
        if (target == "/") {
            requested_path_in_str = path_ + "/index.html";
        } else {
            auto decoded_url = url_decode(std::string(target.data(), target.size()));
            requested_path_in_str = path_ +decoded_url;
        }
        fs::path requested_path(requested_path_in_str);
        requested_path =  fs::weakly_canonical(requested_path);
        fs::path root_path(path_);
        root_path =  fs::weakly_canonical(root_path);
        //Проверка на выход за корневой каталог
        if (!IsSubPath(requested_path, root_path)) {
            http::response<http::string_body> res{http::status::bad_request, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.body() = "Error: Path outside of root directory.";
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            res.prepare_payload();
            send(std::move(res));
            return;
        }
        http::file_body::value_type file;
        sys::error_code ec;
        if (sys::error_code ec; file.open(requested_path_in_str.c_str(), beast::file_mode::read, ec), ec) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.body() = "Error: File not found.";
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            res.prepare_payload();
            send(std::move(res));
            return;
        }
        auto const size = file.size();
        http::response<http::file_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, get_mime_type(requested_path.extension()));
        res.content_length(size);
        res.body() = std::move(file);
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        send(std::move(res));
    }
    
    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        try {
            // Обработаем запрос и сформируем соответствующий ответ
            if (req.method() == http::verb::get && req.target() == "/api/v1/maps") {
                handleGetMaps(std::move(req), std::move(send));
            } else if (req.method() == http::verb::get && req.target().starts_with("/api/v1/maps/")) {
                handleGetMapById(std::move(req), std::move(send));
            } else if (req.method() == http::verb::post && req.target() == "/api/v1/game/join") {
                handleJoinToGame(std::move(req), std::move(send));
            } else if (req.target().starts_with("/api/")) {
                send(badRequest(std::move(req)));
            } else {    
                handleRequest(std::move(req), std::move(send));
            }
        } catch (std::exception& e) {
            std::cerr << "Error handling request: " << e.what() << std::endl;
        }   
    }
private:
    std::string url_decode(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '%') {
                if (i + 2 < s.size()) {
                    int value;
                    std::istringstream is(s.substr(i + 1, 2));
                    if (is >> std::hex >> value) {
                        result += static_cast<char>(value);
                        i += 2;
                    } else {
                        result += '%';
                    }
                }
            } else if (s[i] == '+') {
                result += ' ';
            } else {
                result += s[i];
            }
        }            
        return result;
    }
    

    template <typename Body, typename Allocator, typename Send>
    void handleGetMaps(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        json::array maps_list_;
        for (const auto& map : game_.GetMaps()) {
            std::cout << *(map.GetId()) << std::endl;
            maps_list_.push_back(json::object{
                { "id", *(map.GetId())},
                {"name", map.GetName()}
                });
        }
        sendResponse(std::move(req), std::move(send), maps_list_);
    }

    json::value toJson(const model::Road& road) {
        json::object road_json {
            {"x0", road.GetStart().x},
            {"y0", road.GetStart().y}
        };

        if (road.IsHorizontal()) {
            road_json["x1"] = road.GetEnd().x;
        } else if (road.IsVertical()) {
            road_json["y1"] = road.GetEnd().y;
        }

        return road_json;
    }
    json::value toJson(const model::Building& building) {
        const auto& bounds = building.GetBounds();
        return json::object{
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        };
    }

    json::value toJson(const model::Office& office) {
        const auto& position = office.GetPosition();
        const auto & offset = office.GetOffset();
        return json::object{
            {"id", *office.GetId()},
            {"x", position.x},
            {"y", position.y},
            {"offsetX", offset.dx},
            {"offsetY", offset.dy}
        };
    }

    json::value toJson(const std::vector<model::Road>& roads) {
        json::array roads_json;
        for (const model::Road road : roads) {
            roads_json.push_back(toJson(road));
        }
        return roads_json;
    }

    json::value toJson(const std::vector<model::Building>& buildings) {
        json::array buildings_json;
        for (const model::Building& building : buildings) {
            buildings_json.push_back(toJson(building));
        }
        return buildings_json;
    }

    json::value toJson(const std::vector<model::Office>& offices) {
        json::array offices_json;
        for (const model::Office& office : offices) {
            offices_json.push_back(toJson(office));
        }

        return offices_json;
    }

    template <typename Body, typename Allocator, typename Send>
    void handleGetMapById(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target_str = std::string(req.target().substr(std::strlen("/api/v1/maps/")));
        model::Map::Id map_id{target_str};
        auto map = game_.FindMap(map_id);
        if (!map) {
            send(mapNotFound(std::move(req)));
            return;
        }

        json::object map_json{
            {"id", *(map->GetId())},
            {"name", map->GetName()},
            {"roads", toJson(map->GetRoads())},
            {"buildings", toJson(map->GetBuildings())},
            {"offices", toJson(map->GetOffices())}};

        sendResponse(std::move(req), std::move(send), map_json);
    }

    template <typename Body, typename Allocator, typename Send, typename Json>
    void sendResponse(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, Json&& json_response) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
        res.body() = json::serialize(json_response);
        send(std::move(res));
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> mapNotFound(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "mapNotFound"},
            {"message", "Map not found"}};

        return createErrorResponse(std::move(req), http::status::not_found, error_response);
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> badRequest(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "badRequest"},
            {"message", "Bad request"}};

        return createErrorResponse(std::move(req), http::status::bad_request, error_response);
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> notFound(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "notFound"},
            {"message", "Not found"}};

        return createErrorResponse(std::move(req), http::status::not_found, error_response);
    }
    template <typename Body, typename Allocator, typename Json>
    http::response<http::string_body> createErrorResponse(http::request<Body, http::basic_fields<Allocator>>&& req, http::status status, Json&& json_response) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
        res.body() = json::serialize(json_response);
        return res;
    }

    template <typename Body, typename Allocator, typename Send>
    void handleJoinToGame(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send){
        
    }
    model::Game& game_;
    std::string path_;
};

}  // namespace http_handler
