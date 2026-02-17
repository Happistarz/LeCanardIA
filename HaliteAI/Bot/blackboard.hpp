#pragma once

#include "bot_constants.hpp"
#include "hlt/types.hpp"
#include <set>
#include <map>
#include <deque>
#include <vector>
#include "hlt/position.hpp"

namespace hlt
{
    struct GameMap;
}

namespace bot
{

    enum class GamePhase
    {
        EARLY,  // 0-25% des tours
        MID,    // 25-60% des tours
        LATE,   // 60-85% des tours
        ENDGAME // 85-100% des tours
    };

    struct EnemyShipInfo
    {
        hlt::EntityId id;
        hlt::Position position;
        int halite;
    };

    /// Resultat d'une simulation d'extraction sur une cell
    struct MiningEstimate
    {
        int halite_extracted; // Halite net extrait apres simulation
        int mine_turns;       // Nombre de tours passes a miner
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
        std::set<hlt::Position> reserved_positions;            // Cells occupées en ce moment
        std::map<hlt::Position, hlt::EntityId> targeted_cells; // Cells "destination" d'un ship

        std::set<hlt::Position> danger_zones;    // Cells de position dangereuse
        std::set<hlt::Position> stuck_positions; // Cells occupées par des ships physiquement stuck

        // ANTI-OSCILLATION

        /// Historique des 4 dernieres positions de chaque ship
        std::map<hlt::EntityId, std::deque<hlt::Position>> position_history;

        /// Ships actuellement en oscillation
        std::set<hlt::EntityId> oscillating_ships;

        /// Update l'historique et detecte les oscillations
        void update_position_history(hlt::EntityId ship_id, const hlt::Position &pos);

        /// Retourne true si ce ship oscille
        bool is_ship_oscillating(hlt::EntityId ship_id) const;

        // INSPIRATION

        /// Cells ou un ship serait inspire (>=2 ennemis dans INSPIRATION_RADIUS)
        std::set<hlt::Position> inspired_zones;

        /// Calcule les zones d'inspiration a partir des positions ennemies
        void compute_inspired_zones(int map_width, int map_height);

        // COMBAT

        /// Tous les ships ennemis ce tour
        std::vector<EnemyShipInfo> enemy_ships;

        /// Cibles de chasse persistantes : my_ship_id → target_enemy_id
        std::map<hlt::EntityId, hlt::EntityId> hunt_targets;

        /// Trouve le meilleur target d'un ship pour la chasse (retourne (-1,-1) si aucun)
        hlt::Position find_hunt_target(const hlt::GameMap &game_map,
                                       const hlt::Position &ship_pos,
                                       hlt::EntityId ship_id);

        /// Verif si un ship ennemi menacant est a portee de flee
        bool has_nearby_threat(const hlt::GameMap &game_map,
                               const hlt::Position &ship_pos,
                               int ship_halite) const;

        bool is_position_stuck(const hlt::Position &pos) const; // Cell bloquée par un ship stuck ?
        GamePhase current_phase;                                // Phase actuelle
        int average_halite;                                     // Halite moyen par cell
        int total_ships_alive;                                  // Nombre de ship

        bool should_spawn; // Faut-il spawn ce tour ?

        bool is_position_safe(const hlt::Position &pos) const;                  // Cell safe ?
        bool is_position_reserved(const hlt::Position &pos) const;              // Cell occupée ?
        void reserve_position(const hlt::Position &pos, hlt::EntityId ship_id); // Reserver une cell
        void clear_turn_data();                                                 // Reset des données temporaires

        // CLUSTERING / HEATMAP

        // DROPOFF

        /// Position planifiée pour le prochain dropoff (-1,-1 si aucun)
        hlt::Position planned_dropoff_pos{-1, -1};
        /// Ship assigne a la construction du dropoff (-1 si aucun)
        hlt::EntityId dropoff_ship_id = -1;

        /// Position du dernier dropoff construit (-1,-1 si aucun)
        hlt::Position recent_dropoff_pos{-1, -1};
        /// Age en tours depuis la construction du dernier dropoff (0 = ce tour)
        int recent_dropoff_age = -1;

        std::vector<hlt::Position> drop_positions;

        /// Trouve la meilleure pos pour un dropoff
        hlt::Position find_best_dropoff_position(
            const hlt::GameMap &game_map,
            const std::vector<hlt::Position> &existing_depots,
            int min_depot_distance) const;

        std::vector<std::vector<int>> halite_heatmap;

        /// Targets persistants
        std::map<hlt::EntityId, hlt::Position> persistent_targets;

        /// Calcule la heatmap de halite
        void compute_heatmap(const hlt::GameMap &game_map);

        /// Simule l'extraction tour par tour sur une cell (retourne halite extrait + nb tours)
        MiningEstimate estimate_mining(int cell_halite, int ship_cargo, bool inspired) const;

        /// Trouve le best target explore via HPT = halite_net / (aller + mine + retour)
        hlt::Position find_best_explore_target(const hlt::GameMap &game_map,
                                               const hlt::Position &ship_pos,
                                               hlt::EntityId ship_id,
                                               int ship_cargo,
                                               const std::vector<hlt::Position> &drop_positions) const;
    };

} // namespace bot