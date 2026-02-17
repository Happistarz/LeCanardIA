#include "map_utils.hpp"
#include "hlt/constants.hpp"

namespace bot
{
    namespace map_utils
    {
        int sum_halite_in_radius(const hlt::GameMap &game_map,
                                 const hlt::Position &center, int radius)
        {
            int w = game_map.width;
            int h = game_map.height;
            int total = 0;

            for (int dy = -radius; dy <= radius; ++dy)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (std::abs(dx) + std::abs(dy) > radius)
                        continue;

                    // Wrap-around toroidal
                    int nx = ((center.x + dx) % w + w) % w;

                    // Wrap-around toroidal
                    int ny = ((center.y + dy) % h + h) % h;

                    total += game_map.cells[ny][nx].halite;
                }
            }

            return total;
        }

        int count_in_radius(const hlt::Position &center,
                            const std::vector<hlt::Position> &positions,
                            int radius, int width, int height)
        {
            int count = 0;

            for (const auto &pos : positions)
            {
                if (toroidal_distance(center, pos, width, height) <= radius)
                    ++count;
            }

            return count;
        }

        void navigate_toward(std::shared_ptr<hlt::Ship> ship,
                             hlt::GameMap &game_map,
                             const hlt::Position &destination,
                             const std::set<hlt::Position> &stuck_positions,
                             const std::set<hlt::Position> &danger_zones,
                             hlt::Direction &out_best_dir,
                             std::vector<hlt::Direction> &out_alternatives,
                             bool is_returning)
        {
            // Deja a destination : rester sur place
            if (ship->position == destination)
            {
                out_best_dir = hlt::Direction::STILL;
                out_alternatives.assign(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());

                return;
            }

            // Directions optimales vers la destination
            std::vector<hlt::Direction> unsafe_moves = game_map.get_unsafe_moves(ship->position, destination);

            // Scorer toutes les directions
            struct ScoredDir
            {
                hlt::Direction dir;
                int distance;
                int move_cost;
                bool is_stuck;
                bool is_dangerous;
                bool is_optimal;
            };

            // x4 move_cost en return pour eviter le burn
            int cost_weight = is_returning ? 4 : 1;

            std::vector<ScoredDir> scored;
            for (const auto &dir : hlt::ALL_CARDINALS)
            {
                hlt::Position target = game_map.normalize(ship->position.directional_offset(dir));
                int dist = game_map.calculate_distance(target, destination);
                int cost = (game_map.at(target)->halite / hlt::constants::MOVE_COST_RATIO) * cost_weight;

                bool stuck = stuck_positions.find(target) != stuck_positions.end();
                bool dangerous = danger_zones.find(target) != danger_zones.end();
                bool optimal = false;

                for (const auto &um : unsafe_moves)
                {
                    if (um == dir)
                    {
                        optimal = true;
                        break;
                    }
                }

                scored.push_back({dir, dist, cost, stuck, dangerous, optimal});
            }

            // Tri : stuck > danger > optimal > dist > cost
            std::sort(scored.begin(), scored.end(),
                      [](const ScoredDir &a, const ScoredDir &b)
                      {
                          if (a.is_stuck != b.is_stuck)
                              return !a.is_stuck;
                          if (a.is_dangerous != b.is_dangerous)
                              return !a.is_dangerous;
                          if (a.is_optimal != b.is_optimal)
                              return a.is_optimal;
                          if (a.distance != b.distance)
                              return a.distance < b.distance;
                          return a.move_cost < b.move_cost;
                      });

            out_best_dir = scored[0].dir;
            out_alternatives.clear();

            for (size_t i = 1; i < scored.size(); ++i)
                out_alternatives.push_back(scored[i].dir);
        }

    } // namespace map_utils
} // namespace bot
