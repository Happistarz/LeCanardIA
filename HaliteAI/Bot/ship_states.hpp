#pragma once

#include "move_request.hpp"
#include "hlt/game_map.hpp"
#include "hlt/ship.hpp"

#include <memory>

namespace bot
{
    class ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipExploreState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipCollectState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipReturnState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipFleeState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipUrgentReturnState : public ShipStateType
    {
    public:
        static MoveRequest execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };
} // namespace bot