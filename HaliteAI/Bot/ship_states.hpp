#pragma once

#include "ship_fsm.hpp"

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

        /// Navigation helper accessible depuis l'exterieur (ex: dropoff routing)
        static void navigate_toward_static(std::shared_ptr<hlt::Ship> ship,
                                           hlt::GameMap &game_map,
                                           const hlt::Position &destination,
                                           hlt::Direction &out_best_dir,
                                           std::vector<hlt::Direction> &out_alternatives);
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