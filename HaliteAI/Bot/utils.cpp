#include "utils.hpp"
#include "traffic_manager.hpp"

namespace bot
{
    MoveRequest make_still_request(hlt::EntityId ship_id, const hlt::Position &pos, int priority)
    {
        std::vector<hlt::Direction> alts(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
        return MoveRequest{ship_id, pos, pos, hlt::Direction::STILL, priority, alts};
    }

    MoveRequest make_move_request(
        std::shared_ptr<hlt::Ship> ship,
        const hlt::Position &target,
        int priority,
        hlt::GameMap &game_map,
        StuckPositionQuery is_stuck_fn)
    {
        if (ship->position == target)
            return make_still_request(ship->id, ship->position, priority);

        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        navigate_toward(ship->position, target, game_map, best_dir, alternatives, is_stuck_fn);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired, best_dir, priority, alternatives};
    }
} // namespace bot
