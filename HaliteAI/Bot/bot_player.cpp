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

        // 1. Nettoyage & Analyse
        blackboard.clear_turn_data();
        blackboard.update_metrics(game);
        blackboard.update_phase(game.turn_number, hlt::constants::MAX_TURNS);
        blackboard.update_best_cluster(game); // Ton nouveau GPS

        // 2. MACRO : Spawn Intelligent
        if (blackboard.should_spawn(*game.me)) {
            command_queue.push_back(game.me->shipyard->spawn());
            blackboard.reserve_position(game.me->shipyard->position, -1);
        }

        // 3. BOUCLE DES SHIPS
        static std::mt19937 rng(game.turn_number + 12345); // Pour le test random

        for (const auto& ship_iterator : game.me->ships) {
            auto ship = ship_iterator.second;

            // ---------------------------------------------------------
            // PARTIE A : DECISION MACRO
            // ---------------------------------------------------------

            // On vérifie si on doit construire un Dropoff
            // Critères :
            // 1. On a les sous (4000 + coût vaisseau)
            // 2. On est loin de la base (> 15 cases)
            // 3. La case actuelle est riche (> 700 Halite, c'est un bon spot)
            // 4. On a assez de vaisseaux (> 10) pour se permettre d'en sacrifier un

            int dist_to_base = game.game_map->calculate_distance(ship->position, game.me->shipyard->position);
            bool is_rich_spot = game.game_map->at(ship->position)->halite > 700;
            bool rich_enough = game.me->halite >= hlt::constants::DROPOFF_COST;
            bool enough_ships = game.me->ships.size() > 10;

            if (rich_enough && enough_ships && dist_to_base > 15 && is_rich_spot) {

                // CONSTRUIRE !
                command_queue.push_back(ship->make_dropoff());

                // On met à jour le budget virtuel pour ne pas en faire 2 d'un coup
                game.me->halite -= hlt::constants::DROPOFF_COST;

                // On passe au vaisseau suivant immédiatement.
                continue;
            }

            // ---------------------------------------------------------
            // PARTIE B : MICRO / Code test à changer
            // ---------------------------------------------------------
            // Si la Macro n'a rien ordonné de spécial, le vaisseau vit sa vie.
            bool moved = false;

            // code de mouvement aléatoire

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