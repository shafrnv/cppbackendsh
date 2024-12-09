#pragma once

#include <boost/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <iostream>  // for debugging
#include <algorithm>
#include "tagged.h"
#include "loot_generator.h"
#include "collision_detector.h"
#include <chrono>

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
class LostObject{
public:
    using Id = util::Tagged<int, LostObject>;
    LostObject(Id id, uint type, geom::Point2D coord, int value) noexcept
        : id_{std::move(id)}
        , type_(std::move(type))
        , coord_(std::move(coord))
        , value_(std::move(value)) {}


    const Id& GetId() const noexcept {
        return id_;
    }
    const uint& GetType() const noexcept {
        return type_;
    }
    const int& GetValue() const noexcept {
        return value_;
    }
    const geom::Point2D& GetCoordinate() const noexcept {
        return coord_;
    }
    bool operator==(const LostObject&) const = default;
private:
    Id id_;
    uint type_;
    geom::Point2D coord_;
    int value_;
};

class VectorItemGathererProvider : public collision_detector::ItemGathererProvider {
public:
    VectorItemGathererProvider(std::vector<collision_detector::Item> items,
                               std::vector<collision_detector::Gatherer> gatherers)
        : items_(items)
        , gatherers_(gatherers) {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }
    collision_detector::Item GetItem(size_t idx) const override {
        return items_[idx];
    }
    size_t GatherersCount() const override {
        return gatherers_.size();
    }
    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_[idx];
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

class ExtraData{
public:
    boost::json::array loot_types_;
private:
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

    const ExtraData& GetExtraData() const noexcept {
        return extra_data_;
    }

