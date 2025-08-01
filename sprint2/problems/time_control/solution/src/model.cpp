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
GameSession::DogPointer GameSession::AddDog(std::string name, const Road& road, const Speed& dog_speed_initial) {
    size_t index = dogs_.size();
    model::Dog::Id dog_id{ static_cast<int>(index)};
    Coordinate random_coordinate ={0,0};

    std::random_device rd;
    std::mt19937 gen(rd());
    /*
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
    }*/

    // Направление пса по умолчанию — север
    Direction initial_direction = Direction::NORTH;
    auto dog = std::make_shared<Dog>(dog_id, name,random_coordinate, dog_speed_initial, initial_direction);
    // std::cout<<"initial coord"<<road.GetStart().x <<" "<<road.GetStart().y<<std::endl<<road.GetEnd().x<<" "<<road.GetEnd().y<<std::endl;
    // std::cout<<"Random coord"<<random_coordinate.x<<" "<<random_coordinate.y<<std::endl;
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
GameSession& Game::AddGameSession(const Map& map_) {
    size_t index = sessions_.size();
    model::GameSession::Id session_id{std::to_string(index)};
    GameSession session(session_id, map_);
    sessions_.emplace_back(std::move(session));
    return sessions_.back();
}

Players::PlayerPointer Players::AddPlayer(std::string dog_name, GameSession* session) {
    PlayerTokens pltk_;
    Token token = pltk_.generateToken();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    auto roads_count = session->GetMap().GetRoads().size();
    std::uniform_int_distribution<> dis(0, static_cast<int>(roads_count - 1));
    const  Road& road = session->GetMap().GetRoads()[0/*dis(gen)*/];
    auto dog = session->AddDog(dog_name, road, model::Speed(0,0));
    std::cout<<"here 1"<< std::endl;
    auto player_ =std::make_shared<Player>(*session, dog, token);
    std::cout<<"here 2"<< std::endl;
    players_.emplace_back(std::move(player_));
    std::cout<<"here 3"<< std::endl;
    return player_;
    std::cout<<"here 4"<< std::endl;
} 

Players::PlayerPointer* Players::findPlayerByToken(Token& token) {
    for (auto& player : players_) {
         if (player->GetToken() == token) {
             return &player;
         }
    }
    return nullptr;
}
}  // namespace model
