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

        /// Met a jour le blackboard avec les donnees du tour (phase, halite moyen, stuck ships)
        void update_blackboard();

        /// Supprime les FSM des ships detruits
        void cleanup_dead_ships();

        /// Recupere les positions de tous les points de depot (shipyard + dropoffs)
        std::vector<hlt::Position> get_dropoff_positions() const;

        /// Collecte les MoveRequests de tous les ships via leurs FSM
        std::vector<MoveRequest> collect_move_requests();

        /// Determine si on doit spawn un nouveau ship ce tour
        bool should_spawn(const std::vector<MoveRequest> &requests,
                          const std::vector<MoveResult> &results) const;

    public:
        BotPlayer(hlt::Game &game_instance);

        /// Joue un tour complet et retourne les commandes a envoyer au moteur
        std::vector<hlt::Command> play_turn();
    };
} // namespace bot