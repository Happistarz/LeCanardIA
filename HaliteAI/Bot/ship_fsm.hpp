#pragma once

#include "hlt/command.hpp"
#include "hlt/entity.hpp"
#include "hlt/game_map.hpp"
#include "hlt/position.hpp"
#include "hlt/ship.hpp"

namespace bot
{

    enum class ShipState
    {
        EXPLORE,      // Chercher des ressources
        COLLECT,      // Ramasser des ressources
        RETURN,       // Retourner au dépôt
        FLEE,         // Éviter un danger
        URGENT_RETURN // Retourner immédiatement au dépôt
    };

    class ShipFSM
    {
      private:
        hlt::EntityId m_ship_id;
        ShipState m_current_state;
        hlt::Position m_current_target;


  public:
    explicit ShipFSM(hlt::EntityId ship_id);

        hlt::Command update(std::shared_ptr<hlt::Ship> ship,
                        hlt::GameMap &game_map, const hlt::Position &shipyard_position,
                            int turns_remaining);

    ShipState get_current_state() const;
    hlt::EntityId get_ship_id() const { return m_ship_id; }
  };
} // namespace bot