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

    /// Resultat simulation extraction sur une cell
    struct MiningEstimate
    {
        int halite_extracted; // Halite net extrait
        int mine_turns;       // Tours de mining
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

        /// Historique des 4 dernieres positions par ship
        std::map<hlt::EntityId, std::deque<hlt::Position>> position_history;

        /// Ships en oscillation
        std::set<hlt::EntityId> oscillating_ships;

        /// Update historique + detecte oscillations
        void update_position_history(hlt::EntityId ship_id, const hlt::Position &pos);

        /// True si le ship oscille
        bool is_ship_oscillating(hlt::EntityId ship_id) const;

        // INSPIRATION

        /// Cells inspirees (>=2 ennemis dans INSPIRATION_RADIUS)
        std::set<hlt::Position> inspired_zones;

        /// Calcule les inspired_zones a partir des ennemis
        void compute_inspired_zones(int map_width, int map_height);

        // COMBAT

        /// Tous les ships ennemis du tour
        std::vector<EnemyShipInfo> enemy_ships;

        /// Cibles de chasse : my_ship -> enemy_id
        std::map<hlt::EntityId, hlt::EntityId> hunt_targets;

        /// Best target de chasse (-1,-1 si rien)
        hlt::Position find_hunt_target(const hlt::GameMap &game_map,
                                       const hlt::Position &ship_pos,
                                       hlt::EntityId ship_id);

        /// Menace ennemi a portee de flee ?
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

        /// Pos planifiee pour le prochain dropoff
        hlt::Position planned_dropoff_pos{-1, -1};
        /// Ship assigne au dropoff
        hlt::EntityId dropoff_ship_id = -1;

        /// Pos du dernier dropoff construit
        hlt::Position recent_dropoff_pos{-1, -1};
        /// Age en tours depuis le dernier dropoff
        int recent_dropoff_age = -1;

        std::vector<hlt::Position> drop_positions;

        /// Positions allies
        std::vector<hlt::Position> allied_positions;

        /// Best pos pour un nouveau dropoff
        hlt::Position find_best_dropoff_position(
            const hlt::GameMap &game_map,
            const std::vector<hlt::Position> &existing_depots,
            int min_depot_distance) const;

        /// Verifie si une position est trop proche des depots existants
        bool is_too_close_to_depots(const hlt::Position &pos,
                                     const std::vector<hlt::Position> &depots,
                                     int min_distance, int w, int h) const;

        std::vector<std::vector<int>> halite_heatmap;

        /// Targets persistants
        std::map<hlt::EntityId, hlt::Position> persistent_targets;

        /// Calcule la heatmap
        void compute_heatmap(const hlt::GameMap &game_map);

        /// Simule extraction tour par tour
        MiningEstimate estimate_mining(int cell_halite, int ship_cargo, bool inspired) const;

        /// Best target explore via HPT = net / (aller + mine + retour)
        hlt::Position find_best_explore_target(const hlt::GameMap &game_map,
                                               const hlt::Position &ship_pos,
                                               hlt::EntityId ship_id,
                                               int ship_cargo,
                                               const std::vector<hlt::Position> &drop_positions) const;

        /// Score HPT d'une cell candidate pour l'exploration
        int score_explore_candidate(const hlt::GameMap &game_map,
                                     const hlt::Position &candidate,
                                     int dist, int ship_cargo, int avg_move_burn,
                                     const std::vector<hlt::Position> &drop_positions) const;
    };

} // namespace bot