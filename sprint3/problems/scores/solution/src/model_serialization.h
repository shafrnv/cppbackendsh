#include <boost/serialization/vector.hpp>

#include "model.h"

namespace serialization {

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(dog.GetId())
        , name_(dog.GetName())
        , coordinate_(dog.GetCoordinate())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirectionENUM())
        , score_(dog.GetScore())
        , bag_(dog.GetBagObjects()) {
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{id_, name_, coordinate_, speed_, direction_};
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);
        for (const auto& item : bag_) {
            dog.AddBagObject(item);
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar&* id_;
        ar& name_;
        ar& coordinate_;
        ar& speed_;
        ar& direction_;
        ar& bag_;
        ar& score_;
    }

private:
    model::Dog::Id id_;
    std::string name_;
    geom::Point2D coordinate_; // Координаты пса на карте
    model::Speed speed_; // Скорость пса на карте
    model::Direction direction_;
    model::Dog::BagObjects bag_;
    int score_= 0;
};

/* Другие классы модели сериализуются и десериализуются похожим образом */

}  // namespace serialization
