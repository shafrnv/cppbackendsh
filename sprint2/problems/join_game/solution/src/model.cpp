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
Dog GameSession::AddDog(std::string name) {
    size_t index = dogs_.size();
    model::Dog::Id dog_id{std::to_string(index)};
    std::cout << "DOGID" << *dog_id << std::endl;
    Dog dog(dog_id, name);
    if (auto [it, inserted] = dog_id_to_index_.emplace(dog.GetId(), index); !inserted) {
        std::cout << "Oh no"<< std::endl;
        throw std::invalid_argument("Dog with id "s + *dog.GetId() + " already exists"s);
     } else {
        try {
            std::cout << "Oh yaaaaaaaay" << std::endl;
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
std::vector<Player> Players::players_;
Player& Players::AddPlayer(std::string dog, GameSession* session) {
    PlayerTokens pltk_;
    Token token = pltk_.generateToken();
    Player player_(*session, session->AddDog(dog), token);
    players_.emplace_back(std::move(player_));
    Player& addedPlayer = players_.back();
    return addedPlayer;
} 
Player* Players::findPlayerByToken(Token& token) {
    for (auto& player : players_) {
         if (player.GetToken() == token) {
             return &player;
         }
    }
    return nullptr;
}
}  // namespace model
