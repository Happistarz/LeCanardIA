#include "blackboard.hpp"
#include "map_utils.hpp"
#include "hlt/game_map.hpp"

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

                        // Ponderation : les cases proches comptent plus
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
                                                       hlt::EntityId ship_id) const
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

                // Ignorer les cases deja ciblees par un autre ship
                auto it = targeted_cells.find(candidate);
                if (it != targeted_cells.end() && it->second != ship_id)
                    continue;

                // Score = heatmap / (distance + 1) pour favoriser les zones proches et riches
                int heatmap_val = halite_heatmap[ny][nx];
                int effective_score = heatmap_val / (dist + 1);

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

                // Verifier la distance minimale a tous les depots existants
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

    // ── Combat ──────────────────────────────────────────────────

    hlt::Position Blackboard::find_hunt_target(const hlt::GameMap &game_map,
                                                const hlt::Position &ship_pos,
                                                hlt::EntityId ship_id)
    {
        int w = halite_heatmap[0].size();
        int h = halite_heatmap.size();
        int search_radius = (current_phase == GamePhase::LATE)
                                ? constants::HUNT_RADIUS_LATE
                                : constants::HUNT_RADIUS;

        // Verifier si le target actuel est encore valide
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

        // Chercher une nouvelle cible
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

            // Compter les defenders autour de la cible
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