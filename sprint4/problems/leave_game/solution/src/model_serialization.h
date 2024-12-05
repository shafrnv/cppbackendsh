#pragma once
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "model.h"

namespace serialization {

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class Point2DRepr {
public:
    Point2DRepr() = default;

    explicit Point2DRepr(double x, double y)
       : x_(x)
       , y_(y) {}

    [[nodiscard]] geom::Point2D Restore() const {
        geom::Point2D coord{x_, y_};
        return coord;
    }
    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version){
        ar & x_;
        ar & y_;
    }
private:
    double x_;
    double y_;
};
// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class SpeedRepr {
public:
    SpeedRepr() = default;

    explicit SpeedRepr(double x, double y)
       : vx(x)
       , vy(y) {}

    [[nodiscard]] model::Speed Restore() const {
        model::Speed speed{vx, vy};
        return speed;
    }
    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version){
        ar & vx;
        ar & vy;
    }
private:
    double vx;
    double vy;
};

// LostObjectRepr (LostObjectRepresentation) - сериализованное представление класса LostObject
class LostObjectRepr {
public:
    LostObjectRepr() = default;

    explicit LostObjectRepr(const model::LostObject& object)
        : id_(*(object.GetId()))
        , type_(object.GetType())
        , coord_(object.GetCoordinate().x, object.GetCoordinate().y)
        , value_(object.GetValue()) {}

    [[nodiscard]] model::LostObject Restore() const {
        model::LostObject::Id id{id_};
        model::LostObject lost_object{id, type_, coord_.Restore(), value_};
        return lost_object;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& type_;
        ar& coord_;
        ar& value_;
    }

private:
    int id_;
    uint type_;
    Point2DRepr coord_;
    int value_;
};

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(*(dog.GetId()))
        , name_(dog.GetName())
        , coordinate_(dog.GetCoordinate().x, dog.GetCoordinate().y)
        , speed_(dog.GetSpeed().vx, dog.GetSpeed().vy)
        , direction_(dog.GetDirectionENUM())
        , score_(dog.GetScore())
        , retirement_time_ms_(dog.GetRetirementTime())
        , time_in_game_ms_(dog.GetTimeInGame())
         {
            for(const auto& item : dog.GetBagObjects()) {
                auto lost_object_ = std::make_shared<LostObjectRepr>(*item.get());
                bag_.emplace_back(std::move(lost_object_));
            }
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog::Id id{id_};
        model::Dog dog{id, name_, coordinate_.Restore(), speed_.Restore(), direction_};
        for (const auto& item : bag_) {
            auto restord_obj = std::make_shared<model::LostObject>(item->Restore());
            dog.AddBagObject(restord_obj);
        }
        dog.AddInGameTime(time_in_game_ms_);
        dog.AddRetirementTime(retirement_time_ms_);
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& coordinate_;
        ar& speed_;
        ar& direction_;
        ar& bag_;
        ar& score_;
    }

private:
    int id_;
    std::string name_;
    Point2DRepr coordinate_; 
    SpeedRepr speed_; 
    model::Direction direction_;
    std::vector<std::shared_ptr<LostObjectRepr>> bag_;
    double retirement_time_ms_ = 0;
    double time_in_game_ms_ = 0;
    int score_= 0;
};

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class TempMapRepr {
public:
    TempMapRepr() = default;

    explicit TempMapRepr(std::string id, std::string name)
       : id_(id)
       , name_(name) {}

    [[nodiscard]] model::Map Restore() const {
        model::Map::Id id{id_};
        model::Map map{id, name_};
        return map;
    }
    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version){
        ar & id_;
        ar & name_;
    }
private:
    std::string id_;
    std::string name_;
};

// GameSessionRepr (GameSessionRepresentation) - сериализованное представление класса GameSession
class GameSessionRepr {
public:
    GameSessionRepr() = default;

    explicit GameSessionRepr(model::GameSession& session)
        : id_(*session.GetId())
        , map_for_serialize_(*session.GetMap().GetId(), session.GetMap().GetName()) {
            for (auto& dog : session.GetDogs()) {
                auto dog_ = std::make_shared<DogRepr>(*dog.get());
                dogs_.emplace_back(std::move(dog_));
            }
            for (const auto& item : session.GetLostObjects()) {
                auto lost_object_ = std::make_shared<LostObjectRepr>(*item.get());
                lost_objects_.emplace_back(std::move(lost_object_));
            }
         }

    [[nodiscard]] model::GameSession Restore() const {
        model::GameSession::Id id{id_};

        model::GameSession game_session{id, map_for_serialize_.Restore()};
        for (auto& dog : dogs_) {
            auto restored_dog = std::make_shared<model::Dog>(dog->Restore());
            game_session.AddYetDog(restored_dog);
        }   
        for (auto& item : lost_objects_) { 
            auto restored_lost_object = std::make_shared<model::LostObject>(item->Restore());
            game_session.AddYetObject(restored_lost_object);
        }
        return game_session;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& dogs_;
        ar& map_for_serialize_;
        ar& lost_objects_;
    }

private:
    std::string id_;
    std::vector<std::shared_ptr<DogRepr>> dogs_;
    TempMapRepr map_for_serialize_;
    double session_time_ = 0;
    std::vector<std::shared_ptr<LostObjectRepr>> lost_objects_;
};

class PlayerRepr {
public:
    PlayerRepr() = default;

    explicit PlayerRepr(const model::Player& player)
        : token_(*player.GetToken()) {
            session_ = std::make_shared<GameSessionRepr>(*player.GetSession().get());
            dog_ = std::make_shared<DogRepr>(*player.GetDog().get());
         }

    [[nodiscard]] model::Player Restore() const {
        Token token{token_};
        
        model::Player player{std::make_shared<model::GameSession>(session_->Restore())
                            , std::make_shared<model::Dog>(dog_->Restore()),token};

        return player;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& token_;
        ar& session_;
        ar& dog_;
    }

private:
    std::string token_;
    std::shared_ptr<GameSessionRepr> session_;
    std::shared_ptr<DogRepr> dog_;
};

struct SerializationObj {
    SerializationObj() = default;

    explicit SerializationObj(model::Game::GameSessions game_sessions, 
                            model::Players::Players_ players)  {
        for (const auto& session : game_sessions) {
                auto game_session_ = std::make_shared<GameSessionRepr>(*session.get());
                game_sessions_.emplace_back(std::move(game_session_));
        }
        for (const auto& player : players) {
                auto player_ = std::make_shared<PlayerRepr>(*player.get());
                players_.emplace_back(std::move(player_));
        }                            
    }

    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & game_sessions_;
        ar & players_;
    }
    
    std::vector<std::shared_ptr<GameSessionRepr>> game_sessions_; 
    std::vector<std::shared_ptr<PlayerRepr>> players_;
};	


}  // namespace serialization
