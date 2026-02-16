#pragma once

#include "hlt/types.hpp"
#include "hlt/game.hpp"
#include "hlt/command.hpp"
#include "ship_fsm.hpp"
#include "traffic_manager.hpp"
#include "blackboard.hpp"

#include <vector>
#include <memory>
#include <unordered_map>

namespace bot
{
    class BotPlayer
    {
    private:
        hlt::Game &game;
        std::unordered_map<hlt::EntityId, std::unique_ptr<ShipFSM>> ship_fsms;
        hlt::EntityId m_converting_ship_id = -1; // Ship en cours de conversion en dropoff

        /// Update le blackboard avec les donnees du turn
        void update_blackboard();
        // Update les stats de la map
        void update_map_stats(Blackboard& bb, std::unique_ptr<hlt::GameMap>& game_map);
        // Update la phase de jeu
        void update_game_phase(Blackboard& bb);
        // Update les ships bloqués
        void update_stuck_ships(Blackboard& bb, std::unique_ptr<hlt::GameMap>& game_map, std::shared_ptr<hlt::Player> me);
        // Update les infos sur les ennemis
        void update_enemy_info(Blackboard& bb, std::unique_ptr<hlt::GameMap>& game_map);
        // Update les cibles persistantes
        void update_persistent_targets(Blackboard& bb);
        // Update l'historique de positions
        void update_position_history(Blackboard& bb, std::unique_ptr<hlt::GameMap>& game_map, std::shared_ptr<hlt::Player> me);

        /// Supprime les FSM des ships morts
        void cleanup_dead_ships();

        /// Recupere les positions de tous les points de drop
        std::vector<hlt::Position> get_drops_positions() const;

        /// Collecte les MoveRequests de tous les ships via leurs FSM
        std::vector<MoveRequest> collect_move_requests();
        // Logique de skip de ship
        bool should_skip_ship(const hlt::Ship& ship) const;
        // Est-ce un dropoff ship ?
        bool is_dropoff_ship(const hlt::Ship& ship, const Blackboard& bb) const;
        // Navigation spéciale dropoff
        MoveRequest handle_dropoff_ship(std::shared_ptr<hlt::Ship> ship,
            hlt::GameMap& map,
            const Blackboard& bb);
        // Ship normal (FSM)
        MoveRequest handle_normal_ship(std::shared_ptr<hlt::Ship> ship,
            hlt::GameMap& map,
            int turns_remaining);

        /// Retourne la position du drop le plus proche de la position donnee
        hlt::Position closest_drop(const hlt::Position &pos) const;

        /// Tente de construire un dropoff si les conditions sont reunies
        bool try_build_dropoff(std::vector<hlt::Command> &commands);
        // Conditions globales
        bool can_build_dropoff(const Blackboard&, const hlt::Player&, const hlt::GameMap&) const;
        // Reset si ship mort
        void reset_dead_dropoff_plan(Blackboard&, const hlt::Player&);
        // Plan actif ?
        bool has_active_dropoff_plan(const Blackboard&) const;
        // Exécuter le plan (navigation + conversion)
        bool execute_dropoff_plan(std::vector<hlt::Command>&, Blackboard&, hlt::Player&, hlt::GameMap&);
        // Créer un nouveau plan
        bool create_new_dropoff_plan(Blackboard&, hlt::Player&, hlt::GameMap&);
        std::shared_ptr<hlt::Ship> find_best_dropoff_ship(hlt::Player&, hlt::GameMap&, const hlt::Position&);

        /// Determine si on doit spawn un nouveau ship
        bool should_spawn(const std::vector<MoveRequest> &requests,
                          const std::vector<MoveResult> &results) const;
        // Phase + tours restants
        bool can_spawn_phase(const Blackboard&, int turns_remaining) const;
        // Halite disponible (spawn + dropoff reserve)
        bool has_spawn_halite(const Blackboard&, const hlt::Player&) const;
        // Map assez riche ?
        bool map_is_rich_enough(const Blackboard&) const;
        // Limite de ships
        bool below_max_ships(const Blackboard&, const hlt::GameMap&) const;
        // Collision sur le shipyard
        bool shipyard_will_be_occupied(const hlt::Player&, std::unique_ptr<hlt::GameMap>& game_map,
            const std::vector<MoveRequest>&,
            const std::vector<MoveResult>&) const;

    public:
        BotPlayer(hlt::Game &game_instance);

        /// Joue le tour du jeu, retourne la liste des commandes a executer
        std::vector<hlt::Command> play_turn();
    };
} // namespace bot