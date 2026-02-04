#pragma once

#include <set>
#include <map>
#include "hlt/position.hpp"
#include "hlt/game.hpp"

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
        Blackboard() : map_width(0), map_height(0), total_halite(0), average_halite(0), current_turn(0), max_turns(0) { }

    public:

        int map_width;                          // Largeur de la carte
        int map_height;                         // Hauteur de la carte

        // Statistiques
        long total_halite;      // Quantité totale d'argent sur la map
        int average_halite;     // Richesse moyenne d'une case
        int current_turn;
        int max_turns;
        GamePhase current_phase;

        // Initialisation des dimensions (à appeler au début)
        void init(int width, int height) {
            map_width = width;
            map_height = height;
        }

        // Structures de données avec le comparateur
        std::set<hlt::Position, PositionComparator> reserved_positions;             // Cases occupées en ce moment
        std::map<hlt::Position, hlt::EntityId, PositionComparator> targeted_cells;  // Cases "destination" d'un bateau
        std::set<hlt::Position, PositionComparator> danger_zones;                   // Cases de position dangereuse

        // Fonctions d'analyse
        void update_metrics(hlt::Game& game);              // Scanne la carte
        void update_phase(int turn, int total_turns);      // Met à jour la phase (Early/Mid/Late)
        bool should_spawn(const hlt::Player& me);
        int total_ships_alive;                  // Nombre de bateaux

        // Fonctions de base
        bool is_position_safe(const hlt::Position& pos) const;                      // Case safe ?
        bool is_position_reserved(const hlt::Position& pos) const;                  // Case occupée ?
        void reserve_position(const hlt::Position& pos, hlt::EntityId ship_id);     // Reserver une case
        void clear_turn_data();                                                     // Reset des données temporaires

    };

}