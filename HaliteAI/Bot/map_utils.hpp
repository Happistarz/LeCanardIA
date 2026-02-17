#pragma once

#include "hlt/position.hpp"
#include "hlt/direction.hpp"
#include "hlt/game_map.hpp"
#include "hlt/ship.hpp"

#include <vector>
#include <memory>
#include <set>
#include <cstdlib>
#include <algorithm>

namespace bot
{
    namespace map_utils
    {
        /// Calcule la distance toroidale entre deux positions
        inline int toroidal_distance(const hlt::Position &a, const hlt::Position &b,
                                     int width, int height)
        {
            int dx = std::abs(a.x - b.x);
            int dy = std::abs(a.y - b.y);

            return std::min(dx, width - dx) + std::min(dy, height - dy);
        }

        /// Trouve la position la plus proche dans un vecteur
        inline hlt::Position closest_position(const hlt::Position &from,
                                              const std::vector<hlt::Position> &candidates,
                                              int width, int height)
        {
            hlt::Position best = candidates[0];
            int best_dist = toroidal_distance(from, best, width, height);

            for (size_t i = 1; i < candidates.size(); ++i)
            {
                int d = toroidal_distance(from, candidates[i], width, height);
                if (d >= best_dist)
                    continue;
                best_dist = d;
                best = candidates[i];
            }

            return best;
        }

        /// Somme le halite dans un rayon toroidal
        int sum_halite_in_radius(const hlt::GameMap &game_map,
                                 const hlt::Position &center, int radius);

        /// Compte combien de positions du vecteur sont dans le rayon
        int count_in_radius(const hlt::Position &center,
                            const std::vector<hlt::Position> &positions,
                            int radius, int width, int height);

        /// Navigue selon plusieurs criteres
        void navigate_toward(std::shared_ptr<hlt::Ship> ship,
                             hlt::GameMap &game_map,
                             const hlt::Position &destination,
                             const std::set<hlt::Position> &stuck_positions,
                             const std::set<hlt::Position> &danger_zones,
                             hlt::Direction &out_best_dir,
                             std::vector<hlt::Direction> &out_alternatives,
                             bool is_returning = false);

    } // namespace map_utils
} // namespace bot
