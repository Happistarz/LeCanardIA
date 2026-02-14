#pragma once

#include "bot_constants.hpp"
#include "hlt/types.hpp"
#include <set>
#include <map>
#include <vector>
#include "hlt/position.hpp"

namespace hlt { struct GameMap; }

namespace bot
{

    enum class GamePhase
    {
        EARLY,   // 0-25% des tours
        MID,     // 25-60% des tours
        LATE,    // 60-85% des tours
        ENDGAME  // 85-100% des tours
    };

    /// Informations d'un ship ennemi, reutilisable pour le combat et l'analyse
    struct EnemyShipInfo
    {
        hlt::EntityId id;
        hlt::Position position;
        int halite;
    };

    struct Blackboard
    {

        // Création du Singleton
        static Blackboard &get_instance()
        {
            static Blackboard instance;
            return instance;
        }
        Blackboard(Blackboard const &) = delete;
        Blackboard(Blackboard &&) = delete;

    private:
        // Constructeur privé
        Blackboard() : current_phase(), average_halite(0), total_ships_alive(0), should_spawn(false)
        {
        }

    public:
        std::set<hlt::Position> reserved_positions;            // Cases occupées en ce moment
        std::map<hlt::Position, hlt::EntityId> targeted_cells; // Cases "destination" d'un bateau

        std::set<hlt::Position> danger_zones;    // Cases de position dangereuse
        std::set<hlt::Position> stuck_positions; // Cases occupées par des ships physiquement coincés

        // ── Combat ────────────────────────────────────────────────

        /// Tous les ships ennemis ce tour (peuple par update_blackboard)
        std::vector<EnemyShipInfo> enemy_ships;

        /// Cibles de chasse persistantes : my_ship_id → target_enemy_id
        std::map<hlt::EntityId, hlt::EntityId> hunt_targets;

        /// Trouve la meilleure cible de chasse pour un ship
        /// Retourne la position de la cible ou (-1,-1) si aucune
        hlt::Position find_hunt_target(const hlt::GameMap &game_map,
                                        const hlt::Position &ship_pos,
                                        hlt::EntityId ship_id);

        /// Verifie si un ship ennemi menacant est a portee de flee
        bool has_nearby_threat(const hlt::GameMap &game_map,
                               const hlt::Position &ship_pos,
                               int ship_halite) const;

        bool is_position_stuck(const hlt::Position &pos) const; // Case bloquée par un ship stuck ?
        GamePhase current_phase;                                // Phase actuelle
        int average_halite;                                     // Halite moyen par case
        int total_ships_alive;                                  // Nombre de bateaux

        bool should_spawn; // Faut-il spawn ce tour ?

        bool is_position_safe(const hlt::Position &pos) const;                  // Case safe ?
        bool is_position_reserved(const hlt::Position &pos) const;              // Case occupée ?
        void reserve_position(const hlt::Position &pos, hlt::EntityId ship_id); // Reserver une case
        void clear_turn_data();                                                 // Reset des données temporaires

        // ── Clustering / Heatmap ──────────────────────────────────


        // ── Dropoff ─────────────────────────────────────────

        /// Position persistante du futur dropoff (-1,-1 si aucun)
        hlt::Position planned_dropoff_pos{-1, -1};
        /// Ship assigne a la construction du dropoff (-1 si aucun)
        hlt::EntityId dropoff_ship_id = -1;

        /// Trouve la meilleure position pour un dropoff
        hlt::Position find_best_dropoff_position(
            const hlt::GameMap &game_map,
            const std::vector<hlt::Position> &existing_depots,
            int min_depot_distance) const;

        std::vector<std::vector<int>> halite_heatmap;

        /// Targets persistants (survivent entre les tours, evite l'oscillation)
        std::map<hlt::EntityId, hlt::Position> persistent_targets;

        /// Calcule la heatmap de halite (somme ponderee dans un rayon)
        void compute_heatmap(const hlt::GameMap &game_map);

        /// Trouve la meilleure cible d'exploration pour un ship
        hlt::Position find_best_explore_target(const hlt::GameMap &game_map,
                                                const hlt::Position &ship_pos,
                                                hlt::EntityId ship_id) const;
    };

} // namespace bot