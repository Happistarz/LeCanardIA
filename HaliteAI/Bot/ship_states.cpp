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
    // Gere aussi les ships en oscillation en forcant une rupture de pattern
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

        // Si le ship oscille, forcer une direction alternative pour casser le pattern
        if (bb.is_ship_oscillating(ship->id) && !out_alternatives.empty())
        {
            // Chercher une alternative perpendiculaire (pas la meme direction ni l'inverse)
            for (size_t i = 0; i < out_alternatives.size(); ++i)
            {
                hlt::Position alt_pos = game_map.normalize(
                    ship->position.directional_offset(out_alternatives[i]));
                bool safe = bb.danger_zones.find(alt_pos) == bb.danger_zones.end();
                bool not_stuck = bb.stuck_positions.find(alt_pos) == bb.stuck_positions.end();

                if (safe && not_stuck)
                {
                    // Swap : l'alternative devient la direction principale
                    hlt::Direction old_best = out_best_dir;
                    out_best_dir = out_alternatives[i];
                    out_alternatives[i] = old_best;
                    break;
                }
            }
        }
    }

    // EXPLORE
    MoveRequest ShipExploreState::execute(std::shared_ptr<hlt::Ship> ship,
                                          hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        Blackboard &bb = Blackboard::get_instance();

        // Si le ship oscille, abandonner sa cible pour forcer un nouveau chemin
        if (bb.is_ship_oscillating(ship->id))
        {
            bb.persistent_targets.erase(ship->id);
        }

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

    // FLEE — fuit vers le depot le plus proche en maximisant la distance aux menaces
    MoveRequest ShipFleeState::execute(std::shared_ptr<hlt::Ship> ship,
                                       hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        const Blackboard &bb = Blackboard::get_instance();

        // Collecter les menaces proches (ennemis plus legers que nous)
        std::vector<hlt::Position> threats;
        for (const auto &enemy : bb.enemy_ships)
        {
            if (enemy.halite < ship->halite)
            {
                int dist = game_map.calculate_distance(ship->position, enemy.position);
                if (dist <= constants::FLEE_THREAT_RADIUS + 1)
                    threats.push_back(enemy.position);
            }
        }

        // Si plus de menace, naviguer normalement vers le depot
        if (threats.empty())
        {
            hlt::Direction best_dir;
            std::vector<hlt::Direction> alternatives;
            navigate_with_blackboard(ship, game_map, shipyard_position, best_dir, alternatives);
            hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
            return MoveRequest{ship->id, ship->position, desired,
                               best_dir, constants::FLEE_PRIORITY, alternatives};
        }

        // Scorer chaque direction (4 cardinales + STILL)
        struct ScoredMove
        {
            hlt::Direction dir;
            int safety;    // Somme des distances aux menaces
            int to_depot;  // Distance au depot (plus petit = mieux)
            int cell_cost; // Cout de deplacement sur cette cell
        };

        std::vector<ScoredMove> moves;

        // STILL : rester sur place (l'ennemi paye pour venir a nous)
        {
            int safety = 0;
            for (const auto &t : threats)
                safety += game_map.calculate_distance(ship->position, t);
            moves.push_back({hlt::Direction::STILL, safety,
                             game_map.calculate_distance(ship->position, shipyard_position), 0});
        }

        for (const auto &dir : hlt::ALL_CARDINALS)
        {
            hlt::Position target = game_map.normalize(ship->position.directional_offset(dir));

            int safety = 0;
            for (const auto &t : threats)
                safety += game_map.calculate_distance(target, t);

            bool in_danger = bb.danger_zones.find(target) != bb.danger_zones.end();
            if (in_danger)
                safety -= 100; // Grosse penalite si on fonce dans un ennemi

            int to_depot = game_map.calculate_distance(target, shipyard_position);
            int cell_cost = game_map.at(target)->halite / hlt::constants::MOVE_COST_RATIO;

            moves.push_back({dir, safety, to_depot, cell_cost});
        }

        // Trier : maximiser safety, puis minimiser distance au depot
        std::sort(moves.begin(), moves.end(),
                  [](const ScoredMove &a, const ScoredMove &b)
                  {
                      if (a.safety != b.safety)
                          return a.safety > b.safety;
                      return a.to_depot < b.to_depot;
                  });

        hlt::Direction best_dir = moves[0].dir;
        std::vector<hlt::Direction> alternatives;
        for (size_t i = 1; i < moves.size(); ++i)
            if (moves[i].dir != hlt::Direction::STILL)
                alternatives.push_back(moves[i].dir);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, constants::FLEE_PRIORITY, alternatives};
    }

    // HUNT — ship leger qui pourchasse un enemy riche
    MoveRequest ShipHuntState::execute(std::shared_ptr<hlt::Ship> ship,
                                       hlt::GameMap &game_map, const hlt::Position &shipyard_position)
    {
        Blackboard &bb = Blackboard::get_instance();

        hlt::Position target = bb.find_hunt_target(game_map, ship->position, ship->id);

        // Pas de cible valide → fallback explore
        if (target.x < 0)
        {
            return ShipExploreState::execute(ship, game_map, shipyard_position);
        }

        // Naviguer vers la cible en evitant les defenders mais PAS la cible elle-meme
        // Creer un set de dangers sans la position de la cible
        std::set<hlt::Position> hunt_dangers = bb.danger_zones;
        hunt_dangers.erase(target);

        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;
        map_utils::navigate_toward(ship, game_map, target,
                                   bb.stuck_positions, hunt_dangers,
                                   best_dir, alternatives);

        hlt::Position desired = game_map.normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired,
                           best_dir, constants::HUNT_PRIORITY, alternatives};
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