#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

void GameSession::AddLostObject() {
    size_t index = GetObjSize()++;
    model::LostObject::Id lost_object_id{ static_cast<int>(index)};
    size_t loot_types_count = current_map_.GetExtraData().loot_types_.size();
    // здесь норм
    geom::Point2D random_coordinate;
    std::random_device rd;
    std::mt19937 gen(rd());
    auto roads_count =current_map_.GetRoads().size();
    std::uniform_int_distribution<> dis(0, static_cast<int>(roads_count - 1));
    std::uniform_int_distribution<> dis_type(0, static_cast<int>(loot_types_count - 1));
    const Road& road = current_map_.GetRoads()[dis(gen)];
    if (road.IsHorizontal()) {
        std::uniform_real_distribution<> dis_x(road.GetStart().x, road.GetEnd().x);
        random_coordinate.x = dis_x(gen);
        random_coordinate.y = road.GetStart().y; 
    } else if (road.IsVertical()) {
        std::uniform_real_distribution<> dis_y(road.GetStart().y, road.GetEnd().y);
        random_coordinate.y = dis_y(gen);
        random_coordinate.x = road.GetStart().x; 
    }
    uint item_type_ = dis_type(gen);
    auto lost_object_ = std::make_shared<LostObject>(lost_object_id,item_type_,random_coordinate,current_map_.GetExtraData().loot_types_[item_type_].as_object().at("value").as_int64());
    lost_objects_.emplace_back(std::move(lost_object_)); 
}

GameSession::DogPointer GameSession::AddDog(std::string name, const Road& road, const Speed& dog_speed_initial) {
    size_t index = dogs_.size();
    model::Dog::Id dog_id{ static_cast<int>(index)};
    geom::Point2D random_coordinate ={0,0};

    std::random_device rd;
    std::mt19937 gen(rd());

    if (road.IsHorizontal()) {
        std::uniform_real_distribution<> dis_x(road.GetStart().x, road.GetEnd().x);
        random_coordinate.x = dis_x(gen);
        random_coordinate.y = road.GetStart().y; 
    } else if (road.IsVertical()) {
        std::uniform_real_distribution<> dis_y(road.GetStart().y, road.GetEnd().y);
        random_coordinate.y = dis_y(gen);
        random_coordinate.x = road.GetStart().x;
    }

    Direction initial_direction = Direction::NORTH;
    auto dog = std::make_shared<Dog>(dog_id, name,random_coordinate, dog_speed_initial, initial_direction);
    if (auto [it, inserted] = dog_id_to_index_.emplace(dog.get()->GetId(), index); !inserted) {
        throw std::invalid_argument("Dog with id "s + std::to_string(*dog.get()->GetId()) + " already exists"s);
     } else {
        try {
            dogs_.emplace_back(dog);
            return dog;   
        } catch (...) {
            dog_id_to_index_.erase(it);
            throw;
        }
    };
}

std::shared_ptr<GameSession> Game::AddGameSession(const Map& map_) {
    size_t index = sessions_.size();
    model::GameSession::Id session_id{std::to_string(index)};
    auto session = std::make_shared<GameSession>(session_id, map_);
    sessions_.emplace_back(std::move(session));
    return sessions_.back();
}

Players::PlayerPointer Players::AddPlayer(std::string dog_name, std::shared_ptr<GameSession> session, bool random_points) {
    PlayerTokens pltk_;
    Token token = pltk_.generateToken();

    if (random_points) {
        std::random_device rd;
        std::mt19937 gen(rd());
        auto roads_count = session.get()->GetMap().GetRoads().size();
        std::uniform_int_distribution<> dis(0, static_cast<int>(roads_count - 1));
        const  Road& road = session.get()->GetMap().GetRoads()[dis(gen)];
        auto dog = session.get()->AddDog(dog_name, road, model::Speed(0,0));
        auto player_ =std::make_shared<Player>(session, dog, token);
        players_.emplace_back(std::move(player_));
    } else {
        const  Road& road = session.get()->GetMap().GetRoads()[0];
        auto dog = session.get()->AddDog(dog_name, road, model::Speed(0,0));
        auto player_ =std::make_shared<Player>(session, dog, token);
        players_.emplace_back(std::move(player_));
    }
    return players_.back();
} 

Players::PlayerPointer Players::findPlayerByToken(Token& token) {
    for (PlayerPointer& player : players_) {
         if (player.get()->GetToken() == token) {
             return player;
         }
    }   
    return nullptr;
}
}  // namespace model
