#pragma once
#include "http_server.h"
#include "model.h"
#include <filesystem>
#include <cassert>
#include <iostream>
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
        std::cout << "is it here" <<std::endl;
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
        std::cout << "is it here2" <<std::endl;
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
        std::cout << "is it here3" <<std::endl;
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
        std::cout << "is it here4" <<std::endl;
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
            } else if (req.target() == "/api/v1/game/join") {
                if (req.method() == http::verb::post){
                    handleJoinGame(std::move(req), std::move(send));
                } else {
                    send(badMethodNotPost(std::move(req)));
                }
            } else if (req.target() == "/api/v1/game/players") {
                 if (!(req.method() == http::verb::get || req.method() == http::verb::head)) {
                    send(badMethodNotGetOrHead(std::move(req)));
                } else{
                    handleGetPlayers(std::move(req), std::move(send));
                }
            }else if (req.target() == "/api/v1/game/state") {
                std::cout << "is it here6" <<std::endl;
                if (!(req.method() == http::verb::get || req.method() == http::verb::head)) {
                    send(badMethodNotGetOrHead(std::move(req)));
                } else{
                    handleGetStateInformation(std::move(req), std::move(send));
                }
            }else if (req.target() == "/api/v1/game/player/action") {
                if (req.method() == http::verb::post){
                    handleAction(std::move(req), std::move(send));
                } else {
                    send(badMethodNotPost(std::move(req)));
                }
            } else if (req.target().starts_with("/api/")) {
                send(badRequest(std::move(req)));
            } else {   
                std::cout << "is it here5" <<std::endl;
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
    void handleJoinGame(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        json::error_code ec;
        auto json_body = json::parse(req.body(),ec);
        if (ec) {
            send(badParse(std::move(req)));
            return;
        }
        
        if (!json_body.as_object().contains("mapId")){
            send(mapNotContainsToAuth(std::move(req)));
            return;
        }
        if (!json_body.as_object().contains("userName")){
            send(nameNotContainsToAuth(std::move(req)));
            return;
        }
        std::string map_id_ = std::string(json_body.at("mapId").as_string());
        std::string name = std::string(json_body.at("userName").as_string());
        
        model::Map::Id map_id{map_id_};
        auto map = game_.FindMap(map_id);
        if (!map && !name.empty()) {
            send(invalidMapToAuth(std::move(req)));
            return;
        }

        if (!map && name.empty()) {
            send(invalidDataToAuth(std::move(req)));
            return;
        }

        if (name.empty()) {
            send(badPlayerName(std::move(req)));
            return;
        }
        
        if (auto session_yet = game_.FindGameSessionByMap(*map); session_yet) {
            std::cout<<"yet: " << *(session_yet->GetId())<<std::endl;
            auto player_ = model::Players::AddPlayer(name,session_yet);
            std::cout<<"yet dog id: " << *(player_.GetDog().GetId())<<std::endl;
            std::cout<< "Token"<< *(player_.GetToken())<<std::endl;
            json::object player_json{
                {"authToken",  *(player_.GetToken())},
                {"playerId", *(player_.GetDog().GetId())}
            };
            sendResponseToAuth(std::move(req), std::move(send), player_json);
            
        
        } else {
            std::cout<<"of no"<< std::endl;
            auto& new_session = game_.AddGameSession(*map);
            std::cout<<"new: " << *(new_session.GetId())<<std::endl;
            auto player_ = model::Players::AddPlayer(name, &new_session);
            
            json::object player_json{
                {"authToken",  *(player_.GetToken())},
                {"playerId", *(player_.GetDog().GetId())}
            };
            sendResponseToAuth(std::move(req), std::move(send), player_json);
        }
    }
    template <typename Body, typename Allocator, typename Send>
    void handleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.find("Authorization")==req.end()) {
            send(missingAuthHeaderToAuth(std::move(req)));
            return;
        }
        auto auth_header = req["Authorization"];
        const std::string bearer_prefix = "Bearer ";
        if (auth_header.size() <= bearer_prefix.size() ||
            auth_header.substr(0, bearer_prefix.size()) != bearer_prefix) 
        {
            send(invalidAuthHeaderToAuth(std::move(req)));
            return;
        }
        std::string token_str = std::string(auth_header.substr(bearer_prefix.size()));
        if (token_str.empty() || token_str.size() != 32) {
            send(invalidAuthHeaderToAuth(std::move(req)));
            return;
        }
            Token token{token_str};
            auto player_ = model::Players::findPlayerByToken(token);
            if (!player_) {
                send(badToken(std::move(req)));
            } else {
            json::object players;
            int index = 0;
            for(auto& dog :player_->GetSession().GetDogs()){
                cout << index << dog.GetName() << endl;
                players[std::to_string(index++)] =json::object{
                    {"name", dog.GetName()}
                    };
            }
            sendResponseToAuth(std::move(req), std::move(send), players);
            } 
    }
    template <typename Body, typename Allocator, typename Send>
    void handleGetStateInformation(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        
        if (req.find("Authorization")==req.end()) {
            send(missingAuthHeaderToAuth(std::move(req)));
            return;
        }
        auto auth_header = req["Authorization"];
        const std::string bearer_prefix = "Bearer ";
        if (auth_header.size() <= bearer_prefix.size() ||
            auth_header.substr(0, bearer_prefix.size()) != bearer_prefix) 
        {
            send(invalidAuthHeaderToAuth(std::move(req)));
            return;
        }
        std::cout << "is it here8" <<std::endl;
        std::string token_str = std::string(auth_header.substr(bearer_prefix.size()));
        if (token_str.empty() || token_str.size() != 32) {
            send(invalidAuthHeaderToAuth(std::move(req)));
            return;
        }
            Token token{token_str};
            auto player_ = model::Players::findPlayerByToken(token);
            if (!player_) {
                send(badToken(std::move(req)));
            } else {
            json::object players;
            //ОШИБКА СЕГМЕНТИРОВАНИЯ КОГДА 2 ПЛЕЕРА std::cout << "ses" << *(player_->GetSession().GetId()) << std::endl;
            for(auto& dog :player_->GetSession().GetDogs()){
                std::cout << "is" << *(dog.GetId()) << std::endl;
                players[std::to_string(*(dog.GetId()))] = json::object{
                    {"pos", json::array{dog.GetCoordinate().x, dog.GetCoordinate().y}},
                    {"speed",json::array{ dog.GetSpeed().vx, dog.GetSpeed().vy}},
                    {"dir", dog.GetDirection()}
                    };
            }
            json::object response;
            response["players"] = players;
            sendResponseToAuth(std::move(req), std::move(send), response); 
            } 
    }
    template <typename Body, typename Allocator, typename Send>
    void handleAction(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.find("Authorization")==req.end()) {
            send(missingAuthHeaderToAuth(std::move(req)));
            return;
        }
        auto auth_header = req["Authorization"];
        const std::string bearer_prefix = "Bearer ";
        if (auth_header.size() <= bearer_prefix.size() ||
            auth_header.substr(0, bearer_prefix.size()) != bearer_prefix) 
        {
            send(invalidAuthHeaderToAuth(std::move(req)));
            return;
        }
        std::string token_str = std::string(auth_header.substr(bearer_prefix.size()));
        if (token_str.empty() || token_str.size() != 32) {
            send(invalidAuthHeaderToAuth(std::move(req)));
            return;
        }
            Token token{token_str};
            auto player_ = model::Players::findPlayerByToken(token);
            if (!player_) {
                send(badToken(std::move(req)));
                return;
            } else {
                json::error_code ec;
                auto json_body = json::parse(req.body(),ec);
                if (ec) {
                    send(badParse(std::move(req)));
                    return;
                }
                std::string move_key = std::string(json_body.at("move").as_string());
                if (move_key=="L") {
                    player_->GetDog().SetDirection(model::Direction::WEST);
                    player_->GetDog().SetSpeed(model::Speed(-(player_->GetSession().GetMap().GetSpeed().vx),0));
                } else if (move_key=="R") {
                    player_->GetDog().SetDirection(model::Direction::EAST);
                    player_->GetDog().SetSpeed(model::Speed(player_->GetSession().GetMap().GetSpeed().vx, 0));
                } else if (move_key=="U") {
                    player_->GetDog().SetDirection(model::Direction::NORTH);
                    player_->GetDog().SetSpeed(model::Speed(0,-(player_->GetSession().GetMap().GetSpeed().vy)));
                } else if (move_key=="D") {
                    player_->GetDog().SetDirection(model::Direction::SOUTH);
                    player_->GetDog().SetSpeed(model::Speed(0,player_->GetSession().GetMap().GetSpeed().vy));
                } else if (move_key.empty()){
                    player_->GetDog().SetSpeed(model::Speed(0,0));
                }
                json::object response;
                sendResponseToAuth(std::move(req), std::move(send), response); 
            }
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

    template <typename Body, typename Allocator, typename Send, typename Json>
    void sendResponseToAuth(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, Json&& json_response) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");
        res.body() = json::serialize(json_response);
        res.content_length(res.body().size());
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
    http::response<http::string_body> mapNotContainsToAuth(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidArgument"},
            {"message", "Map not found"}};

        return createErrorResponseToAuth(std::move(req), http::status::bad_request, error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> nameNotContainsToAuth(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidArgument"},
            {"message", "Name not found"}};

        return createErrorResponseToAuth(std::move(req), http::status::bad_request, error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> invalidMapToAuth(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "mapNotFound"},
            {"message", "Map not found"}};

        return createErrorResponseToAuth(std::move(req), http::status::not_found, error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> invalidDataToAuth(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidArgument"},
            {"message", "Map not found"}};

        return createErrorResponseToAuth(std::move(req), http::status::bad_request, error_response);
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> badRequest(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "badRequest"},
            {"message", "Bad request"}};

        return createErrorResponse(std::move(req), http::status::bad_request, error_response);
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> badAuth(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidToken"},
            {"message", "Authorization header is missing"}};

        return createErrorResponseToAuth(std::move(req), http::status::unauthorized, error_response);
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> badToken(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "unknownToken"},
            {"message", "Player token has not been found"}};

        return createErrorResponseToAuth(std::move(req), http::status::unauthorized, error_response);
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> notFound(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "notFound"},
            {"message", "Not found"}};

        return createErrorResponse(std::move(req), http::status::not_found, error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> badPlayerName(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidArgument"},
            {"message", "Invalid name"}};

        return createErrorResponseToAuth(std::move(req), http::status::bad_request, error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> badParse(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidArgument"},
            {"message", "Join game request parse error"}};

        return createErrorResponseToAuth(std::move(req), http::status::bad_request, error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> badMethodNotPost(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidMethod"},
            {"message", "Only POST method is expected"}};

        return createErrorResponseToAuthToAllow(std::move(req), http::status::method_not_allowed, "POST", error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> badMethodNotGetOrHead(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidMethod"},
            {"message", "Invalid method"}};

        return createErrorResponseToAuthToAllow(std::move(req), http::status::method_not_allowed, "GET, HEAD", error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> missingAuthHeaderToAuth(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidToken"},
            {"message", "Authorization header is missing"}};

        return createErrorResponseToAuth(std::move(req), http::status::unauthorized, error_response);
    }
    template <typename Body, typename Allocator>
    http::response<http::string_body> invalidAuthHeaderToAuth(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidToken"},
            {"message", "Player token has not been found"}};

        return createErrorResponseToAuth(std::move(req), http::status::unauthorized, error_response);
    }
    template <typename Body, typename Allocator, typename Json>
    http::response<http::string_body> createErrorResponse(http::request<Body, http::basic_fields<Allocator>>&& req, http::status status, Json&& json_response) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());  
        res.body() = json::serialize(json_response);
        return res;
    }

    template <typename Body, typename Allocator, typename Json>
    http::response<http::string_body> createErrorResponseToAuth(http::request<Body, http::basic_fields<Allocator>>&& req, http::status status, Json&& json_response) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, "application/json");
        //res.set(http::field::content_length, std::to_string(res.body().size()));
        res.set(http::field::cache_control, "no-cache");
        res.keep_alive(req.keep_alive());  
        res.body() = json::serialize(json_response);
        res.content_length(res.body().size());
        return res;
    }

    template <typename Body, typename Allocator, typename Json>
    http::response<http::string_body> createErrorResponseToAuthToAllow(http::request<Body, http::basic_fields<Allocator>>&& req, http::status status, std::string allowed_type, Json&& json_response) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, "application/json");
        //res.set(http::field::content_length, std::to_string(res.body().size()));
        res.set(http::field::cache_control, "no-cache");
        res.set(http::field::allow, allowed_type);
        res.body() = json::serialize(json_response);    
        res.content_length(res.body().size());
        return res;
    }
    model::Players players_;
    model::Game& game_;
    std::string path_;
};
    
}  // namespace http_handler
