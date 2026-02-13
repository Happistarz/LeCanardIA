#pragma once

#include <set>
#include <map>
#include "hlt/position.hpp"

namespace bot
{

    enum class GamePhase
    {
        EARLY,   // 0-25% des tours
        MID,     // 25-60% des tours
        LATE,    // 60-85% des tours
        ENDGAME  // 85-100% des tours
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

        bool is_position_stuck(const hlt::Position &pos) const; // Case bloquée par un ship stuck ?
        GamePhase current_phase;                                // Phase actuelle
        int average_halite;                                     // Halite moyen par case
        int total_ships_alive;                                  // Nombre de bateaux

        bool should_spawn; // Faut-il spawn ce tour ?

        bool is_position_safe(const hlt::Position &pos) const;                  // Case safe ?
        bool is_position_reserved(const hlt::Position &pos) const;              // Case occupée ?
        void reserve_position(const hlt::Position &pos, hlt::EntityId ship_id); // Reserver une case
        void clear_turn_data();                                                 // Reset des données temporaires
    };

} // namespace bot