    const Speed& GetSpeed() const noexcept {
        return default_map_dog_speed;
    }
    const int& GetBagCapacity() const noexcept {
        return default_bag_capacity;
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
    void SetBagCapacity(int capacity) noexcept {
        default_bag_capacity = capacity;
    }
    void SetExtraData(const ExtraData& extra_data) noexcept {
        extra_data_ = extra_data;
    }

    const Office* FindOffice(const Office::Id& id) const noexcept {
        if (auto it = warehouse_id_to_index_.find(id); it != warehouse_id_to_index_.end()) {
            return &offices_.at(it->second);
        }
        return nullptr;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    
    ExtraData extra_data_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    Speed default_map_dog_speed;
    int default_bag_capacity;
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
    using BagPointer = std::shared_ptr<LostObject>;
    using BagObjects= std::vector<BagPointer>;

    explicit Dog(Id id, std::string name, geom::Point2D coordinate, Speed speed, Direction direction) noexcept
        : id_(std::move(id)),
          name_(std::move(name)),
          coordinate_(std::move(coordinate)),
          speed_(std::move(speed)),
          direction_(direction) {}
    const Id& GetId() const noexcept {
        return id_;
    }
    const std::string& GetName() const noexcept {
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

    void AddBagObject(const BagPointer& lost_object) noexcept {
        score_ += lost_object.get()->GetValue();
        bag_.emplace_back(std::move(lost_object));
    }

    void DeleteBagObjects() noexcept {
        bag_.clear();
    }

    void AddRetirementTime(const double& retirement_time) noexcept {
        retirement_time_ms += retirement_time;
    }
    
    void CancelRetirementTime() noexcept {
        retirement_time_ms = 0;
    }

    void AddInGameTime(const double& ses_time) noexcept {
        session_in_time +=ses_time;
    }

    const BagObjects& GetBagObjects() const noexcept {
        return bag_;
    }
    const size_t GetBagSize() const noexcept {
        return bag_.size();
    }
    const geom::Point2D& GetCoordinate() const noexcept {
        return coordinate_;
    }

    const Speed& GetSpeed() const noexcept {
        return speed_;
    }

    const int& GetScore() const noexcept {
        return score_;
    }

    const double& GetRetirementTime() const noexcept {
        return retirement_time_ms;
    }

    const double& GetTimeInGame() const noexcept {
        return session_in_time;
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

    const Road* GetCurrentRoad(const std::vector<Road>& roads, const geom::Point2D& coordinate) const {
        for (const auto& road : roads) {
            if (road.IsHorizontal()){
                double max_coord_cur_x = std::max(road.GetStart().x,road.GetEnd().x); 
                double min_coord_cur_x = std::min(road.GetStart().x,road.GetEnd().x); 
                if ((coordinate.y <= road.GetStart().y + 0.4 && coordinate.y >= road.GetStart().y - 0.4) &&
                    coordinate.x >= min_coord_cur_x - 0.4 && coordinate.x <= max_coord_cur_x + 0.4) {
                    return &road;
                }
            }
            if (road.IsVertical()){
                double max_coord_cur_y = std::max(road.GetStart().y,road.GetEnd().y); 
                double min_coord_cur_y = std::min(road.GetStart().y,road.GetEnd().y); 
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
    geom::Point2D coordinate_; // Координаты пса на карте
    Speed speed_;           // Скорость пса на карте
    Direction direction_;   // Направление пса
    BagObjects bag_;
    double retirement_time_ms = 0;
    double session_in_time = 0;
    int score_ = 0;
};

class GameSession {
public:
    using DogPointer = std::shared_ptr<Dog>;
    using Dogs = std::vector<DogPointer>;
    using Id = util::Tagged<std::string, GameSession>;
    using LostObjectPointer = std::shared_ptr<LostObject>;
    using LostObjects= std::vector<LostObjectPointer>;
    
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
    size_t& GetObjSize() noexcept {
        return spawned_objects_size_;
    }

    DogPointer AddDog(std::string name, const Road& road, const Speed& dog_speed_initial);

    const LostObjects& GetLostObjects() const noexcept {
        return lost_objects_;
    }

    void AddLostObject();

    void AddYetDog(DogPointer& dog_){
        size_t index = dogs_.size();
        if (auto [it, inserted] = dog_id_to_index_.emplace(dog_.get()->GetId(), index); !inserted) {
        } else {
            try {
                dogs_.emplace_back(dog_);  
            } catch (...) {
                dog_id_to_index_.erase(it);
                throw;
            }
        };
    }
    void AddYetObject(LostObjectPointer& object_){
        lost_objects_.push_back(object_);
    }

    //TODO: Сделать удаление ещё из LostObjectIdToIndex и изменить логику
    void DeleteLostObjects(const size_t& index) noexcept{
        lost_objects_.erase(lost_objects_.begin() + index);
    }

    void DeleteDog(const DogPointer& dog_) noexcept{
        dogs_.erase(std::find_if(dogs_.begin(), dogs_.end(),
        [&](const DogPointer& dog) {return dog == dog_; }));
        dog_id_to_index_.erase(dog_->GetId());
    }

    void SetMapIdForSerialize(const Map& map ) {
        current_map_ = map;
    }

    LostObjectPointer FindLostObject(const LostObject::Id& id) noexcept {
        if (auto it = lost_object_id_to_index_.find(id); it != lost_object_id_to_index_.end()) {
            return lost_objects_.at(it->second);
        }
        return nullptr;
    }
    //TODO: Использовать после, того как сделаю сериализацию этого из LostObjectIdToIndex и изменить логику
    DogPointer FindDog(const Dog::Id& id) noexcept {
        if (auto it = dog_id_to_index_.find(id); it != dog_id_to_index_.end()) {
            return dogs_.at(it->second);
        }
        return nullptr;
    }

private:
    Id id_;
    

    using DogIdHasher = util::TaggedHasher<Dog::Id>;
    using DogIdToIndex = std::unordered_map<Dog::Id, size_t, DogIdHasher>;
    Dogs dogs_;
    DogIdToIndex dog_id_to_index_;

    Map current_map_;
    std::string map_id_for_serialize_;

    using LostObjectsIdHasher = util::TaggedHasher<LostObject::Id>;
    using LostObjectIdToIndex = std::unordered_map<LostObject::Id, size_t, LostObjectsIdHasher>;

    LostObjects lost_objects_;
    LostObjectIdToIndex lost_object_id_to_index_;
    size_t spawned_objects_size_ = 0;
};


class Player{
public:  
    Player(std::shared_ptr<GameSession> session, std::shared_ptr<Dog> dog, Token token)
        : session_(session), dog_(std::move(dog)), token_(std::move(token)) {}

    const std::shared_ptr<GameSession> GetSession() const noexcept {
        return session_;
    }
    const std::shared_ptr<Dog> GetDog() const noexcept {
        return dog_;
    }
    const Token& GetToken() const noexcept {
        return token_;
    }
    void SetSessionForSerialize(std::shared_ptr<GameSession> session) {
        session_ = session;
    }
    void SetDogForSerialize(std::shared_ptr<Dog> dog) {
        dog_ = dog;
    }
private:
    std::shared_ptr<GameSession> session_;
    std::shared_ptr<Dog> dog_;
    Token token_;

};
class Players{
public:
    using PlayerPointer = std::shared_ptr<Player>;
    using Players_ = std::vector<PlayerPointer>;
    
    Players::PlayerPointer AddPlayer(std::string dog_name, std::shared_ptr<GameSession>, bool random_points);
    Players::PlayerPointer findPlayerByToken(Token& token);
    Players::PlayerPointer FindPlayerByDog(Dog::Id);

    void AddYetPlayer(const Player& player){
        players_.push_back(std::make_shared<Player>(player));
    }

    void DeletePlayer(const PlayerPointer& player_, GameSession::Dogs& other_dogs_in_ses) noexcept{
        auto it = std::find(players_.begin(), players_.end(),player_);
            //[&](const PlayerPointer& player) { return player == player_; });
        if (it!= players_.end()) {
            auto dogIt = std::find(other_dogs_in_ses.begin(), other_dogs_in_ses.end(),player_->GetDog());
           // [&](const GameSession::DogPointer& dog) {player_->GetDog()->GetId() == dog->GetId(); });

            if (dogIt != other_dogs_in_ses.end()){
                std::cout << "Dog Id" << (*dogIt)->GetName() << std::endl;
                other_dogs_in_ses.erase(dogIt);
            }   
            std::cout << "Token player" << *(*it)->GetToken() << std::endl;
            players_.erase(it); 
        }
    }

    Players_& GetPlayers() noexcept {
        return players_;
    }

    Players_ players_;
private:
    
};


class Game {
public:
    using GameSessionPointer = std::shared_ptr<GameSession>;
    using GameSessions = std::vector<GameSessionPointer>;
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    std::shared_ptr<GameSession> AddGameSession(const Map& map_);

    void AddYetGameSession(const GameSession& session){
        sessions_.push_back(std::make_shared<GameSession>(session));
    }

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

    GameSessionPointer* FindGameSessionByMap(const Map& map) noexcept {
        for(auto& session: sessions_){
            if (session.get()->GetMap().GetId() == map.GetId()){
                return &session;
            }
        }
        return nullptr;
    }

    const std::chrono::milliseconds& GetLootPeriod() const noexcept {
        return loot_generator_period;
    }

    const double& GetLootProbability() const noexcept {
        return loot_generator_probability;
    }

    const double& GetDogRetirementTime() const noexcept {
        return dog_retirement_time_s;
    }

    void SetLootPeriod(std::string period) noexcept {
        loot_generator_period =  std::chrono::milliseconds(std::stoll(period));;
    }

    void SetLootProbability(double probability) noexcept {
        loot_generator_probability = probability;
    }

    void SetRetirementTime(int period) noexcept {
        dog_retirement_time_s = period;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    using GameSessionIdHasher = util::TaggedHasher<GameSession::Id>;
    using GameSessionIdToIndex = std::unordered_map<GameSession::Id, size_t, GameSessionIdHasher>;
    GameSessions sessions_;
    GameSessionIdToIndex session_id_to_index_;
    int time_delta;

    double dog_retirement_time_s = 60;
    std::chrono::milliseconds loot_generator_period;
    double loot_generator_probability;
};

}  // namespace model
