#pragma once

#include "hlt/game.hpp"
#include "hlt/command.hpp"
#include "ship_fsm.hpp"
#include "traffic_manager.hpp"
#include <vector>
#include <unordered_map>
#include <memory>

namespace bot {

    class BotPlayer {
    public:
        BotPlayer(hlt::Game& game_instance);
        std::vector<hlt::Command> step();

    private:
        hlt::Game &game;

        // Gestion FSM par vaisseau (micro)
        std::unordered_map<hlt::EntityId, std::unique_ptr<ShipFSM>> m_ship_fsms;

        // 1. Analyse : Met à jour le blackboard et détecte les menaces
        void perform_analysis();

        // 2. Stratégie : Décide si on spawn un nouveau vaisseau
        void handle_spawn(std::vector<hlt::Command>& commands);

        // 3. Dispatcher : Boucle sur tous les vaisseaux pour donner les missions
        void assign_missions_to_ships();

        // 4. Exécution : Transforme les missions en commandes via FSM + TrafficManager
        void execute_missions(std::vector<hlt::Command>& commands);

        // Sous-fonction pour décider de la mission d'un seul vaisseau
        void determine_mission_for_single_ship(std::shared_ptr<hlt::Ship> ship, bool under_threat);

        // --- Helpers d'intégration macro/micro ---
        // Synchronise les FSM avec les vaisseaux vivants
        void sync_ship_fsms();

        // Génère un MoveRequest de navigation vers une cible (pour missions spéciales)
        MoveRequest make_navigate_request(std::shared_ptr<hlt::Ship> ship,
                                          const hlt::Position& target, int priority);

        // Retourne la liste des positions de dépôt (shipyard + dropoffs)
        std::vector<hlt::Position> get_dropoff_positions();

        // Trouve le dropoff le plus proche d'un vaisseau
        hlt::Position find_nearest_dropoff(std::shared_ptr<hlt::Ship> ship);
    };
} // namespace bot