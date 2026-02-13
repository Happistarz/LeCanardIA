#pragma once

#include "move_request.hpp"
#include "hlt/entity.hpp"
#include "hlt/position.hpp"
#include "hlt/direction.hpp"
#include "hlt/command.hpp"
#include "hlt/game_map.hpp"
#include "hlt/ship.hpp"

#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

namespace bot
{
    /// Gestion de traffic (singleton)
    ///
    /// Etapes :
    /// - init() chaque tour avec les données du jeu
    /// - Collecte les MoveRequest de chaque ShipFSM
    /// - Ajuste les PRIORITY
    /// - Tri par PRIORITY HIGH TO LOW
    /// - Résout les conflits de mouvement (swaps, collisions)
    /// - MoveResult final pour chaque ship
    class TrafficManager
    {
    public:
        static TrafficManager &instance();

        // Non-copiable
        TrafficManager(const TrafficManager &) = delete;
        TrafficManager &operator=(const TrafficManager &) = delete;

        /// Initialise le contexte du tour courant
        void init(hlt::GameMap &game_map,
                  const std::vector<hlt::Position> &dropoff_positions,
                  const std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Ship>> &ships,
                  int turns_remaining);

        /// Résout tous les conflits de mouvement
        std::vector<MoveResult> resolve_all(std::vector<MoveRequest> &requests);

    private:
        TrafficManager() = default;

        bool is_dropoff(const hlt::Position &pos) const;

        void adjust_priorities(std::vector<MoveRequest> &requests);

        void resolve_conflicts(
            std::vector<MoveRequest> &requests,
            std::vector<MoveResult> &results,
            std::unordered_set<size_t> &resolved_indices);

        void manage_stuck_ships(
            std::vector<MoveRequest> &requests,
            std::vector<MoveResult> &results,
            std::unordered_set<size_t> &resolved_indices,
            std::unordered_set<hlt::Position> &occupied_positions);

        // Contexte du tour courant (set par init())
        hlt::GameMap *m_game_map = nullptr;
        const std::vector<hlt::Position> *m_dropoff_positions = nullptr;
        const std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Ship>> *m_ships = nullptr;
        int m_turns_remaining = 0;
    };
} // namespace bot