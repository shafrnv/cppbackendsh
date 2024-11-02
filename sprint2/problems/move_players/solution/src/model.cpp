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
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
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
Dog& GameSession::AddDog(std::string name, const Road& road, const Speed& dog_speed_initial) {
    size_t index = dogs_.size();
    model::Dog::Id dog_id{index};
    Coordinate random_coordinate;

    std::random_device rd;
    std::mt19937 gen(rd());
    
    if (road.IsHorizontal()) {
        // Если дорога горизонтальная, выбираем случайную координату по оси X между start и end
        std::uniform_real_distribution<> dis_x(road.GetStart().x, road.GetEnd().x);
        random_coordinate.x = dis_x(gen);
        random_coordinate.y = road.GetStart().y;  // y остаётся неизменным
    } else if (road.IsVertical()) {
        // Если дорога вертикальная, выбираем случайную координату по оси Y между start и end
        std::uniform_real_distribution<> dis_y(road.GetStart().y, road.GetEnd().y);
        random_coordinate.y = dis_y(gen);
        random_coordinate.x = road.GetStart().x;  // x остаётся неизменным
    }

    // Направление пса по умолчанию — север
    Direction initial_direction = Direction::NORTH;
    Dog dog(dog_id, name,random_coordinate, dog_speed_initial, initial_direction);
    // std::cout<<"initial coord"<<road.GetStart().x <<" "<<road.GetStart().y<<std::endl<<road.GetEnd().x<<" "<<road.GetEnd().y<<std::endl;
    // std::cout<<"Random coord"<<random_coordinate.x<<" "<<random_coordinate.y<<std::endl;
    if (auto [it, inserted] = dog_id_to_index_.emplace(dog.GetId(), index); !inserted) {
        throw std::invalid_argument("Dog with id "s + std::to_string(*dog.GetId()) + " already exists"s);
     } else {
        try {
            dogs_.emplace_back(dog);
            return dogs_.back();   
        } catch (...) {
            dog_id_to_index_.erase(it);
            throw;
        }
    };
}
GameSession& Game::AddGameSession(const Map& map_) {
    size_t index = sessions_.size();
    model::GameSession::Id session_id{std::to_string(index)};
    GameSession session(session_id, map_);
    sessions_.emplace_back(std::move(session));
    return sessions_.back();
}
std::vector<Player> Players::players_;
Player& Players::AddPlayer(std::string dog, GameSession* session) {
    PlayerTokens pltk_;
    Token token = pltk_.generateToken();
    std::random_device rd;
    std::mt19937 gen(rd());
    auto roads_count = session->GetMap().GetRoads().size();
    std::uniform_int_distribution<> dis(0, roads_count - 1);
    const  Road& road = session->GetMap().GetRoads()[dis(gen)];
    Player player_(*session, session->AddDog(dog, road,session->GetMap().GetSpeed()), token);
    std::cout <<"add pl" <<std::endl;
    players_.emplace_back(std::move(player_));
    Player& addedPlayer = players_.back();
    return addedPlayer;
} 
Player* Players::findPlayerByToken(Token& token) {
    for (auto& player : players_) {
            std::cout<<*(player.GetToken())<<*(player.GetDog().GetId())<<std::endl;
         if (player.GetToken() == token) {
            std::cout<<"find pl"<<std::endl;
             return &player;
         }
    }
    return nullptr;
}
}  // namespace model
