#include "ship_fsm.hpp"

namespace bot
{
    ShipFSM::ShipFSM(hlt::EntityId ship_id)
        : m_ship_id(ship_id)
        , m_current_state(ShipState::EXPLORE)
        , m_current_target(hlt::Position{0, 0})
    {
    }

    ShipState ShipFSM::get_current_state() const
    {
        return m_current_state;
    }

    hlt::Command ShipFSM::update(std::shared_ptr<hlt::Ship> ship, hlt::GameMap& game_map,
                                 const hlt::Position& shipyard_position, int turns_remaining)
    {
    }
} // namespace bot