#pragma once

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
    // Move request / result

    struct MoveRequest
    {
        // Priority constants HIGH TO LOW
        static constexpr int SHIP_ON_DROPOFF_PRIORITY = 100;   // Must leave dropoff
        static constexpr int URGENT_RETURN_NEAR_PRIORITY = 90; // Urgent, distance <= 2
        static constexpr int URGENT_RETURN_PRIORITY = 80;      // Urgent, distance > 2
        static constexpr int FLEE_PRIORITY = 60;               // Fleeing danger
        static constexpr int RETURN_PRIORITY = 50;             // Returning cargo
        static constexpr int EXPLORE_PRIORITY = 20;            // Exploring
        static constexpr int COLLECT_PRIORITY = 10;            // Collecting

        hlt::EntityId m_ship_id;                    // Ship identifier
        hlt::Position m_current;                    // Current position
        hlt::Position m_desired;                    // Desired position
        hlt::Direction m_desired_direction;         // Desired direction
        int m_priority;                             // Processing priority
        std::vector<hlt::Direction> m_alternatives; // Fallback directions
    };

    struct MoveResult
    {
        hlt::EntityId m_ship_id;
        hlt::Direction m_final_direction;
    };

    // Gestionnaire de trafic
    class TrafficManager
    {
    public:
        static TrafficManager &instance();

        TrafficManager(const TrafficManager &) = delete;
        TrafficManager &operator=(const TrafficManager &) = delete;

        // Initialise le context du tour
        void init(hlt::GameMap &game_map,
                  const std::vector<hlt::Position> &dropoff_positions,
                  const std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Ship>> &ships,
                  int turns_remaining);

        // Resout tous les conflits et retourne les directions finales
        std::vector<MoveResult> resolve_all(std::vector<MoveRequest> &requests);

    private:
        TrafficManager() = default;

        // Verifie si une position est un depot
        bool is_dropoff(const hlt::Position &pos) const;

        // Augmente la priorite des ships sur depots ou retour urgent proche
        void adjust_priorities(std::vector<MoveRequest> &requests);

        // Detecte et resout les conflits de swap frontal
        void resolve_conflicts(
            std::vector<MoveRequest> &requests,
            std::vector<MoveResult> &results,
            std::unordered_set<size_t> &resolved_indices);

        // Force STILL pour les ships qui ne peuvent pas bouger
        void manage_stuck_ships(
            std::vector<MoveRequest> &requests,
            std::vector<MoveResult> &results,
            std::unordered_set<size_t> &resolved_indices,
            std::unordered_set<hlt::Position> &occupied_positions);

        // Context du tour
        hlt::GameMap *m_game_map = nullptr;
        const std::vector<hlt::Position> *m_dropoff_positions = nullptr;
        const std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Ship>> *m_ships = nullptr;
        int m_turns_remaining = 0;
    };
} // namespace bot