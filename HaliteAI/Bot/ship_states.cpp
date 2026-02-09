#include "ship_states.hpp"
#include "blackboard.hpp"
#include "utils.hpp"
#include "hlt/game_map.hpp"
#include "hlt/direction.hpp"

#include <algorithm>
#include <vector>

namespace bot
{
    // Requete statique de position bloquee
    static bool blackboard_is_stuck(const hlt::Position &pos)
    {
        return Blackboard::get_instance().is_position_stuck(pos);
    }

    // BASE
    MoveRequest ShipStateType::execute(std::shared_ptr<hlt::Ship> ship,
                                       hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        return make_still_request(ship->id, ship->position, MoveRequest::COLLECT_PRIORITY);
    }

    // EXPLORATION
    MoveRequest ShipExploreState::execute(std::shared_ptr<hlt::Ship> ship,
                                          hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        auto scored = score_directions_by_halite(ship->position, game_map);

        // Tri par halite decroissant
        sort_scored_directions(scored);

        // Si la meilleure cell adjacente n'a pas d'avantage sur la case actuelle, rester sur place
        int current_halite = game_map.at(ship->position)->halite;
        if (scored[0].score <= current_halite)
            return make_still_request(ship->id, ship->position, MoveRequest::EXPLORE_PRIORITY);

        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        extract_best_and_alternatives(scored, best_dir, alternatives);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, MoveRequest::EXPLORE_PRIORITY, alternatives};
    }

    // COLLECTE
    MoveRequest ShipCollectState::execute(std::shared_ptr<hlt::Ship> ship,
                                          hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        // Rester sur place, alternatives triees par halite en fallback
        auto scored = score_directions_by_halite(ship->position, game_map);
        sort_scored_directions(scored);

        std::vector<hlt::Direction> alternatives;
        for (const auto &sd : scored)
            alternatives.push_back(sd.dir);

        return MoveRequest{ship->id, ship->position, ship->position,
                           hlt::Direction::STILL, MoveRequest::COLLECT_PRIORITY, alternatives};
    }

    // RETOUR
    MoveRequest ShipReturnState::execute(std::shared_ptr<hlt::Ship> ship,
                                         hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        return make_move_request(ship, shipyard_position,
                                 MoveRequest::RETURN_PRIORITY, game_map, blackboard_is_stuck);
    }

    // FUITE
    MoveRequest ShipFleeState::execute(std::shared_ptr<hlt::Ship> ship,
                                       hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        // Maximiser la distance par rapport au depot
        std::vector<ScoredDirection> scored;
        scored.reserve(4);

        for (const auto &dir : hlt::ALL_CARDINALS)
        {
            hlt::Position target = game_map.normalize(ship->position.directional_offset(dir));
            int dist = game_map.calculate_distance(target, shipyard_position);
            scored.push_back({dir, dist, false, false}); // distance plus grande = meilleur score
        }

        sort_scored_directions(scored);

        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        extract_best_and_alternatives(scored, best_dir, alternatives);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, MoveRequest::FLEE_PRIORITY, alternatives};
    }

    // RETOUR URGENT
    MoveRequest ShipUrgentReturnState::execute(std::shared_ptr<hlt::Ship> ship,
                                               hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        return make_move_request(ship, shipyard_position,
                                 MoveRequest::URGENT_RETURN_PRIORITY, game_map, blackboard_is_stuck);
    }

} // namespace bot