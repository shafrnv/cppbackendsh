#pragma once

#include <boost/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <iostream>  // for debugging
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
        , name_(std::move(name)) {
    }

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

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
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
    Dog(Id id, std::string name) noexcept
        : id_(std::move(id)), name_(std::move(name)) {}
    const Id& GetId() const noexcept {
        return id_;
    }const std::string& GetName() const noexcept {
        return name_;
    }
    
private:
    Id id_;
    std::string name_;
};

class GameSession {
public:
    using Dogs = std::vector<Dog>;
    using Id = util::Tagged<std::string, GameSession>;
    GameSession(Id id, Map map)
        : id_(std::move(id)), current_map_(map) {}
    const Id& GetId() const noexcept {
        return id_;
    }
    const Map& GetMap() const noexcept {
        return current_map_;
    }
    const Dogs& GetDogs() const noexcept {
        return dogs_;
    }
    Dog& AddDog(std::string name);
private:
    Id id_;
    using DogIdHasher = util::TaggedHasher<Dog::Id>;
    using DogIdToIndex = std::unordered_map<Dog::Id, size_t, DogIdHasher>;
    std::vector<Dog> dogs_;
    DogIdToIndex dog_id_to_index_;
    Map current_map_;
};


class Player{
public:  
    Player(GameSession& session, Dog& dog, Token token)
        : session_(session), dog_(dog), token_(std::move(token)) {}

    const GameSession& GetSession() const noexcept {
        return session_;
    }
    const Dog& GetDog() const noexcept {
        return dog_;
    }
    const Token& GetToken() const noexcept {
        return token_;
    }
private:
    GameSession& session_;
    Dog& dog_;
    Token token_;
    
};
class Players{
public:
    static Player& AddPlayer(std::string dog, GameSession* session);
    static Player* findPlayerByToken(Token& token);
private:
    static std::vector<Player> players_;
};


class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);
    GameSession& AddGameSession(const Map& map_);
    const Maps& GetMaps() const noexcept {
        return maps_;
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
};

}  // namespace model
