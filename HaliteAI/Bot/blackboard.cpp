#include "blackboard.hpp"
#include "map_utils.hpp"
#include "hlt/game_map.hpp"
#include "hlt/constants.hpp"

#include <cstdlib>
#include <cmath>

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

    // Heatmap par blur exponentiel separable
    void Blackboard::compute_heatmap(const hlt::GameMap &game_map)
    {
        int w = game_map.width;
        int h = game_map.height;
        double alpha = 0.4;

        std::vector<std::vector<double>> temp(h, std::vector<double>(w, 0.0));
        halite_heatmap.assign(h, std::vector<int>(w, 0));

        // Pass horizontal avec wrap-around
        for (int y = 0; y < h; ++y)
        {
            double acc = 0.0;
            for (int x = 0; x < w * 2; ++x)
            {
                int rx = x % w;
                acc = acc * (1.0 - alpha) + game_map.cells[y][rx].halite * alpha;
                temp[y][rx] += acc;
            }
            acc = 0.0;
            for (int x = w * 2 - 1; x >= 0; --x)
            {
                int rx = x % w;
                acc = acc * (1.0 - alpha) + game_map.cells[y][rx].halite * alpha;
                temp[y][rx] += acc;
            }
        }

        // Pass vertical avec wrap-around
        std::vector<std::vector<double>> result(h, std::vector<double>(w, 0.0));
        for (int x = 0; x < w; ++x)
        {
            double acc = 0.0;
            for (int y = 0; y < h * 2; ++y)
            {
                int ry = y % h;
                acc = acc * (1.0 - alpha) + temp[ry][x] * alpha;
                result[ry][x] += acc;
            }
            acc = 0.0;
            for (int y = h * 2 - 1; y >= 0; --y)
            {
                int ry = y % h;
                acc = acc * (1.0 - alpha) + temp[ry][x] * alpha;
                result[ry][x] += acc;
            }
        }

        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                halite_heatmap[y][x] = static_cast<int>(result[y][x]);
    }

    // Simule l'extraction tour par tour, arrete si marginal < avg/8
    MiningEstimate Blackboard::estimate_mining(int cell_halite, int ship_cargo, bool inspired) const
    {
        int extracted = 0;
        int turns = 0;
        int remaining = cell_halite;
        int cargo = ship_cargo;
        int min_marginal = average_halite / 8;
        if (min_marginal < 1) min_marginal = 1;

        while (remaining > 0 && cargo < hlt::constants::MAX_HALITE)
        {
            int extract_ratio = inspired ? hlt::constants::INSPIRED_EXTRACT_RATIO : hlt::constants::EXTRACT_RATIO;
            int base = remaining / extract_ratio;
            if (base == 0) break;

            int gain = base;
            if (inspired)
                gain += static_cast<int>(base * hlt::constants::INSPIRED_BONUS_MULTIPLIER);

            // Rendement marginal trop faible, on arrete de miner
            if (gain < min_marginal && turns > 0)
                break;

            if (cargo + gain > hlt::constants::MAX_HALITE)
                gain = hlt::constants::MAX_HALITE - cargo;

            extracted += gain;
            cargo += gain;
            remaining -= base;
            turns++;
            if (turns >= 12) break;
        }

        return {extracted, turns > 0 ? turns : 1};
    }

    // Score HPT = halite_net / (travel + mine + return)
    hlt::Position Blackboard::find_best_explore_target(const hlt::GameMap &game_map,
                                                       const hlt::Position &ship_pos,
                                                       hlt::EntityId ship_id,
                                                       int ship_cargo,
                                                       const std::vector<hlt::Position> &drop_positions) const
    {
        int w = game_map.width;
        int h = game_map.height;
        int best_score = -1;
        hlt::Position best_pos = ship_pos;
        int move_cost_ratio = hlt::constants::MOVE_COST_RATIO > 0 ? hlt::constants::MOVE_COST_RATIO : 10;
        int avg_move_burn = average_halite / move_cost_ratio;

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

                auto it = targeted_cells.find(candidate);
                if (it != targeted_cells.end() && it->second != ship_id)
                    continue;

                int cell_halite = game_map.cells[ny][nx].halite;
                bool inspired = inspired_zones.find(candidate) != inspired_zones.end();

                MiningEstimate est = estimate_mining(cell_halite, ship_cargo, inspired);

                // Dist retour = min vers un drop
                int return_dist = dist;
                for (const auto& drop : drop_positions)
                {
                    int dd = map_utils::toroidal_distance(candidate, drop, w, h);
                    if (dd < return_dist) return_dist = dd;
                }

                // HPT net = (extracted - burns) / temps_total
                int travel_burn = dist * avg_move_burn;
                int return_burn = return_dist * avg_move_burn;
                int net_halite = est.halite_extracted - travel_burn - return_burn;
                if (net_halite <= 0)
                    continue;

                int total_time = dist + est.mine_turns + return_dist;
                if (total_time <= 0) total_time = 1;

                int effective_score = (net_halite * 100) / total_time;

                // Tiebreaker heatmap pour les zones denses
                effective_score += halite_heatmap[ny][nx] / 100;

                if (recent_dropoff_pos.x >= 0 && recent_dropoff_age >= 0)
                {
                    int rd_dist = map_utils::toroidal_distance(candidate, recent_dropoff_pos, w, h);
                    if (rd_dist <= constants::DROPOFF_REDIRECT_RADIUS)
                    {
                        int proximity_bonus = (constants::DROPOFF_REDIRECT_RADIUS - rd_dist + 1);
                        effective_score = effective_score * (constants::DROPOFF_REDIRECT_BOOST + proximity_bonus) / constants::DROPOFF_REDIRECT_BOOST;
                    }
                }

                if (effective_score > best_score)
                {
                    best_score = effective_score;
                    best_pos = candidate;
                }
            }
        }

        return best_pos;
    }

    // Meilleur spot dropoff : halite reel + dominance allies + bonus ships proches
    hlt::Position Blackboard::find_best_dropoff_position(
        const hlt::GameMap &game_map,
        const std::vector<hlt::Position> &existing_depots,
        int min_depot_distance) const
    {
        int w = game_map.width;
        int h = game_map.height;
        int best_score = -1;
        hlt::Position best_pos(-1, -1);
        int dropoff_radius = 7;

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                hlt::Position candidate(x, y);

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

                // Somme halite reel dans le rayon
                int real_halite = 0;
                for (int dy2 = -dropoff_radius; dy2 <= dropoff_radius; ++dy2)
                {
                    for (int dx2 = -dropoff_radius; dx2 <= dropoff_radius; ++dx2)
                    {
                        int md = std::abs(dx2) + std::abs(dy2);
                        if (md > dropoff_radius) continue;
                        int nx = ((x + dx2) % w + w) % w;
                        int ny = ((y + dy2) % h + h) % h;
                        real_halite += game_map.cells[ny][nx].halite;
                    }
                }

                // Dominance : allies vs ennemis
                int allies_nearby = 0;
                int enemies_nearby = 0;
                for (const auto &apos : allied_positions)
                {
                    if (map_utils::toroidal_distance(candidate, apos, w, h) <= dropoff_radius)
                        allies_nearby++;
                }
                for (const auto &enemy : enemy_ships)
                {
                    if (map_utils::toroidal_distance(candidate, enemy.position, w, h) <= dropoff_radius)
                        enemies_nearby++;
                }

                // Zone dominee par ennemis, skip
                if (enemies_nearby > allies_nearby + 1)
                    continue;

                // Bonus allies proches
                int ally_bonus = allies_nearby * 500;

                int score = real_halite + ally_bonus;
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

        // Detecte oscillation A -> B -> A -> B
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

        // Compter ennemis dans le rayon d'inspiration par cell
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

        // Target actuel encore valide ?
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

        // Nouvelle target de chasse
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

            // Defenders autour de la target
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