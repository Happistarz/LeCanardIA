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

        /// Supprime les FSM des ships morts
        void cleanup_dead_ships();

        /// Recupere les positions de tous les points de drop
        std::vector<hlt::Position> get_drops_positions() const;

        /// Collecte les MoveRequests de tous les ships via leurs FSM
        std::vector<MoveRequest> collect_move_requests();

        /// Retourne la position du drop le plus proche de la position donnee
        hlt::Position closest_drop(const hlt::Position &pos) const;

        /// Tente de construire un dropoff si les conditions sont reunies
        bool try_build_dropoff(std::vector<hlt::Command> &commands);

        /// Determine si on doit spawn un nouveau ship
        bool should_spawn(const std::vector<MoveRequest> &requests,
                          const std::vector<MoveResult> &results) const;

    public:
        BotPlayer(hlt::Game &game_instance);

        /// Joue le tour du jeu, retourne la liste des commandes a executer
        std::vector<hlt::Command> play_turn();
    };
} // namespace bot