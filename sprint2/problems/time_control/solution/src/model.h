#pragma once

#include <boost/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <iostream>  // for debugging
#include <algorithm>
#include "tagged.h"

namespace detail {
struct TokenTag {};
}  // namespace detail
using Token = util::Tagged<std::string, detail::TokenTag>;

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};
// Структура для координат псов
struct Coordinate {
    double x; // Координата по оси X
    double y; // Координата по оси Y

    // Конструктор с параметрами по умолчанию
    Coordinate(double x_val = 0.0, double y_val = 0.0) noexcept
        : x(x_val), y(y_val) {}
};
// Структура для скорости псов
struct Speed {
    double vx; // Скорость по оси X (единиц карты в секунду)
    double vy; // Скорость по оси Y (единиц карты в секунду)

    // Конструктор с параметрами по умолчанию
    Speed(double vx_val = 1.0, double vy_val=1.0) noexcept
        : vx(vx_val), vy(vy_val) {}
};
// Перечисление для направления
enum class Direction {
    NORTH, // Север
    SOUTH, // Юг
    EAST,  // Восток
    WEST   // Запад
};
struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};
class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {}

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    const Speed& GetSpeed() const noexcept {
        return default_map_dog_speed;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    void SetSpeed(const Speed& speed) noexcept {
        default_map_dog_speed = speed;
    }
    
private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    Speed default_map_dog_speed;
};

class PlayerTokens {
private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

public:
    Token generateToken() {
        auto num1 = generator1_();
        auto num2 = generator2_();
        
        std::stringstream token_stream;
        token_stream << std::hex << std::setfill('0')
                     << std::setw(16) << num1
                     << std::setw(16) << num2;
        
        return Token(token_stream.str());
    }
};
class Dog{
public:
    using Id = util::Tagged<int, Dog>;
    explicit Dog(Id id, std::string name, Coordinate coordinate, Speed speed, Direction direction) noexcept
        : id_(std::move(id)),
          name_(std::move(name)),
          coordinate_(std::move(coordinate)),
          speed_(std::move(speed)),
          direction_(direction) {}
    const Id& GetId() const noexcept {
        return id_;
    }const std::string& GetName() const noexcept {
        return name_;
    }
    void SetCoordinateX(double coord) noexcept {
        coordinate_.x = coord;
    }
    void SetCoordinateY(double coord) noexcept {
        coordinate_.y = coord;
    }
    void SetSpeed(const Speed& speed) noexcept {
        speed_ = speed;
    }
    void SetDirection(Direction direction) noexcept {
        direction_ = direction;
    }
    const Coordinate& GetCoordinate() const noexcept {
        return coordinate_;
    }

    const Speed& GetSpeed() const noexcept {
        return speed_;
    }

    std::string GetDirection() const noexcept {
        if (direction_ == Direction::EAST) {
            return "R";
        } else if (direction_ == Direction::WEST) {
            return "L";
        } else if (direction_ == Direction::NORTH) {
            return "U";
        } else if (direction_ == Direction::SOUTH) {
            return "D";
        }
        return "";
    }
    const Direction& GetDirectionENUM() const noexcept {
        return direction_;
    }

    const Road* GetCurrentRoad(const std::vector<Road>& roads, const Coordinate& coordinate) const {
        for (const auto& road : roads) {
            if (road.IsHorizontal()){
                double max_coord_cur_x =std::max(road.GetStart().x,road.GetEnd().x); 
                double min_coord_cur_x =std::min(road.GetStart().x,road.GetEnd().x); 
                if ((coordinate.y <= road.GetStart().y + 0.4 && coordinate.y >= road.GetStart().y - 0.4) &&
                    coordinate.x >= min_coord_cur_x - 0.4 && coordinate.x <= max_coord_cur_x + 0.4) {
                    return &road;
                }
            }
            if (road.IsVertical()){
                double max_coord_cur_y =std::max(road.GetStart().y,road.GetEnd().y); 
                double min_coord_cur_y =std::min(road.GetStart().y,road.GetEnd().y); 
                if ((coordinate.x <= road.GetStart().x + 0.4 && coordinate.x >= road.GetStart().x - 0.4) &&
                    coordinate.y >= min_coord_cur_y - 0.4 && coordinate.y <= max_coord_cur_y +0.4) {
                    return &road;
                }
            }
             // Собака не на дороге
        }
        return nullptr;
    }
private:
    Id id_;
    std::string name_;
    Coordinate coordinate_; // Координаты пса на карте
    Speed speed_;           // Скорость пса на карте
    Direction direction_;   // Направление пса
};

class GameSession {
public:
    using DogPointer = std::shared_ptr<Dog>;
    using Dogs = std::vector<DogPointer>;
    using Id = util::Tagged<std::string, GameSession>;
    GameSession(Id id, Map map)
        : id_(std::move(id)), current_map_(map) {}
    const Id& GetId() const noexcept {
        return id_;
    }
    const Map& GetMap() const noexcept {
        return current_map_;
    }
    Dogs& GetDogs() noexcept {
        return dogs_;
    }
    
    DogPointer AddDog(std::string name,const Road& road, const Speed& dog_speed_initial);
private:
    Id id_;
    using DogIdHasher = util::TaggedHasher<Dog::Id>;
    using DogIdToIndex = std::unordered_map<Dog::Id, size_t, DogIdHasher>;
    Dogs dogs_;
    DogIdToIndex dog_id_to_index_;
    Map current_map_;
};


class Player{
public:  
    Player(GameSession& session, std::shared_ptr<Dog> dog, Token token)
        : session_(session), dog_(std::move(dog)), token_(std::move(token)) {}

    GameSession& GetSession() noexcept {
        return session_;
    }
    std::shared_ptr<Dog> GetDog() noexcept {
        return dog_;
    }
    const Token& GetToken() const noexcept {
        return token_;
    }
    
private:
    GameSession& session_;
    std::shared_ptr<Dog> dog_;
    Token token_;
    
};
class Players{
public:
    using PlayerPointer = std::shared_ptr<Player>;
    using Players_ = std::vector<PlayerPointer>;
    
    PlayerPointer AddPlayer(std::string dog_name, GameSession* session);
    Players::PlayerPointer* findPlayerByToken(Token& token);

    Players_& GetPlayers() noexcept {
        return players_;
    }
    Players_ players_;
private:
    
};


class Game {
public:
    using Maps = std::vector<Map>;
    using GameSessions = std::vector<GameSession>;
    void AddMap(Map map);
    GameSession& AddGameSession(const Map& map_);
    const Maps& GetMaps() const noexcept {
        return maps_;
    }
    GameSessions& GetGameSessions() noexcept {
        return sessions_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    GameSession* FindGameSessionByMap(const Map& map) noexcept {
        for(auto& session: sessions_){
            if (session.GetMap().GetId() == map.GetId()){
                return &session;
            }
        }
        return nullptr;
    }
    
private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    using GameSessionIdHasher = util::TaggedHasher<GameSession::Id>;
    using GameSessionIdToIndex = std::unordered_map<GameSession::Id, size_t, GameSessionIdHasher>;
    std::vector<GameSession> sessions_;
    GameSessionIdToIndex session_id_to_index_;
    int time_delta;
};

}  // namespace model
