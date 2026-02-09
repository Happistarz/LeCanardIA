#pragma once

#include "traffic_manager.hpp"
#include "hlt/game_map.hpp"
#include "hlt/ship.hpp"

#include <memory>

namespace bot
{
    // BASE : COLLECT
    class ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                   hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    // Se deplacer vers la cell la plus rentable
    class ShipExploreState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                   hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    // Rester sur la cell pour collecter le halite
    class ShipCollectState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                   hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    // Naviguer vers le depot le plus proche
    class ShipReturnState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                   hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    // Fuite : maximiser la distance par rapport aux menaces
    class ShipFleeState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                   hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    // Retour haute priorite ignorant l'optimisation d'halite
    class ShipUrgentReturnState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                   hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

} // namespace bot