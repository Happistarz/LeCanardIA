#include "bot_player.hpp"
#include "blackboard.hpp"
#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include <algorithm>
#include <vector>
#include <random>

namespace bot {

    BotPlayer::BotPlayer(hlt::Game& game_instance) : game(game_instance) { }

    std::vector<hlt::Command> BotPlayer::step() {
        std::vector<hlt::Command> command_queue;
        Blackboard& blackboard = Blackboard::get_instance();

        // =========================================================
        // 1. ANALYSE MACRO
        // =========================================================
        blackboard.clear_turn_data();
        blackboard.update_metrics(game);
        blackboard.update_phase(game.turn_number, hlt::constants::MAX_TURNS);
        blackboard.update_best_cluster(game);

        // Analyse des menaces (Pour la défense)
        // On cherche l'ennemi le plus proche de notre chantier naval
        int dist_nearest_enemy = 999;
        hlt::Position enemy_base_target = {0,0};
        bool enemy_base_found = false;

        for (const auto& player : game.players) {
            if (player.id == game.me->id) continue; // C'est nous

            // 1. Où est sa base ? (Pour l'attaque)
            enemy_base_target = player.shipyard->position;
            enemy_base_found = true;

            // 2. Où sont ses vaisseaux ? (Pour la défense)
            for (const auto& ship_entry : player.ships) {
                int dist = game.game_map->calculate_distance(ship_entry.second->position, game.me->shipyard->position);
                if (dist < dist_nearest_enemy) {
                    dist_nearest_enemy = dist;
                }
            }
        }

        // Est-ce qu'on est menacé ? (Ennemi à moins de 10 cases de la base)
        bool under_threat = dist_nearest_enemy < 10;


        // =========================================================
        // 2. DISPATCHER DE MISSIONS
        // =========================================================

        // On spawn si nécessaire (inchangé)
        if (blackboard.should_spawn(*game.me)) {
            command_queue.push_back(game.me->shipyard->spawn());
            blackboard.reserve_position(game.me->shipyard->position, -1);
        }

        // On passe en revue CHAQUE vaisseau pour lui donner un rôle
        for (auto& ship_iterator : game.me->ships) {
            auto ship = ship_iterator.second;
            hlt::EntityId id = ship->id;

            // --- A. MISE A JOUR ETAT (Reset automatique) ---
            // Si un vaisseau rentrait (RETURNING) et qu'il est vide (0 halite), il a fini !
            // Il redevient chômeur (MINING par défaut) pour recevoir un nouvel ordre.
            if (blackboard.ship_missions[id] == MissionType::RETURNING && ship->halite == 0) {
                blackboard.assign_mission(id, MissionType::MINING);
            }

            // --- B. LOGIQUE PRIORITAIRE (Les urgences d'abord) ---

            // 1. EST-CE QU'IL DOIT DEVENIR UNE BASE ? (CONSTRUCTING)
            // Si c'est validé, on change la mission.
            int dist_to_base = game.game_map->calculate_distance(ship->position, game.me->shipyard->position);
            bool is_rich_spot = game.game_map->at(ship->position)->halite > 700; //
            bool rich_enough = game.me->halite >= hlt::constants::DROPOFF_COST;    //

            // On ne construit pas si on est déjà en mode fin de partie ou défense critique
            if (rich_enough && dist_to_base > 15 && is_rich_spot && blackboard.current_phase != GamePhase::ENDGAME) {
                blackboard.assign_mission(id, MissionType::CONSTRUCTING);
                // Pas de 'continue' ici, on laisse le dispatcher finir d'analyser
            }

                // 2. EST-CE QU'IL DOIT RENTRER ? (RETURNING)
                // S'il est plein à 90% OU si c'est la fin (Endgame)
            else if (ship->halite > hlt::constants::MAX_HALITE * 0.9 || blackboard.current_phase == GamePhase::ENDGAME) {
                blackboard.assign_mission(id, MissionType::RETURNING, game.me->shipyard->position);
            }

                // 3. EST-CE QU'IL DOIT DÉFENDRE ? (DEFENDING)
                // Si la base est menacée et que le vaisseau est proche, il lâche tout pour défendre.
            else if (under_threat && dist_to_base < 15) {
                blackboard.assign_mission(id, MissionType::DEFENDING, game.me->shipyard->position);
            }

                // 4. EST-CE QU'IL DOIT ATTAQUER ? (ATTACKING)
                // En late game, si on n'a rien de mieux à faire (pas plein), on attaque.
            else if (blackboard.current_phase == GamePhase::LATE && enemy_base_found) {
                blackboard.assign_mission(id, MissionType::ATTACKING, enemy_base_target);
            }

                // 5. PAR DÉFAUT : MINEUR (MINING)
                // Si aucune condition n'est remplie, il va chercher l'argent.
            else {
                // Si on n'avait pas de mission ou qu'on était défenseur mais que la menace est finie
                if (blackboard.ship_missions.find(id) == blackboard.ship_missions.end() ||
                    blackboard.ship_missions[id] == MissionType::DEFENDING) {

                    blackboard.assign_mission(id, MissionType::MINING, blackboard.best_cluster_position);
                }
            }
        }


        // =========================================================
        // 3. EXÉCUTION
        // =========================================================
        static std::mt19937 rng(game.turn_number + 12345);

        for (const auto& ship_iterator : game.me->ships) {
            auto ship = ship_iterator.second;

            // On récupère les ordres du cerveau
            MissionType mission = blackboard.ship_missions[ship->id];
            hlt::Position target = blackboard.ship_targets[ship->id];

            // --- CAS SPECIAL : CONSTRUCTION ---
            if (mission == MissionType::CONSTRUCTING) {
                command_queue.push_back(ship->make_dropoff());
                game.me->halite -= hlt::constants::DROPOFF_COST;
                continue; // Action terminale pour ce tour
            }

            // --- INTERFACE VERS LA MICRO (A CHANGER) ---
            // Remplacer le bloc ci-dessous par la State Machine (FSM).

            // [TEMPORAIRE : Simulation basique en attendant Mathieu le fou de micro]
            bool moved = false;

            // Petite logique temporaire pour montrer que ça marche :
            // Si on attaque ou défend, on bouge vers la cible, sinon random intelligent
            if (mission == MissionType::ATTACKING || mission == MissionType::DEFENDING || mission == MissionType::RETURNING) {
                // Pour l'instant on laisse le random faire
            }

            // Mouvement Aléatoire Intelligent (Anti-Collision)
            std::vector<hlt::Direction> directions(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
            std::shuffle(directions.begin(), directions.end(), rng);

            for (const auto& direction : directions) {
                hlt::Position target_pos = ship->position;
                if (direction == hlt::Direction::NORTH) target_pos.y = (ship->position.y - 1 + game.game_map->height) % game.game_map->height;
                if (direction == hlt::Direction::SOUTH) target_pos.y = (ship->position.y + 1) % game.game_map->height;
                if (direction == hlt::Direction::EAST) target_pos.x = (ship->position.x + 1) % game.game_map->width;
                if (direction == hlt::Direction::WEST) target_pos.x = (ship->position.x - 1 + game.game_map->width) % game.game_map->width;

                if (blackboard.is_position_safe(target_pos)) {
                    blackboard.reserve_position(target_pos, ship->id);
                    command_queue.push_back(ship->move(direction));
                    moved = true;
                    break;
                }
            }
            if (!moved) {
                blackboard.reserve_position(ship->position, ship->id);
                command_queue.push_back(ship->stay_still());
            }
        }

        return command_queue;
    }
}