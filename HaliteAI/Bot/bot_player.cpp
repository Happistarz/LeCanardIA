#include "bot_player.hpp"
#include "blackboard.hpp"
#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"

#include <algorithm>
#include <vector>
#include <random>

namespace bot {

    BotPlayer::BotPlayer(hlt::Game& game_instance) : game(game_instance) {

    }

    std::vector<hlt::Command> BotPlayer::step() {
        std::vector<hlt::Command> command_queue;

        // 1. Récupération du Cerveau Central (Blackboard)
        Blackboard& blackboard = Blackboard::get_instance();

        // 2. Nettoyage des données du tour précédent
        blackboard.clear_turn_data();

        // -------------------------------------------------------------------------
        // PARTIE 1 : MACRO
        // Décision de créer un nouveau vaisseau
        // -------------------------------------------------------------------------

        // On spawn si : on est en début de partie (<= 200 tours), on a les sous, et la base est libre.
        if (game.turn_number <= 200 &&
            game.me->halite >= hlt::constants::SHIP_COST &&
            !blackboard.is_position_reserved(game.me->shipyard->position))
        {
            command_queue.push_back(game.me->shipyard->spawn());

            // On verrouille la base pour qu'aucun vaisseau ne vienne s'écraser sur le nouveau né
            blackboard.reserve_position(game.me->shipyard->position, -1);
        }

        // -------------------------------------------------------------------------
        // PARTIE 2 : MICRO (Temporaire pour test)
        // Déplacement des vaisseaux existants
        // -------------------------------------------------------------------------

        static std::mt19937 rng(std::random_device{}());

        for (const auto& ship_iterator : game.me->ships) {
            auto ship = ship_iterator.second;
            bool moved = false;

            // Initialisation correcte vecteur depuis l'array
            std::vector<hlt::Direction> directions(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
            std::shuffle(directions.begin(), directions.end(), rng);

            for (const auto& direction : directions) {
                // Calcul manuel de la position cible (Target)
                // Comme game_map->at(pos, dir) n'existe pas, on calcule les coordonnees a la main
                hlt::Position target_pos = ship->position;

                if (direction == hlt::Direction::NORTH) {
                    target_pos.y = (ship->position.y - 1 + game.game_map->height) % game.game_map->height;
                } else if (direction == hlt::Direction::SOUTH) {
                    target_pos.y = (ship->position.y + 1) % game.game_map->height;
                } else if (direction == hlt::Direction::EAST) {
                    target_pos.x = (ship->position.x + 1) % game.game_map->width;
                } else if (direction == hlt::Direction::WEST) {
                    target_pos.x = (ship->position.x - 1 + game.game_map->width) % game.game_map->width;
                }

                // Vérifie si cette cible est safe
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