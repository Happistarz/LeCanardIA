#pragma once

#include "hlt/game.hpp"
#include "hlt/command.hpp"
#include <vector>

namespace bot {

    class BotPlayer {
    public:
        BotPlayer(hlt::Game& game_instance);
        std::vector<hlt::Command> step();

    private:
        hlt::Game &game;

        // 1. Analyse : Met à jour le blackboard et détecte les menaces
        void perform_analysis();

        // 2. Stratégie : Décide si on spawn un nouveau vaisseau
        void handle_spawn(std::vector<hlt::Command>& commands);

        // 3. Dispatcher : Boucle sur tous les vaisseaux pour donner les missions
        void assign_missions_to_ships();

        // 4. Exécution : Transforme les missions en commandes de mouvement
        void execute_missions(std::vector<hlt::Command>& commands);

        // Sous-fonction pour décider de la mission d'un seul vaisseau
        void determine_mission_for_single_ship(std::shared_ptr<hlt::Ship> ship, bool under_threat);
    };
} // namespace bot