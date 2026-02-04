#include "blackboard.hpp"

namespace bot
{

    bool Blackboard::is_position_safe(const hlt::Position &pos) const
    {
        return true; // Placeholder implementation
    }

    bool Blackboard::is_position_reserved(const hlt::Position &pos) const
    {
        return reserved_positions.find(pos) != reserved_positions.end();
    }

    void Blackboard::reserve_position(const hlt::Position &pos, hlt::EntityId ship_id)
    {
    }

    void Blackboard::clear_turn_data()
    {
    }
} // namespace bot