#include "ship_states.hpp"
#include "blackboard.hpp"
#include "map_utils.hpp"
#include "bot_constants.hpp"
#include "hlt/game_map.hpp"
#include "hlt/direction.hpp"

#include <algorithm>
#include <vector>
#include <utility>

namespace bot
{
    // BASE
    MoveRequest ShipStateType::execute(std::shared_ptr<hlt::Ship> ship,
                                       hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        std::vector<hlt::Direction> alternatives(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
        return MoveRequest{ship->id, ship->position, ship->position,
                           hlt::Direction::STILL, constants::COLLECT_PRIORITY, alternatives};
    }

    // Helper local : appelle map_utils::navigate_toward avec les donnees du blackboard
    static void navigate_with_blackboard(std::shared_ptr<hlt::Ship> ship,
                                         hlt::GameMap &game_map,
                                         const hlt::Position &destination,
                                         hlt::Direction &out_best_dir,
                                         std::vector<hlt::Direction> &out_alternatives)
    {
        const Blackboard &bb = Blackboard::get_instance();
        map_utils::navigate_toward(ship, game_map, destination,
                                   bb.stuck_positions, bb.danger_zones,
                                   out_best_dir, out_alternatives);
    }

    // EXPLORE
    MoveRequest ShipExploreState::execute(std::shared_ptr<hlt::Ship> ship,
                                          hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        Blackboard &bb = Blackboard::get_instance();

        // 1. Verifier si on a deja un target persistant
        auto pt_it = bb.persistent_targets.find(ship->id);
        if (pt_it != bb.persistent_targets.end())
        {
            hlt::Position target = pt_it->second;
            int dist = game_map.calculate_distance(ship->position, target);

            // Abandonner si arrive ou zone epuisee
            if (dist == 0 || game_map.at(target)->halite < constants::PERSISTENT_TARGET_MIN_HALITE)
            {
                bb.persistent_targets.erase(pt_it);
            }
            else
            {
                // Continuer vers le meme target
                bb.targeted_cells[target] = ship->id;

                hlt::Direction best_dir;
                std::vector<hlt::Direction> alternatives;
                navigate_with_blackboard(ship, game_map, target, best_dir, alternatives);

                hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
                return MoveRequest{ship->id, ship->position, desired,
                                   best_dir, constants::EXPLORE_PRIORITY, alternatives};
            }
        }

        // 2. Chercher une nouvelle cible via la heatmap
        hlt::Position target = bb.find_best_explore_target(game_map, ship->position, ship->id);

        if (target != ship->position)
        {
            // Persister la cible
            bb.persistent_targets[ship->id] = target;
            bb.targeted_cells[target] = ship->id;

            hlt::Direction best_dir;
            std::vector<hlt::Direction> alternatives;
            navigate_with_blackboard(ship, game_map, target, best_dir, alternatives);

            hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
            return MoveRequest{ship->id, ship->position, desired,
                               best_dir, constants::EXPLORE_PRIORITY, alternatives};
        }

        // 3. Fallback : meilleure case adjacente
        int max_halite = -1;
        hlt::Direction best_direction = hlt::Direction::STILL;
        hlt::Position best_adj = ship->position;

        for (const auto &direction : hlt::ALL_CARDINALS)
        {
            hlt::Position target_pos = ship->position.directional_offset(direction);
            int cell_halite = game_map.at(target_pos)->halite;

            if (cell_halite > max_halite)
            {
                max_halite = cell_halite;
                best_adj = target_pos;
                best_direction = direction;
            }
        }

        if (best_adj == ship->position)
        {
            std::vector<hlt::Direction> alternatives(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
            return MoveRequest{ship->id, ship->position, ship->position,
                               hlt::Direction::STILL, constants::EXPLORE_PRIORITY, alternatives};
        }

        std::vector<std::pair<int, hlt::Direction>> scored_dirs;
        for (const auto &direction : hlt::ALL_CARDINALS)
        {
            if (direction == best_direction)
                continue;
            hlt::Position alt_pos = ship->position.directional_offset(direction);
            scored_dirs.push_back({game_map.at(alt_pos)->halite, direction});
        }

        std::sort(scored_dirs.begin(), scored_dirs.end(),
                  [](const auto &a, const auto &b)
                  { return a.first > b.first; });

        std::vector<hlt::Direction> alternatives;
        for (const auto &sd : scored_dirs)
            alternatives.push_back(sd.second);

        return MoveRequest{ship->id, ship->position, best_adj,
                           best_direction, constants::EXPLORE_PRIORITY, alternatives};
    }

    // COLLECT
    MoveRequest ShipCollectState::execute(std::shared_ptr<hlt::Ship> ship,
                                          hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        // Si la cell actuelle a du halite, rester sur place
        // Alternatives : toutes les directions cardinales triées par halite décroissante
        std::vector<std::pair<int, hlt::Direction>> scored_dirs;
        for (const auto &direction : hlt::ALL_CARDINALS)
        {
            hlt::Position alt_pos = ship->position.directional_offset(direction);
            scored_dirs.push_back({game_map.at(alt_pos)->halite, direction});
        }

        // Tri décroissant par halite
        std::sort(scored_dirs.begin(), scored_dirs.end(),
                  [](const auto &a, const auto &b)
                  { return a.first > b.first; });

        // Alternatives : toutes les directions cardinales triées par halite décroissante
        std::vector<hlt::Direction> alternatives;
        for (const auto &sd : scored_dirs)
            alternatives.push_back(sd.second);

        return MoveRequest{ship->id, ship->position, ship->position,
                           hlt::Direction::STILL, constants::COLLECT_PRIORITY, alternatives};
    }

    // RETURN
    MoveRequest ShipReturnState::execute(std::shared_ptr<hlt::Ship> ship,
                                         hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        navigate_with_blackboard(ship, game_map, shipyard_position, best_dir, alternatives);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, constants::RETURN_PRIORITY, alternatives};
    }

    // FLEE
    MoveRequest ShipFleeState::execute(std::shared_ptr<hlt::Ship> ship,
                                       hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        // Fuir dans la direction qui maximise la distance au shipyard
        int max_distance = -1;
        hlt::Direction best_dir = hlt::Direction::STILL;
        std::vector<std::pair<int, hlt::Direction>> scored_dirs;

        for (const auto &direction : hlt::ALL_CARDINALS)
        {
            hlt::Position target = game_map.normalize(ship->position.directional_offset(direction));
            int dist = game_map.calculate_distance(target, shipyard_position);
            
            if (dist > max_distance)
            {
                max_distance = dist;
                best_dir = direction;
            }
            scored_dirs.push_back({dist, direction});
        }

        // Trier les directions par distance décroissante
        std::sort(scored_dirs.begin(), scored_dirs.end(),
                  [](const auto &a, const auto &b)
                  { return a.first > b.first; });

        std::vector<hlt::Direction> alternatives;
        for (const auto &sd : scored_dirs)
        {
            if (sd.second != best_dir)
                alternatives.push_back(sd.second);
        }

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));

        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, constants::FLEE_PRIORITY, alternatives};
    }

    // URGENT RETURN
    MoveRequest ShipUrgentReturnState::execute(std::shared_ptr<hlt::Ship> ship,
                                               hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        navigate_with_blackboard(ship, game_map, shipyard_position, best_dir, alternatives);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, constants::URGENT_RETURN_PRIORITY, alternatives};
    }
} // namespace bot