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
        Blackboard() { }

    public:

        std::set<hlt::Position> reserved_positions;             // Cases occupées en ce moment
        std::map<hlt::Position, hlt::EntityId> targeted_cells;  // Cases "destination" d'un bateau

        std::set<hlt::Position> danger_zones;   // Cases de position dangereuse
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