#pragma once

#include <set>
#include <map>
#include "hlt/position.hpp"

namespace bot {

    enum class GamePhase {
        EARLY,      // Tours 1-100
        MID,        // Tours 100-300
        LATE,       // Tours 300-400
        ENDGAME     // Tours 400-500
    };

    // Nécessaire pour utiliser Position dans les sets/maps
    struct PositionComparator {
        bool operator()(const hlt::Position& a, const hlt::Position& b) const {
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        }
    };

    struct Blackboard {

        // Création du Singleton
        static Blackboard& get_instance(){
            static Blackboard instance;
            return instance;
        }
        Blackboard(Blackboard const&) = delete;
        Blackboard(Blackboard&&) = delete;

    private:

        // Constructeur privé
        Blackboard() : map_width(0), map_height(0), should_spawn(false) { }

    public:

        int map_width;                          // Largeur de la carte
        int map_height;                         // Hauteur de la carte

        // Initialisation des dimensions (à appeler au début)
        void init(int width, int height) {
            map_width = width;
            map_height = height;
        }

        // Structures de données avec le comparateur
        std::set<hlt::Position, PositionComparator> reserved_positions;             // Cases occupées en ce moment
        std::map<hlt::Position, hlt::EntityId, PositionComparator> targeted_cells;  // Cases "destination" d'un bateau
        std::set<hlt::Position, PositionComparator> danger_zones;                   // Cases de position dangereuse

        GamePhase current_phase;                // Phase actuelle
        int average_halite;                     // Halite moyen par case
        int total_ships_alive;                  // Nombre de bateaux

        bool should_spawn;                      // Faut-il spawn ce tour ?

        bool is_position_safe(const hlt::Position& pos) const;                      // Case safe ?
        bool is_position_reserved(const hlt::Position& pos) const;                  // Case occupée ?
        void reserve_position(const hlt::Position& pos, hlt::EntityId ship_id);     // Reserver une case
        void clear_turn_data();                                                     // Reset des données temporaires

    };

}