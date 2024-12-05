#pragma once
#include "http_server.h"
#include "model.h"
#include "loot_generator.h"
#include "model_serialization.h"
#include <filesystem>
#include <cassert>
#include <iostream>
#include <locale>
#include <string>
#include <fstream> 
#include <boost/beast.hpp>
#include <pqxx/pqxx>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace sys = boost::system;
using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;
using QueryParams = std::unordered_map<std::string, std::string>;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, loot_gen::LootGenerator& loot_gen, std::string path_static, 
                            bool random_spawn, std::chrono::milliseconds save_interval, fs::path& serialization_file_path,
                            bool have_state_period , boost::asio::strand<boost::asio::io_context::executor_type>& strand)
        : game_{game},
        loot_gen_{loot_gen},
        path_{path_static},
        random_spawn_{random_spawn},
        save_interval_{save_interval},
        have_state_period_{have_state_period},
        time_since_last_save_{0},
        serialization_file_path_{serialization_file_path},
        strand_{strand} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    boost::asio::dispatch(strand_, [this, req = std::move(req), send = std::move(send)]() mutable {
        try {
            if (req.method() == http::verb::get && req.target() == "/api/v1/maps") {
                handleGetMaps(std::move(req), std::move(send));
            } else if (req.target().starts_with("/api/v1/maps/")) {
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    handleGetMapById(std::move(req), std::move(send));
                } else{
                    send(badMethodNotGetOrHead(std::move(req)));
                }
            } else if (req.target() == "/api/v1/game/join") {
                if (req.method() == http::verb::post){
                    handleJoinGame(std::move(req), std::move(send));
                } else {
                    send(badMethodNotPost(std::move(req)));
                }
            } else if (req.target() == "/api/v1/game/players") {
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    handleGetPlayers(std::move(req), std::move(send));
                } else{
                    send(badMethodNotGetOrHead(std::move(req)));
                }
            }else if (req.target() == "/api/v1/game/state") {
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    handleGetStateInformation(std::move(req), std::move(send));
                } else{
                    send(badMethodNotGetOrHead(std::move(req)));
                }
            }else if (req.target() == "/api/v1/game/player/action") {
                if (req.method() == http::verb::post){
                    handleAction(std::move(req), std::move(send));
                } else {
                    send(badMethodNotPost(std::move(req)));
                }
            } else if (req.target() == "/api/v1/game/tick") {
                if (req.method() == http::verb::post) {
                    handleMovesTick(std::move(req), std::move(send));
                } else {
                    send(badMethodNotPost(std::move(req)));
                }
            } else if (req.target() == "/api/v1/game/records"){
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    handleGetRecords(std::move(req), std::move(send));
                } else{
                    send(badMethodNotGetOrHead(std::move(req)));
                }

            }else if (req.target().starts_with("/api/")) {
                send(badRequest(std::move(req)));
            } else {    
                handleRequest(std::move(req), std::move(send));
            }
        } catch (std::exception& e) {
            std::cerr << "Error handling request: " << e.what() << std::endl;
        }        
    });
    }

    void Tick(int delta){
        std::vector<collision_detector::Item> items;
        std::vector<collision_detector::Item> offices;
        std::vector<collision_detector::Gatherer> gatherers_;
        
        for (auto& session : game_.GetGameSessions()) {
            session->AddSessionTime(delta * 0.001);
            cout << session->GetSessionTime() << endl;
            for (auto it = session->GetDogs().begin(); it < session->GetDogs().end(); ++it) {
                auto& dog = *it;
                std::cout << *(dog->GetId()) << std::endl;
                cout << dog->GetRetirementTime() << endl;
                if (dog->GetRetirementTime() >= game_.GetDogRetirementTime()){
                    const char* db_url = std::getenv("GAME_DB_URL");
                    //postgres://postgres:Mys3Cr3t@localhost:30432/records
                    try {
                        // Создаем новое подключение к базе данных
                        pqxx::connection conn(db_url);

                        // Начинаем транзакцию
                        pqxx::work txn(conn);

                        // Генерируем UUID
                        boost::uuids::uuid id = boost::uuids::random_generator()();
                        std::string id_str = boost::uuids::to_string(id);

                        // Получаем данные игрока
                        std::string name = dog->GetName();
                        int score = dog->GetScore();
                        double play_time_ms = session->GetSessionTime() - dog->GetTimeInGame();
                        cout << play_time_ms << " " << session->GetSessionTime() << " " << dog->GetTimeInGame() << endl;
                        // Вставляем данные в таблицу
                        txn.exec_params(
                            "INSERT INTO retired_players (id, name, score, play_time_ms) "
                            "VALUES ($1::UUID, $2, $3, $4)",
                            id_str, name, score, play_time_ms
                        );

                        txn.commit();
                    } catch (const std::exception& e) {
                        std::cerr << "Не удалось добавить запись об игроке в базу данных: " << e.what() << std::endl;
                    }
                    std::cout << *(dog->GetId()) << std::endl;
                    players_.DeletePlayer(players_.FindPlayerByDog(dog->GetId()));
                    session->DeleteDog(dog);
                } else {
                    collision_detector::Gatherer gatherer_;
                    gatherer_.start_pos = dog->GetCoordinate();
                    gatherer_.width = 0.3;
                    
                    dog->AddInGameTime(delta * 0.001);
                    if (dog->GetSpeed().vx==0 && dog->GetSpeed().vy==0){
                        dog->AddRetirementTime(delta * 0.001);
                    }else {
                        UpdateCoords(delta*0.001, session, dog);
                        dog->CancelRetirementTime();
                    }
                    gatherer_.end_pos = dog->GetCoordinate();
                    gatherers_.emplace_back(std::move(gatherer_));  
                }
            }

            unsigned generated_count = loot_gen_.Generate(game_.GetLootPeriod(), session->GetLostObjects().size(), session->GetDogs().size());
            for (unsigned i=0; i<generated_count; ++i){
                session->AddLostObject();
            }

            for (auto& object : session->GetLostObjects()){
                collision_detector::Item item_(object->GetCoordinate(), 0);
                items.emplace_back(std::move(item_));
            }
            for (auto& office : session->GetMap().GetOffices()){
                collision_detector::Item item_({office.GetPosition().x, office.GetPosition().y}, 0.5);
                offices.emplace_back(std::move(item_));
            }

            model::VectorItemGathererProvider item_provider(items, gatherers_);
            std::vector<collision_detector::GatheringEvent> item_events=collision_detector::FindGatherEvents(item_provider);
            model::VectorItemGathererProvider office_provider(offices, gatherers_);
            std::vector<collision_detector::GatheringEvent> office_events=collision_detector::FindGatherEvents(office_provider);
            int previous_item_event_id = -1 ;
            for (auto& event : item_events){
                if (previous_item_event_id == event.item_id){
                    continue;
                }else{
                    model::LostObject::Id object_id{event.item_id};
                    model::Dog::Id dog_id{event.gatherer_id};
                    if (session->GetMap().GetBagCapacity() > session->FindDog(dog_id)->GetBagObjects().size()){
                        session->FindDog(dog_id)->AddBagObject(session->GetLostObjects()[event.item_id]);
                        session->DeleteLostObjects(event.item_id);
                    }
                    previous_item_event_id = event.item_id;
                }
            }
            for (auto& event : office_events){
                model::Dog::Id dog_id{event.gatherer_id};
                session->FindDog(dog_id)->DeleteBagObjects();
            }
        }
        if (have_state_period_){
            time_since_last_save_+= std::chrono::milliseconds(delta);
            if (time_since_last_save_>= save_interval_){
                SaveStateInformation();
                time_since_last_save_-= save_interval_;
            }
        }
    }

    void SaveStateInformation(){
        fs::path filename = serialization_file_path_.filename();
        auto extension = filename.extension();
        auto tmp_path = serialization_file_path_.parent_path().append("tmp");
        tmp_path += extension;
        std::ofstream ofs(tmp_path);
        boost::archive::text_oarchive serialization_archive_to_write{ofs};
        serialization::SerializationObj s_object(game_.GetGameSessions(), players_.GetPlayers());
        serialization_archive_to_write << s_object;
        fs::rename(tmp_path, serialization_file_path_);
        fs::remove(tmp_path);
    }
    void LoadStateInformation(InputArchive& input_archive_){
        serialization::SerializationObj s_object;
        input_archive_ >> s_object;
        for (auto& session : s_object.game_sessions_){
            auto restored_session = session->Restore();
            restored_session.SetMapIdForSerialize(*game_.FindMap(restored_session.GetMap().GetId()));
            game_.AddYetGameSession(restored_session);
        }
        for (auto& player : s_object.players_){
            auto restored_player = player->Restore();
            restored_player.SetSessionForSerialize(*game_.FindGameSessionByMap(restored_player.GetSession()->GetMap()));
            restored_player.SetDogForSerialize(restored_player.GetSession()->FindDog(restored_player.GetDog()->GetId()));
            players_.AddYetPlayer(restored_player);
        }
    }

