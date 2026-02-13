#include "blackboard.hpp"
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
                for (int dy = -HEATMAP_RADIUS; dy <= HEATMAP_RADIUS; ++dy)
                {
                    for (int dx = -HEATMAP_RADIUS; dx <= HEATMAP_RADIUS; ++dx)
                    {
                        int dist = std::abs(dx) + std::abs(dy);
                        if (dist > HEATMAP_RADIUS)
                            continue;

                        int nx = ((x + dx) % w + w) % w;
                        int ny = ((y + dy) % h + h) % h;

                        // Ponderation : les cases proches comptent plus
                        int weight = HEATMAP_RADIUS + 1 - dist;
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

        for (int dy = -EXPLORE_SEARCH_RADIUS; dy <= EXPLORE_SEARCH_RADIUS; ++dy)
        {
            for (int dx = -EXPLORE_SEARCH_RADIUS; dx <= EXPLORE_SEARCH_RADIUS; ++dx)
            {
                int dist = std::abs(dx) + std::abs(dy);
                if (dist > EXPLORE_SEARCH_RADIUS || dist == 0)
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
} // namespace bot