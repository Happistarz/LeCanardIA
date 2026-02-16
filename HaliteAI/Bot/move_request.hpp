#pragma once

#include "bot_constants.hpp"
#include "hlt/entity.hpp"
#include "hlt/position.hpp"
#include "hlt/direction.hpp"

#include <vector>

namespace bot
{
    struct MoveRequest
    {
        hlt::EntityId m_ship_id;                    // Id du ship
        hlt::Position m_current;                    // Position actuelle
        hlt::Position m_desired;                    // Position souhaitee
        hlt::Direction m_desired_direction;         // Direction souhaitee
        int m_priority;                             // Priorite de traitement
        std::vector<hlt::Direction> m_alternatives; // Directions secondaires
    };

    struct MoveResult
    {
        hlt::EntityId m_ship_id;          // Id du ship
        hlt::Direction m_final_direction; // Direction finale du tour
    };

} // namespace bot