private:
    template <typename Body, typename Allocator, typename Send>
    void handleGetRecords(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        int start = 0;
        int maxItems = 100;

        // Получаем параметры запроса
        auto query_params = GetQueryParams(std::move(req));

        if (query_params.count("start")) {
            start = std::stoi(query_params.at("start"));
            if (start < 0) start = 0;
        }

        if (query_params.count("maxItems")) {
            maxItems = std::stoi(query_params.at("maxItems"));
            if (maxItems < 0) maxItems = 0;
            if (maxItems > 100) {
                send(badRequest(std::move(req)));
                return;
            }
        }
        const char* db_url = std::getenv("GAME_DB_URL");


        pqxx::connection conn(db_url);
        pqxx::work txn(conn);

        pqxx::result r = txn.exec_params(
            "SELECT name, score, play_time_ms "
            "FROM retired_players "
            "ORDER BY score DESC, play_time_ms ASC, name ASC "
            "LIMIT $1 OFFSET $2",
            maxItems, start
        );

        // Формируем JSON-ответ
        json::array records;
        for (const auto& row : r) {
            records.push_back(json::object{
                {"name", row["name"].as<std::string>()},
                {"score", row["score"].as<int>()},
                {"playTime", row["play_time_ms"].as<double>()}
            });
        }
        sendResponseToAuth(std::move(req), std::move(send), records);
    }
 
    template <typename Body, typename Allocator>
    QueryParams GetQueryParams(http::request<Body, http::basic_fields<Allocator>>&& req) {
        QueryParams query_params;

        // Получаем целевой URL
        
        auto target = req.target();
        std::string decoded_url = url_decode(std::string(target.data(), target.size()));
        // Находим позицию '?'
        std::size_t pos = decoded_url.find('?');

        if (pos != std::string::npos && pos + 1 < decoded_url.size()) {
            // Извлекаем строку запроса
            std::string query = decoded_url.substr(pos + 1);

            // Разбиваем строку по '&'
            std::vector<std::string> params;
            boost::split(params, query, boost::is_any_of("&"));

            for (const auto& param : params) {
                // Разделяем каждый параметр по символу '='
                std::vector<std::string> kv;
                boost::split(kv, param, boost::is_any_of("="));

                if (kv.size() == 2) {
                    std::string key = kv[0];
                    std::string value = kv[1];
                    // Добавляем пару ключ-значение в карту параметров
                    query_params[key] = value;
                } else if (kv.size() == 1) {
                    // Если значение отсутствует, устанавливаем пустую строку
                    std::string key = kv[0];
                    query_params[key] = "";
                }
            }
        }
        return query_params;
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
        sendResponseToAuth(std::move(req), std::move(send), maps_list_);
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
            auto player_ = players_.AddPlayer(name,*session_yet,random_spawn_);
            json::object player_json{
                {"authToken",  *(players_.players_.back().get()->GetToken())},
                {"playerId", *(players_.players_.back().get()->GetDog()->GetId())}
            };
            sendResponseToAuth(std::move(req), std::move(send), player_json);
            
        
        } else {
            auto new_session = game_.AddGameSession(*map);
            auto player_ = players_.AddPlayer(name, new_session, random_spawn_);
            json::object player_json{
                {"authToken",  *(players_.players_.back().get()->GetToken())},
                {"playerId", *(players_.players_.back().get()->GetDog()->GetId())}
            };
            sendResponseToAuth(std::move(req), std::move(send), player_json);
        }
    }
    template <typename Body, typename Allocator, typename Send>
    void handleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto target = req.target();
        std::string requested_path_in_str;
        if (target == "/") {
            requested_path_in_str = path_ + "/index.html";
        } else {
            auto decoded_url = url_decode(std::string(target.data(), target.size()));
            requested_path_in_str = path_ + decoded_url;
        }
        fs::path requested_path(requested_path_in_str);
        requested_path =  fs::weakly_canonical(requested_path);
        fs::path root_path(path_);
        root_path =  fs::weakly_canonical(root_path);

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
        res.set(http::field::content_type, get_mime_type(requested_path.extension().string()));
        res.content_length(size);
        res.body() = std::move(file);
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        send(std::move(res));
    }
    template <typename Body, typename Allocator, typename Send>
    void handleAction(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.find("Authorization") == req.end()) {
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
            auto player_ = players_.findPlayerByToken(token);
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
                    player_->GetDog().get()->SetDirection(model::Direction::WEST);
                    player_->GetDog().get()->SetSpeed(model::Speed(-(player_->GetSession().get()->GetMap().GetSpeed().vx),0));
                } else if (move_key=="R") {
                    player_->GetDog().get()->SetDirection(model::Direction::EAST);
                    player_->GetDog().get()->SetSpeed(model::Speed(player_->GetSession().get()->GetMap().GetSpeed().vx, 0));
                } else if (move_key=="U") {
                    player_->GetDog().get()->SetDirection(model::Direction::NORTH);
                    player_->GetDog().get()->SetSpeed(model::Speed(0,-(player_->GetSession().get()->GetMap().GetSpeed().vy)));
                } else if (move_key=="D") {
                    player_->GetDog().get()->SetDirection(model::Direction::SOUTH);
                    player_->GetDog().get()->SetSpeed(model::Speed(0,player_->GetSession().get()->GetMap().GetSpeed().vy));
                } else if (move_key.empty()){
                    player_->GetDog().get()->SetSpeed(model::Speed(0,0));
                }
                std::cout << player_->GetDog()->GetDirection() << std::endl;
                json::object response;
                sendResponseToAuth(std::move(req), std::move(send), response); 
            }
    }

    template <typename Body, typename Allocator, typename Send>
    void handleGetStateInformation(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.find("Authorization") == req.end()) {
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
            auto player_ = players_.findPlayerByToken(token);
            
            if (!player_) {
                send(badToken(std::move(req)));
            } else {
            json::object players;
            json::object lost_objects;
            for(auto dog :player_->GetSession()->GetDogs()){  
                json::array bag_;
                for (auto find_obj : dog->GetBagObjects()){
                    bag_.push_back(json::object{
                        {"id", *(find_obj.get()->GetId())},
                        {"type", find_obj.get()->GetType()}
                    });
                }
                players[std::to_string(*(dog.get()->GetId()))] = json::object{
                    {"pos", json::array{dog.get()->GetCoordinate().x, dog.get()->GetCoordinate().y}},
                    {"speed",json::array{ dog.get()->GetSpeed().vx, dog.get()->GetSpeed().vy}},
                    {"dir", dog->GetDirection()},
                    {"bag", bag_},
                    {"score", dog->GetScore()}
                    };
                
            }
            for (auto& lost_obj : player_->GetSession()->GetLostObjects()){
                lost_objects[std::to_string(*(lost_obj.get()->GetId()))] = json::object{
                    {"type", lost_obj.get()->GetType()},
                    {"pos", json::array{lost_obj.get()->GetCoordinate().x, lost_obj.get()->GetCoordinate().y}}
                };
            }
            json::object response;
            response["players"] = players; 
            response["lostObjects"] = lost_objects; 
            sendResponseToAuth(std::move(req), std::move(send), response); 
            } 
    }

    template <typename Body, typename Allocator, typename Send>
    void handleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.find("Authorization") == req.end()) {
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
            auto player_ = players_.findPlayerByToken(token);
            if (!player_) {
                send(badToken(std::move(req)));
            } else {
            json::object players;
            int index = 0;
            for(auto& dog :player_->GetSession().get()->GetDogs()){
                players[std::to_string(index++)] = json::object{
                    {"name", dog.get()->GetName()}
                    };
            }
            sendResponseToAuth(std::move(req), std::move(send), players);
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
            {"offices", toJson(map->GetOffices())},
            {"lootTypes", map->GetExtraData().loot_types_}
        };

        sendResponseToAuth(std::move(req), std::move(send), map_json);
    }

    template <typename Body, typename Allocator, typename Send>
    void handleMovesTick(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        json::error_code ec;
        auto json_body = json::parse(req.body(),ec);
        if (ec) {
            send(badParse(std::move(req)));
            return;
        }
        if (!json_body.as_object().contains("timeDelta") || !json_body.at("timeDelta").is_int64()) {
            send(badTickParse(std::move(req)));
            return; 
        }
        auto time_delta = (json_body.at("timeDelta").as_int64());
        Tick(time_delta);
        json::object response;
        sendResponseToAuth(std::move(req), std::move(send), response);
    }

    bool IsSubPath(fs::path path, fs::path base) {
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
    void UpdateCoords(double time_delta, model::Game::GameSessionPointer& session, const model::GameSession::DogPointer& dog) {
        auto cur_road = dog->GetCurrentRoad(session.get()->GetMap().GetRoads(),dog->GetCoordinate());
        if (dog->GetDirectionENUM() == model::Direction::NORTH || dog->GetDirectionENUM() == model::Direction::SOUTH) {
            geom::Point2D new_coord = {dog->GetCoordinate().x, dog->GetCoordinate().y + time_delta * dog->GetSpeed().vy};
            if (cur_road->IsHorizontal()){
                auto new_vertical_road_up =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {static_cast<double>(dog->GetCoordinate().x), static_cast<double>(cur_road->GetStart().y+1)});
                auto new_vertical_road_down =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), { static_cast<double>(dog->GetCoordinate().x), static_cast<double>(cur_road->GetStart().y-1)});
                if (!(new_coord.y <= cur_road->GetStart().y + 0.4)){
                    if (new_vertical_road_up == nullptr){
                        dog->SetCoordinateY(cur_road->GetStart().y + 0.4);
                        dog->SetSpeed({0,0});
                    }

                    while (!(new_vertical_road_up == nullptr)) {
                        double max_coord =std::max(new_vertical_road_up->GetStart().y,new_vertical_road_up->GetEnd().y);
                        if(new_coord.y >= max_coord+0.4){
                            dog->SetCoordinateY(max_coord+0.4);
                            dog->SetSpeed({0,0});
                            auto new_vertical_road_up =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {dog->GetCoordinate().x, max_coord+1});
                        } else {
                            dog->SetCoordinateY(new_coord.y);
                            break;
                        }
                    } 
                } else if (!(new_coord.y >= cur_road->GetStart().y - 0.4)) {
                    if (new_vertical_road_down == nullptr){
                        dog->SetCoordinateY(cur_road->GetStart().y - 0.4);
                        dog->SetSpeed({0,0});
                    }

                    while (!(new_vertical_road_down == nullptr)) {
                        double min_coord =std::min(new_vertical_road_down->GetStart().y,new_vertical_road_down->GetEnd().y);
                        if (new_coord.y <= min_coord - 0.4) {
                            dog->SetCoordinateY(min_coord-0.4);
                            dog->SetSpeed({0,0});
                            auto new_vertical_road_down =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {dog->GetCoordinate().x, min_coord-1});
                        } else {
                            dog->SetCoordinateY(new_coord.y);
                            break;
                        }
                    }
                    
                } else {
                    dog->SetCoordinateY(new_coord.y);
                }
            } else if (cur_road->IsVertical()){
                double max_coord_cur_y =std::max(cur_road->GetStart().y,cur_road->GetEnd().y); 
                double min_coord_cur_y =std::min(cur_road->GetStart().y,cur_road->GetEnd().y); 
                auto new_vertical_road_up =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {dog->GetCoordinate().x, max_coord_cur_y+1});
                auto new_vertical_road_down =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {dog->GetCoordinate().x, min_coord_cur_y-1});
                if(!(new_coord.y <= max_coord_cur_y+0.4)){
                    if (new_vertical_road_up == nullptr){
                        dog->SetCoordinateY(max_coord_cur_y + 0.4);
                        dog->SetSpeed({0,0});
                    }

                    while (!(new_vertical_road_up == nullptr)) {
                        double max_coord_new_y =std::max(new_vertical_road_up->GetStart().y,new_vertical_road_up->GetEnd().y);
                        if (new_coord.y >= max_coord_new_y+0.4) {
                            dog->SetCoordinateY(max_coord_new_y+0.4);
                            dog->SetSpeed({0,0});
                            auto new_vertical_road_up =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {dog->GetCoordinate().x, max_coord_new_y+1});
                        } else {
                            dog->SetCoordinateY(new_coord.y);
                            break;
                        }
                    } 
                    
                } else if(!(new_coord.y >= min_coord_cur_y-0.4)){
                    if (new_vertical_road_down == nullptr) {
                        dog->SetCoordinateY(min_coord_cur_y - 0.4);
                        dog->SetSpeed({0,0});
                    }

                    while (!(new_vertical_road_down == nullptr)) {
                        double min_coord_new_y =std::min(new_vertical_road_down->GetStart().y,new_vertical_road_down->GetEnd().y);
                        if (new_coord.y <= min_coord_new_y - 0.4) {
                            dog->SetCoordinateY(min_coord_new_y-0.4);
                            dog->SetSpeed({0,0});
                            auto new_vertical_road_down =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {dog->GetCoordinate().x, min_coord_new_y-1});
                        } else {
                            dog->SetCoordinateY(new_coord.y);
                            break;
                        }
                    } 
                    
                } else{
                    dog->SetCoordinateY(new_coord.y);
                }
            }
        } else if (dog->GetDirectionENUM() == model::Direction::EAST || dog->GetDirectionENUM() == model::Direction::WEST) {
            geom::Point2D new_coord = {dog->GetCoordinate().x + time_delta * dog->GetSpeed().vx, dog->GetCoordinate().y};
            if (cur_road->IsVertical()) { 
                auto new_horizontal_road_right =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), { static_cast<double>(cur_road->GetStart().x+1), static_cast<double>(dog->GetCoordinate().y)});
                auto new_horizontal_road_left =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), { static_cast<double>(cur_road->GetStart().x-1), static_cast<double>(dog->GetCoordinate().y) });
                if (!(new_coord.x <= cur_road->GetStart().x + 0.4)){
                    if (new_horizontal_road_right == nullptr)  {
                        dog->SetCoordinateX(cur_road->GetStart().x + 0.4);
                        dog->SetSpeed({0,0});
                    }

                    while (!(new_horizontal_road_right == nullptr)) {
                        double max_coord_right_x =std::max(new_horizontal_road_right->GetStart().x,new_horizontal_road_right->GetEnd().x);
                        if(new_coord.x >= max_coord_right_x+0.4){
                            dog->SetCoordinateX(max_coord_right_x+0.4);
                            dog->SetSpeed({0,0});
                            auto new_horizontal_road_right =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {max_coord_right_x+1, dog->GetCoordinate().y});
                        } else {
                            dog->SetCoordinateX(new_coord.x);
                            break;
                        }
                    } 
                    
                } else if (!(new_coord.x >= cur_road->GetStart().x - 0.4)) {
                    if (new_horizontal_road_left == nullptr) {
                        dog->SetCoordinateX(cur_road->GetStart().x - 0.4);
                        dog->SetSpeed({0,0});
                    }

                    while (!(new_horizontal_road_left == nullptr)) {
                        double min_coord_left_x =std::min(new_horizontal_road_left->GetStart().x,new_horizontal_road_left->GetEnd().x);
                        if (new_coord.x <= min_coord_left_x - 0.4) {
                            dog->SetCoordinateX(min_coord_left_x-0.4);
                            dog->SetSpeed({0,0});
                            auto new_horizontal_road_left =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {min_coord_left_x-1, dog->GetCoordinate().y });
                        } else {
                            dog->SetCoordinateX(new_coord.x);
                            break;
                        }
                    }
                    
                } else {
                    dog->SetCoordinateX(new_coord.x);
                }
            } else if (cur_road->IsHorizontal()){
                double max_coord_cur_x =std::max(cur_road->GetStart().x,cur_road->GetEnd().x); 
                double min_coord_cur_x =std::min(cur_road->GetStart().x,cur_road->GetEnd().x); 
                auto new_horizontal_road_right =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {max_coord_cur_x+1, dog->GetCoordinate().y});
                auto new_horizontal_road_left =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {min_coord_cur_x-1, dog->GetCoordinate().y});
                if(!(new_coord.x <= max_coord_cur_x+0.4)){
                    if (new_horizontal_road_right == nullptr) {
                        dog->SetCoordinateX(max_coord_cur_x + 0.4);
                        dog->SetSpeed({0,0});
                    }
                    while (!(new_horizontal_road_right == nullptr)) {
                        double max_coord_new_x =std::max(new_horizontal_road_right->GetStart().x,new_horizontal_road_right->GetEnd().x);
                        if (new_coord.x >= max_coord_new_x+0.4) {
                            dog->SetCoordinateX(max_coord_new_x+0.4);
                            dog->SetSpeed({0,0});
                            auto new_horizontal_road_right =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {max_coord_new_x+1, dog->GetCoordinate().y});
                        } else {
                            dog->SetCoordinateX(new_coord.x);
                            break;
                        }
                    } 
                    
                } else if(!(new_coord.x >= min_coord_cur_x-0.4)){
                    if (new_horizontal_road_left == nullptr) {
                        dog->SetCoordinateX(min_coord_cur_x - 0.4);
                        dog->SetSpeed({0,0});
                    }
                    
                    while (!(new_horizontal_road_left == nullptr)) {
                        double min_coord_new_x =std::min(new_horizontal_road_left->GetStart().x,new_horizontal_road_left->GetEnd().x);
                        if (new_coord.x <= min_coord_new_x - 0.4) {
                            dog->SetCoordinateX(min_coord_new_x-0.4);
                            dog->SetSpeed({0,0});
                            auto new_horizontal_road_left =dog->GetCurrentRoad(session.get()->GetMap().GetRoads(), {min_coord_new_x-1, dog->GetCoordinate().y});
                        } else {;
                            dog->SetCoordinateX(new_coord.x);
                            break;
                        }
                    } 
                    
                } else{
                    dog->SetCoordinateX(new_coord.x);
                }
            }
        }
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
    http::response<http::string_body> badTickParse(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "invalidArgument"},
            {"message", "Failed to parse tick request JSON"}};

        return createErrorResponseToAuth(std::move(req), http::status::bad_request, error_response);
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> mapNotFound(http::request<Body, http::basic_fields<Allocator>>&& req) {
        json::object error_response{
            {"code", "mapNotFound"},
            {"message", "Map not found"}};

        return createErrorResponseToAuth(std::move(req), http::status::not_found, error_response);
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
        res.set(http::field::cache_control, "no-cache");
        res.set(http::field::allow, allowed_type);
        res.body() = json::serialize(json_response);    
        res.content_length(res.body().size());
        return res;
    }

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

    model::Players players_;
    model::Game& game_;
    fs::path serialization_file_path_;
    std::string path_;
    bool random_spawn_;
    bool have_state_period_;
    std::chrono::milliseconds save_interval_;
    std::chrono::milliseconds time_since_last_save_;
    boost::asio::strand<boost::asio::io_context::executor_type>& strand_;
    loot_gen::LootGenerator loot_gen_;
};
    
}  // namespace http_handler
