#pragma once

/// Utilitaires de navigation et de carte reutilisables.
/// Independant du Blackboard et de la FSM : peut etre reutilise
/// dans tout projet utilisant un systeme de grille toroidale.

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
        /// Distance de Manhattan sur une grille toroidale.
        /// Ne depend d'aucun etat mutable, peut etre appelee n'importe ou.
        inline int toroidal_distance(const hlt::Position &a, const hlt::Position &b,
                                     int width, int height)
        {
            int dx = std::abs(a.x - b.x);
            int dy = std::abs(a.y - b.y);
            return std::min(dx, width - dx) + std::min(dy, height - dy);
        }

        /// Navigation intelligente vers une destination.
        /// Prend en compte les cases bloquees et dangereuses via des sets injectés.
        /// Retourne la meilleure direction et une liste d'alternatives triees.
        ///
        /// @param ship            Le vaisseau a deplacer
        /// @param game_map        La carte du jeu
        /// @param destination     Position cible
        /// @param stuck_positions Cases occupees par des ships physiquement coinces
        /// @param danger_zones    Cases dangereuses (ennemis, structures ennemies)
        /// @param out_best_dir    [out] Meilleure direction calculee
        /// @param out_alternatives [out] Directions alternatives triees par qualite
        void navigate_toward(std::shared_ptr<hlt::Ship> ship,
                             hlt::GameMap &game_map,
                             const hlt::Position &destination,
                             const std::set<hlt::Position> &stuck_positions,
                             const std::set<hlt::Position> &danger_zones,
                             hlt::Direction &out_best_dir,
                             std::vector<hlt::Direction> &out_alternatives);

    } // namespace map_utils
} // namespace bot
