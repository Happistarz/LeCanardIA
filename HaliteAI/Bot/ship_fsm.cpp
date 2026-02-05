#include "ship_fsm.hpp"
#include "ship_states.hpp"
#include "hlt/constants.hpp"

namespace bot
{
    ShipFSM::ShipFSM(hlt::EntityId ship_id)
        : m_ship_id(ship_id), m_current_state(ShipState::EXPLORE), m_current_target(hlt::Position{0, 0})
    {
    }

    ShipState ShipFSM::get_current_state() const
    {
        return m_current_state;
    }

    hlt::Command ShipFSM::update(std::shared_ptr<hlt::Ship> ship, hlt::GameMap &game_map,
                                 const hlt::Position &shipyard_position, int turns_remaining)
    {
        // Urgent return check
        int dist = game_map.calculate_distance(ship->position, shipyard_position);
        if (turns_remaining < dist + SAFE_RETURN_TURNS && m_current_state != ShipState::URGENT_RETURN)
        {
            m_current_state = ShipState::URGENT_RETURN;
        }

        hlt::Command command = ship->stay_still(); // Par défaut

        switch (m_current_state)
        {
        case ShipState::EXPLORE:
            // Si plein, on rentre
            if (ship->halite >= hlt::constants::MAX_HALITE * HALITE_FILL_THRESHOLD)
            {
                m_current_state = ShipState::RETURN;
                command = ShipReturnState::execute(ship, game_map, shipyard_position);
            }
            // Si la case a du halite, on collecte
            else if (game_map.at(ship->position)->halite > hlt::constants::MAX_HALITE * HALITE_LOW_THRESHOLD)
            {
                m_current_state = ShipState::COLLECT;
                command = ShipCollectState::execute(ship, game_map, shipyard_position);
            }
            else
            {
                command = ShipExploreState::execute(ship, game_map, shipyard_position);
            }
            break;

        case ShipState::COLLECT:
            // Si plein, on rentre
            if (ship->halite >= hlt::constants::MAX_HALITE * HALITE_FILL_THRESHOLD)
            {
                m_current_state = ShipState::RETURN;
                command = ShipReturnState::execute(ship, game_map, shipyard_position);
            }
            // Si la case est vide, on explore
            else if (game_map.at(ship->position)->halite < hlt::constants::MAX_HALITE * HALITE_LOW_THRESHOLD)
            {
                m_current_state = ShipState::EXPLORE;
                command = ShipExploreState::execute(ship, game_map, shipyard_position);
            }
            else
            {
                command = ShipCollectState::execute(ship, game_map, shipyard_position);
            }
            break;

        case ShipState::RETURN:
            // Si on est arrivé au shipyard
            if (ship->position == shipyard_position)
            {
                // On repart explorer
                m_current_state = ShipState::EXPLORE;
                command = ShipExploreState::execute(ship, game_map, shipyard_position);
            }
            else
            {
                command = ShipReturnState::execute(ship, game_map, shipyard_position);
            }
            break;

        case ShipState::URGENT_RETURN:
            // Si on est arrivé au shipyard
            if (ship->position == shipyard_position)
            {
                // TODO: Sortir du depot de halite pour laisser la place aux autres
                m_current_state = ShipState::URGENT_RETURN; // Rester en URGENT_RETURN pour l'instant
            }
            command = ShipUrgentReturnState::execute(ship, game_map, shipyard_position);
            break;

        case ShipState::FLEE:
            // Fuir le danger puis retourner à l'exploration
            m_current_state = ShipState::EXPLORE;
            command = ShipExploreState::execute(ship, game_map, shipyard_position);
            break;
        }

        return command;
    }
} // namespace bot