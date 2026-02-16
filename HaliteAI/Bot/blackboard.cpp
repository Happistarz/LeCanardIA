#include "blackboard.hpp"
#include "map_utils.hpp"
#include "hlt/game_map.hpp"
#include "hlt/constants.hpp"

#include <cstdlib>

namespace bot
{

    bool Blackboard::is_position_safe(const hlt::Position &pos) const
    {
        return danger_zones.find(pos) == danger_zones.end();
    }

    bool Blackboard::is_position_reserved(const hlt::Position &pos) const
    {
        return reserved_positions.find(pos) != reserved_positions.end();
    }

    void Blackboard::reserve_position(const hlt::Position &pos, hlt::EntityId ship_id)
    {
        reserved_positions.insert(pos);
        targeted_cells[pos] = ship_id;
    }

    bool Blackboard::is_position_stuck(const hlt::Position &pos) const
    {
        return stuck_positions.find(pos) != stuck_positions.end();
    }

    void Blackboard::clear_turn_data()
    {
        reserved_positions.clear();
        targeted_cells.clear();
        danger_zones.clear();
        stuck_positions.clear();
        enemy_ships.clear();
        inspired_zones.clear();
        oscillating_ships.clear();
        drop_positions.clear();
        should_spawn = false;
    }

    void Blackboard::compute_heatmap(const hlt::GameMap &game_map)
    {
        int w = game_map.width;
        int h = game_map.height;
        halite_heatmap.assign(h, std::vector<int>(w, 0));

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int score = 0;
                for (int dy = -constants::HEATMAP_RADIUS; dy <= constants::HEATMAP_RADIUS; ++dy)
                {
                    for (int dx = -constants::HEATMAP_RADIUS; dx <= constants::HEATMAP_RADIUS; ++dx)
                    {
                        int dist = std::abs(dx) + std::abs(dy);
                        if (dist > constants::HEATMAP_RADIUS)
                            continue;

                        int nx = ((x + dx) % w + w) % w;
                        int ny = ((y + dy) % h + h) % h;

                        // Ponderation : les cells proches comptent plus
                        int weight = constants::HEATMAP_RADIUS + 1 - dist;
                        score += game_map.cells[ny][nx].halite * weight;
                    }
                }
                halite_heatmap[y][x] = score;
            }
        }
    }

    hlt::Position Blackboard::find_best_explore_target(const hlt::GameMap &game_map,
                                                       const hlt::Position &ship_pos,
                                                       hlt::EntityId ship_id,
                                                       const std::vector<hlt::Position> &drop_positions) const
    {
        int w = game_map.width;
        int h = game_map.height;

        int best_score = -1;
        hlt::Position best_pos = ship_pos;

        for (int dy = -constants::EXPLORE_SEARCH_RADIUS; dy <= constants::EXPLORE_SEARCH_RADIUS; ++dy)
        {
            for (int dx = -constants::EXPLORE_SEARCH_RADIUS; dx <= constants::EXPLORE_SEARCH_RADIUS; ++dx)
            {
                int dist = std::abs(dx) + std::abs(dy);
                if (dist > constants::EXPLORE_SEARCH_RADIUS || dist == 0)
                    continue;

                int nx = ((ship_pos.x + dx) % w + w) % w;
                int ny = ((ship_pos.y + dy) % h + h) % h;
                hlt::Position candidate(nx, ny);

                // Skip les cells deja target par un autre ship
                auto it = targeted_cells.find(candidate);
                if (it != targeted_cells.end() && it->second != ship_id)
                    continue;

                int heatmap_val = halite_heatmap[ny][nx];

                if (inspired_zones.find(candidate) != inspired_zones.end())
                    heatmap_val = heatmap_val * 3;

                for (const auto& drop : drop_positions)
                {
                    int drop_dist = map_utils::toroidal_distance(candidate, drop, w, h);
                    if (drop_dist <= constants::HEATMAP_RADIUS)
                        heatmap_val = heatmap_val * 3 / 2;
                }

                // Penalite pour la distance : favoriser les cells proches pour reduire le temps de voyage
                int travel_cost = dist * (average_halite / (hlt::constants::MOVE_COST_RATIO > 0 ? hlt::constants::MOVE_COST_RATIO : 10));
                int net_score = heatmap_val - travel_cost;
                if (net_score < 0) net_score = 0;
                int effective_score = net_score / (dist + 1);

                if (effective_score > best_score)
                {
                    best_score = effective_score;
                    best_pos = candidate;
                }
            }
        }

        return best_pos;
    }

    hlt::Position Blackboard::find_best_dropoff_position(
        const hlt::GameMap &game_map,
        const std::vector<hlt::Position> &existing_depots,
        int min_depot_distance) const
    {
        int w = game_map.width;
        int h = game_map.height;
        int best_score = -1;
        hlt::Position best_pos(-1, -1);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                hlt::Position candidate(x, y);

                // Verif la distance minimale a tous les drops existants
                bool too_close = false;
                for (const auto &depot : existing_depots)
                {
                    int dist = map_utils::toroidal_distance(candidate, depot, w, h);
                    if (dist < min_depot_distance)
                    {
                        too_close = true;
                        break;
                    }
                }
                if (too_close)
                    continue;

                int score = halite_heatmap[y][x];
                if (score > best_score)
                {
                    best_score = score;
                    best_pos = candidate;
                }
            }
        }

        return best_pos;
    }

    // ANTI-OSCILLATION

    void Blackboard::update_position_history(hlt::EntityId ship_id, const hlt::Position &pos)
    {
        auto &history = position_history[ship_id];
        history.push_back(pos);
        if (history.size() > 4)
            history.pop_front();

        // Detection du pattern A -> B -> A -> B
        if (history.size() == 4)
        {
            if (history[0] == history[2] && history[1] == history[3] && !(history[0] == history[1]))
            {
                oscillating_ships.insert(ship_id);
            }
        }
    }

    bool Blackboard::is_ship_oscillating(hlt::EntityId ship_id) const
    {
        return oscillating_ships.find(ship_id) != oscillating_ships.end();
    }

    // INSPIRATION

    void Blackboard::compute_inspired_zones(int map_width, int map_height)
    {
        if (!hlt::constants::INSPIRATION_ENABLED)
            return;

        int radius = hlt::constants::INSPIRATION_RADIUS;
        int needed = hlt::constants::INSPIRATION_SHIP_COUNT;

        // Pour chaque case de la map, compter les ennemis dans le rayon
        for (int y = 0; y < map_height; ++y)
        {
            for (int x = 0; x < map_width; ++x)
            {
                hlt::Position pos(x, y);
                int count = 0;
                for (const auto &enemy : enemy_ships)
                {
                    int dist = map_utils::toroidal_distance(pos, enemy.position, map_width, map_height);
                    if (dist <= radius)
                    {
                        ++count;
                        if (count >= needed)
                        {
                            inspired_zones.insert(pos);
                            break;
                        }
                    }
                }
            }
        }
    }

    // COMBAT

    hlt::Position Blackboard::find_hunt_target(const hlt::GameMap &game_map,
                                               const hlt::Position &ship_pos,
                                               hlt::EntityId ship_id)
    {
        int w = halite_heatmap[0].size();
        int h = halite_heatmap.size();
        int search_radius = (current_phase == GamePhase::LATE)
                                ? constants::HUNT_RADIUS_LATE
                                : constants::HUNT_RADIUS;

        // Verif si le target actuel est encore valide
        auto ht_it = hunt_targets.find(ship_id);
        if (ht_it != hunt_targets.end())
        {
            for (const auto &enemy : enemy_ships)
            {
                if (enemy.id == ht_it->second &&
                    enemy.halite >= constants::HUNT_MIN_ENEMY_HALITE / 2)
                {
                    int dist = map_utils::toroidal_distance(ship_pos, enemy.position, w, h);
                    if (dist <= search_radius * 2)
                        return enemy.position;
                }
            }
            hunt_targets.erase(ht_it);
        }

        // Chercher une nouvelle target
        int best_score = -1;
        hlt::Position best_pos(-1, -1);
        hlt::EntityId best_enemy_id = -1;

        for (const auto &enemy : enemy_ships)
        {
            if (enemy.halite < constants::HUNT_MIN_ENEMY_HALITE)
                continue;

            int dist = map_utils::toroidal_distance(ship_pos, enemy.position, w, h);
            if (dist > search_radius || dist == 0)
                continue;

            // Compter les defenders autour de la target
            int defender_count = 0;
            for (const auto &other : enemy_ships)
            {
                if (other.id == enemy.id)
                    continue;
                if (other.halite < constants::DEFENDER_MAX_HALITE)
                {
                    int d = map_utils::toroidal_distance(other.position, enemy.position, w, h);
                    if (d <= constants::HUNT_DEFENDER_RADIUS)
                        ++defender_count;
                }
            }

            // Score = halite de la target - distance * 100 - defenders * 300
            // 100 -> penalise les targets lointaines, 300 -> penalise les targets bien defendues
            int score = enemy.halite - dist * 100 - defender_count * 300;
            if (score > best_score)
            {
                best_score = score;
                best_pos = enemy.position;
                best_enemy_id = enemy.id;
            }
        }

        if (best_enemy_id >= 0)
            hunt_targets[ship_id] = best_enemy_id;

        return best_pos;
    }

    bool Blackboard::has_nearby_threat(const hlt::GameMap &game_map,
                                       const hlt::Position &ship_pos,
                                       int ship_halite) const
    {
        if (ship_halite < constants::FLEE_MIN_CARGO)
            return false;

        int w = static_cast<int>(halite_heatmap[0].size());
        int h = static_cast<int>(halite_heatmap.size());

        for (const auto &enemy : enemy_ships)
        {
            if (enemy.halite < ship_halite)
            {
                int dist = map_utils::toroidal_distance(ship_pos, enemy.position, w, h);
                if (dist <= constants::FLEE_THREAT_RADIUS)
                    return true;
            }
        }
        return false;
    }
} // namespace bot