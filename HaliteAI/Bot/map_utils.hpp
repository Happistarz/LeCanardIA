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
