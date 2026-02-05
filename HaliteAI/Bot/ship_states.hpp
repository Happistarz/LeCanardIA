#pragma once

#include "ship_fsm.hpp"

namespace bot
{
    class ShipStateType
    {
    public:
        static hlt::Command execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipExploreState : public ShipStateType
    {
    public:
        static hlt::Command execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipCollectState : public ShipStateType
    {
    public:
        static hlt::Command execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipReturnState : public ShipStateType
    {
    public:
        static hlt::Command execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipFleeState : public ShipStateType
    {
    public:
        static hlt::Command execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };

    class ShipUrgentReturnState : public ShipStateType
    {
    public:
        static hlt::Command execute(std::shared_ptr<hlt::Ship> ship,
                                    hlt::GameMap &game_map, const hlt::Position &shipyard_position);
    };
} // namespace bot