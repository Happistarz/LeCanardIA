#pragma once

#include "utils.hpp"
#include "hlt/position.hpp"
#include "hlt/game.hpp"

#include <set>
#include <map>
#include <vector>

namespace bot
{

    // Stockage centralise pour la couche strategique
    struct Blackboard
    {
        // Singleton
        static Blackboard &get_instance()
        {
            static Blackboard instance;
            return instance;
        }

        Blackboard(Blackboard const &) = delete;
        Blackboard(Blackboard &&) = delete;

        // Dimensions de la carte

        int map_width;
        int map_height;

        // Initialisation a la premiere lecture de la carte
        void init(int width, int height);

        // Metriques par tour

        long total_halite;
        int average_halite;
        int current_turn;
        int max_turns;
        int total_ships_alive;
        GamePhase current_phase;

        // Met a jour total_halite / average_halite en scannant la carte
        void update_metrics(hlt::Game &game);

        // Determine la phase de jeu actuelle
        void update_phase(int turn, int total_turns);

        // Analyse la carte et produit les clusters exploitables (hors territoire ennemi)
        void update_clusters(hlt::Game &game);

        // Assigne une cible unique a chaque ship MINING pour repartir la flotte
        void assign_ship_targets(hlt::Game &game);

        // Met a jour les positions des structures ennemies
        void update_enemy_structures(hlt::Game &game);

        // Verifie si creer un vaisseau est rentable ce tour
        bool should_spawn(const hlt::Player &me);

        // Cible cluster

        hlt::Position best_cluster_position;

        // Top N clusters tries par score
        std::vector<hlt::Position> cluster_targets;

        // Cible individuelle par ship
        std::map<hlt::EntityId, hlt::Position> ship_explore_targets;

        // Positions des structures ennemies (shipyard + dropoffs)
        std::vector<hlt::Position> enemy_structures;

        // Distance en dessous de laquelle une cell est consideree en zone ennemie
        static constexpr int ENEMY_ZONE_RADIUS = 8;

        // Nombre max de clusters a retenir
        static constexpr int MAX_CLUSTERS = 8;

        // Verifie si une position est en territoire ennemi
        bool is_enemy_territory(const hlt::Position &pos, hlt::GameMap &game_map) const;

        // Etat des missions des vaisseaux

        std::map<hlt::EntityId, MissionType> ship_missions;
        std::map<hlt::EntityId, hlt::Position> ship_targets;

        // Assigne une mission + cible optionnelle a un vaisseau
        void assign_mission(hlt::EntityId id, MissionType mission, hlt::Position target = {0, 0});

        // Systeme d'escouade

        // Lien ship_id -> squad_id pour les ships en escouade
        std::map<hlt::EntityId, hlt::EntityId> squad_links;

        // Zone de pillage actuelle (territoire ennemi)
        hlt::Position target_loot_zone = {0, 0};

        // Suivi spatial par tour

        std::set<hlt::Position, PositionComparator> reserved_positions;
        std::map<hlt::Position, hlt::EntityId, PositionComparator> targeted_cells;
        std::set<hlt::Position> danger_zones;
        std::set<hlt::Position> stuck_positions;

        bool is_position_safe(const hlt::Position &pos) const;
        bool is_position_reserved(const hlt::Position &pos) const;
        bool is_position_stuck(const hlt::Position &pos) const;
        void reserve_position(const hlt::Position &pos, hlt::EntityId ship_id);

        // Reinitialise les donnees du tour
        void clear_turn_data();

    private:
        Blackboard()
            : map_width(0), map_height(0),
              total_halite(0), average_halite(0),
              current_turn(0), max_turns(0),
              total_ships_alive(0), current_phase(GamePhase::EARLY) {}
    };

} // namespace bot