#include "map_utils.hpp"

namespace bot
{
    namespace map_utils
    {
        void navigate_toward(std::shared_ptr<hlt::Ship> ship,
                             hlt::GameMap &game_map,
                             const hlt::Position &destination,
                             const std::set<hlt::Position> &stuck_positions,
                             const std::set<hlt::Position> &danger_zones,
                             hlt::Direction &out_best_dir,
                             std::vector<hlt::Direction> &out_alternatives)
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

    } // namespace map_utils
} // namespace bot
