#include "ship_states.hpp"
#include "hlt/game_map.hpp"
#include "hlt/direction.hpp"

namespace bot
{
    // Base implementation
    hlt::Command ShipStateType::execute(std::shared_ptr<hlt::Ship> ship,
                                        hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        return hlt::command::move(ship->id, hlt::Direction::STILL);
    }

    // EXPLORE
    hlt::Command ShipExploreState::execute(std::shared_ptr<hlt::Ship> ship,
                                           hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {

        int max_halite = -1;
        hlt::Position best_target = ship->position; // Par défaut

        // Chercher la case adjacente avec le plus de halite
        for (const auto &direction : hlt::ALL_CARDINALS)
        {
            hlt::Position target_pos = ship->position.directional_offset(direction);

            if (game_map.at(target_pos)->halite > max_halite)
            {
                max_halite = game_map.at(target_pos)->halite;
                best_target = target_pos;
            }
        }

        if (best_target == ship->position)
        {
            // TODO: Améliorer la recherche de target si pas de halite autour
            return hlt::command::move(ship->id, hlt::Direction::STILL);
        }

        return hlt::command::move(ship->id, game_map.naive_navigate(ship, best_target));
    }

    // COLLECT
    hlt::Command ShipCollectState::execute(std::shared_ptr<hlt::Ship> ship,
                                           hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        // Rester sur place pour collecter
        return hlt::command::move(ship->id, hlt::Direction::STILL);
    }

    // RETURN
    hlt::Command ShipReturnState::execute(std::shared_ptr<hlt::Ship> ship,
                                          hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        return hlt::command::move(ship->id, game_map.naive_navigate(ship, shipyard_position));
    }

    // FLEE
    hlt::Command ShipFleeState::execute(std::shared_ptr<hlt::Ship> ship,
                                        hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        // TODO: Implémenter une logique de fuite
        return hlt::command::move(ship->id, game_map.naive_navigate(ship, shipyard_position));
    }

    // URGENT RETURN
    hlt::Command ShipUrgentReturnState::execute(std::shared_ptr<hlt::Ship> ship,
                                                hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        // Retourner immédiatement au shipyard
        return hlt::command::move(ship->id, game_map.naive_navigate(ship, shipyard_position));
    }
}
