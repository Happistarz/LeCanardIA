#pragma once

#include "utils.hpp"
#include "ship_fsm.hpp"
#include "traffic_manager.hpp"
#include "hlt/game.hpp"
#include "hlt/command.hpp"

#include <vector>
#include <unordered_map>
#include <memory>

namespace bot
{
    class BotPlayer
    {
    public:
        explicit BotPlayer(hlt::Game &game_instance);

        // Entry point : retourne la liste de commandes du tour
        std::vector<hlt::Command> step();

    private:
        hlt::Game &m_game;

        // Instances FSM par ship
        std::unordered_map<hlt::EntityId, std::unique_ptr<ShipFSM>> m_ship_fsms;

        // Pipeline du tour
        void perform_analysis();
        void handle_spawn(std::vector<hlt::Command> &commands);
        void assign_missions_to_ships();
        void execute_missions(std::vector<hlt::Command> &commands);

        // Logique de mission
        void determine_mission(std::shared_ptr<hlt::Ship> ship, bool under_threat);

        // Utilitaires macro / micro

        // Synchronise les FSM avec les ships vivants
        void sync_ship_fsms();

        // Collecte toutes les positions de depots
        std::vector<hlt::Position> get_dropoff_positions() const;

        // Depot le plus proche d'un ship donne
        hlt::Position find_nearest_dropoff(std::shared_ptr<hlt::Ship> ship) const;

        // Distance du ship ennemi le plus proche
        int compute_nearest_enemy_distance() const;
    };

} // namespace bot