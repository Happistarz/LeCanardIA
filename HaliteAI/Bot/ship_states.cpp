#include "ship_states.hpp"
#include "blackboard.hpp"
#include "bot_parameters.hpp"
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
        Blackboard &bb = Blackboard::get_instance();

        // Si une cell adjacente est riche, y aller directement
        auto scored_local = score_directions_by_halite(ship->position, game_map);
        sort_scored_directions(scored_local);

        int current_halite = game_map.at(ship->position)->halite;
        int best_adjacent = scored_local[0].score;

        // Se deplacer vers la meilleure cell adjacente riche si elle est meilleure que la cell actuelle et pas trop pauvre
        if (best_adjacent > current_halite && best_adjacent > hlt::constants::MAX_HALITE * params::HALITE_LOW_THRESHOLD)
        {
            hlt::Direction best_dir;
            std::vector<hlt::Direction> alternatives;
            extract_best_and_alternatives(scored_local, best_dir, alternatives);

            hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
            return MoveRequest{ship->id, ship->position, desired,
                               best_dir, MoveRequest::EXPLORE_PRIORITY, alternatives};
        }

        // Sinon, aller vers le cluster cible attribue, ou le cluster global en fallback
        hlt::Position cluster_target = bb.best_cluster_position; // fallback
        if (bb.ship_explore_targets.count(ship->id))
            cluster_target = bb.ship_explore_targets[ship->id];

        int dist_to_cluster = game_map.calculate_distance(ship->position, cluster_target);

        if (dist_to_cluster > 2)
        {
            return make_move_request(ship, cluster_target,
                                     MoveRequest::EXPLORE_PRIORITY, game_map, blackboard_is_stuck);
        }

        // Si on est proche du cluster cible mais qu'on est sur une cell pauvre, essayer de se deplacer localement pour trouver une meilleure cell
        if (current_halite > hlt::constants::MAX_HALITE * params::HALITE_LOW_THRESHOLD)
            return make_still_request(ship->id, ship->position, MoveRequest::EXPLORE_PRIORITY);

        // Fallback : aller vers la meilleure cell adjacente
        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        extract_best_and_alternatives(scored_local, best_dir, alternatives);

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