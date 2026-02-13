#include "ship_states.hpp"
#include "blackboard.hpp"
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
        // Alternatives : toutes les directions cardinales
        std::vector<hlt::Direction> alternatives(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
        return MoveRequest{ship->id, ship->position, ship->position,
                           hlt::Direction::STILL, MoveRequest::COLLECT_PRIORITY, alternatives};
    }

    // ── Navigation helper ────────────────────────────────────────
    void ShipExploreState::navigate_toward_static(std::shared_ptr<hlt::Ship> ship,
                                hlt::GameMap &game_map,
                                const hlt::Position &destination,
                                hlt::Direction &out_best_dir,
                                std::vector<hlt::Direction> &out_alternatives)
    {
        const Blackboard &bb = Blackboard::get_instance();

        // If already at destination, stay still
        if (ship->position == destination)
        {
            out_best_dir = hlt::Direction::STILL;
            out_alternatives.assign(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
            return;
        }

        // Directions optimales vers la destination
        std::vector<hlt::Direction> unsafe_moves = game_map.get_unsafe_moves(ship->position, destination);

        // Scorer toutes les directions cardinales
        struct ScoredDir
        {
            hlt::Direction dir;
            int distance;
            bool is_stuck;
            bool is_dangerous;
            bool is_optimal;
        };

        std::vector<ScoredDir> scored;
        for (const auto &dir : hlt::ALL_CARDINALS)
        {
            hlt::Position target = game_map.normalize(ship->position.directional_offset(dir));
            int dist = game_map.calculate_distance(target, destination);
            bool stuck = bb.is_position_stuck(target);
            bool dangerous = !bb.is_position_safe(target);
            bool optimal = false;

            for (const auto &um : unsafe_moves)
            {
                if (um == dir)
                {
                    optimal = true;
                    break;
                }
            }

            scored.push_back({dir, dist, stuck, dangerous, optimal});
        }

        // Trier : non-stuck > non-dangerous > optimal > plus proche
        std::sort(scored.begin(), scored.end(),
                  [](const ScoredDir &a, const ScoredDir &b)
                  {
                      if (a.is_stuck != b.is_stuck)
                          return !a.is_stuck;
                      if (a.is_dangerous != b.is_dangerous)
                          return !a.is_dangerous;
                      if (a.is_optimal != b.is_optimal)
                          return a.is_optimal;
                      return a.distance < b.distance;
                  });

        out_best_dir = scored[0].dir;
        out_alternatives.clear();
        for (size_t i = 1; i < scored.size(); ++i)
            out_alternatives.push_back(scored[i].dir);
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
            if (dist == 0 || game_map.at(target)->halite < Blackboard::PERSISTENT_TARGET_MIN_HALITE)
            {
                bb.persistent_targets.erase(pt_it);
            }
            else
            {
                // Continuer vers le meme target
                bb.targeted_cells[target] = ship->id;

                hlt::Direction best_dir;
                std::vector<hlt::Direction> alternatives;
                navigate_toward_static(ship, game_map, target, best_dir, alternatives);

                hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
                return MoveRequest{ship->id, ship->position, desired,
                                   best_dir, MoveRequest::EXPLORE_PRIORITY, alternatives};
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
            navigate_toward_static(ship, game_map, target, best_dir, alternatives);

            hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
            return MoveRequest{ship->id, ship->position, desired,
                               best_dir, MoveRequest::EXPLORE_PRIORITY, alternatives};
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
                               hlt::Direction::STILL, MoveRequest::EXPLORE_PRIORITY, alternatives};
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
                           best_direction, MoveRequest::EXPLORE_PRIORITY, alternatives};
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
                           hlt::Direction::STILL, MoveRequest::COLLECT_PRIORITY, alternatives};
    }

    // RETURN
    MoveRequest ShipReturnState::execute(std::shared_ptr<hlt::Ship> ship,
                                         hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        ShipExploreState::navigate_toward_static(ship, game_map, shipyard_position, best_dir, alternatives);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, MoveRequest::RETURN_PRIORITY, alternatives};
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
                           best_dir, MoveRequest::FLEE_PRIORITY, alternatives};
    }

    // URGENT RETURN
    MoveRequest ShipUrgentReturnState::execute(std::shared_ptr<hlt::Ship> ship,
                                               hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        ShipExploreState::navigate_toward_static(ship, game_map, shipyard_position, best_dir, alternatives);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, MoveRequest::URGENT_RETURN_PRIORITY, alternatives};
    }
} // namespace bot