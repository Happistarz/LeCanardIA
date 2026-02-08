#include "bot_player.hpp"
#include "blackboard.hpp"
#include "bot_parameters.hpp"
#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include <algorithm>
#include <vector>
#include <random>

namespace bot {

    BotPlayer::BotPlayer(hlt::Game& game_instance) : game(game_instance) { }

    // =========================================================
    // LA BOUCLE PRINCIPALE (Main Loop)
    // C'est maintenant très propre : on lit l'histoire tour par tour.
    // =========================================================
    std::vector<hlt::Command> BotPlayer::step() {
        std::vector<hlt::Command> command_queue;

        // 1. Le Cerveau observe la carte
        perform_analysis();

        // 2. Le Cerveau décide des naissances
        handle_spawn(command_queue);

        // 3. Le Cerveau distribue les rôles (Missions)
        assign_missions_to_ships();

        // 4. Les Muscles exécutent les mouvements (Commandes)
        execute_missions(command_queue);

        return command_queue;
    }

    // =========================================================
    // SOUS-FONCTIONS (Détails d'implémentation)
    // =========================================================

    void BotPlayer::perform_analysis() {
        Blackboard& blackboard = Blackboard::get_instance();
        blackboard.clear_turn_data();
        blackboard.update_metrics(game);
        blackboard.update_phase(game.turn_number, hlt::constants::MAX_TURNS);
        blackboard.update_best_cluster(game);
        // Note: L'analyse de menace est faite localement dans assign_missions pour l'instant,
        // mais pourrait être déplacée ici si on stockait 'under_threat' dans le Blackboard.
    }

    void BotPlayer::handle_spawn(std::vector<hlt::Command>& commands) {
        Blackboard& blackboard = Blackboard::get_instance();
        if (blackboard.should_spawn(*game.me)) {
            commands.push_back(game.me->shipyard->spawn());
            blackboard.reserve_position(game.me->shipyard->position, -1);
        }
    }

    void BotPlayer::assign_missions_to_ships() {
        Blackboard& blackboard = Blackboard::get_instance();

        // --- Analyse des menaces locale ---
        int dist_nearest_enemy = 999;
        hlt::Position enemy_base_target = {0,0};

        for (const auto& player : game.players) {
            if (player.id == game.me->id) continue;
            enemy_base_target = player.shipyard->position;

            for (const auto& ship_entry : player.ships) {
                int dist = game.game_map->calculate_distance(ship_entry.second->position, game.me->shipyard->position);
                if (dist < dist_nearest_enemy) dist_nearest_enemy = dist;
            }
        }

        // Utilisation du paramètre tunable
        bool under_threat = dist_nearest_enemy < params::DANGER_ZONE_RADIUS;

        // --- Boucle d'attribution ---
        for (auto& ship_iterator : game.me->ships) {
            determine_mission_for_single_ship(ship_iterator.second, under_threat);
        }
    }

    // C'est ici le cœur de la logique de décision
    void BotPlayer::determine_mission_for_single_ship(std::shared_ptr<hlt::Ship> ship, bool under_threat) {
        Blackboard& blackboard = Blackboard::get_instance();
        hlt::EntityId id = ship->id;

        // 1. Reset automatique du retour
        if (blackboard.ship_missions[id] == MissionType::RETURNING && ship->halite == 0) {
            blackboard.assign_mission(id, MissionType::MINING);
        }

        // 2. CONSTRUCTING (Base)
        int dist_to_base = game.game_map->calculate_distance(ship->position, game.me->shipyard->position);
        bool is_rich = game.game_map->at(ship->position)->halite > params::DROPOFF_SPOT_HALITE;
        bool has_money = game.me->halite >= params::DROPOFF_COST;
        bool enough_ships = game.me->ships.size() >= params::MIN_SHIPS_FOR_DROPOFF;

        if (has_money && enough_ships && dist_to_base > params::DROPOFF_MIN_DISTANCE && is_rich && blackboard.current_phase != GamePhase::ENDGAME) {
            blackboard.assign_mission(id, MissionType::CONSTRUCTING);
            return; // Mission trouvée, on arrête
        }

        // 3. RETURNING (Rentrée)
        if (ship->halite > hlt::constants::MAX_HALITE * params::RETURN_RATIO || blackboard.current_phase == GamePhase::ENDGAME) {
            blackboard.assign_mission(id, MissionType::RETURNING, game.me->shipyard->position);
            return;
        }

        // 4. DEFENDING (Défense)
        if (under_threat && dist_to_base < params::DEFENSE_INTERCEPT_DIST && ship->halite < params::DEFENSE_HALITE_LIMIT) {
            blackboard.assign_mission(id, MissionType::DEFENDING, game.me->shipyard->position);
            return;
        }

        // 5. SQUAD (Loot / Attaque)
        if (blackboard.current_phase == GamePhase::LATE && game.me->ships.size() > params::SQUAD_MIN_FLEET_SIZE) {
            // Logique existante conservée (Looter, Garde, Recrutement)
            if (blackboard.ship_missions[id] == MissionType::LOOTING) {
                blackboard.assign_mission(id, MissionType::LOOTING, blackboard.target_loot_zone);
                return;
            }
            else if (blackboard.squad_links.count(id)) {
                // Je suis garde du corps
                hlt::EntityId protected_id = blackboard.squad_links[id];
                if (game.me->ships.count(protected_id)) {
                    blackboard.assign_mission(id, MissionType::ATTACKING, game.me->ships[protected_id]->position);
                } else {
                    blackboard.squad_links.erase(id); // Mon protégé est mort
                    blackboard.assign_mission(id, MissionType::RETURNING);
                }
                return;
            }
            else if (blackboard.ship_missions[id] == MissionType::MINING) {
                // Tentative de création d'escouade
                int dist_cluster = game.game_map->calculate_distance(game.me->shipyard->position, blackboard.best_cluster_position);
                if (dist_cluster > params::LOOT_DIST_THRESHOLD) {
                    blackboard.target_loot_zone = blackboard.best_cluster_position;
                    blackboard.assign_mission(id, MissionType::LOOTING, blackboard.target_loot_zone);

                    // Recrutement du copain
                    hlt::EntityId buddy_id = -1;
                    int min_d = 999;
                    for (auto& other : game.me->ships) {
                        if (other.first == id) continue;
                        if (blackboard.ship_missions[other.first] != MissionType::MINING) continue;
                        int d = game.game_map->calculate_distance(ship->position, other.second->position);
                        if (d < min_d && d < params::BUDDY_MAX_DIST) {
                            min_d = d;
                            buddy_id = other.first;
                        }
                    }
                    if (buddy_id != -1) {
                        blackboard.squad_links[buddy_id] = id;
                        blackboard.assign_mission(buddy_id, MissionType::ATTACKING, ship->position);
                    }
                    return;
                }
            }
        }

        // 6. DEFAULT (Mining)
        if (blackboard.ship_missions.find(id) == blackboard.ship_missions.end() ||
            blackboard.ship_missions[id] == MissionType::DEFENDING) { // Si la menace est finie
            blackboard.assign_mission(id, MissionType::MINING, blackboard.best_cluster_position);
        }
    }

    void BotPlayer::execute_missions(std::vector<hlt::Command>& commands) {
        Blackboard& blackboard = Blackboard::get_instance();
        static std::mt19937 rng(game.turn_number + 12345);

        for (const auto& ship_iterator : game.me->ships) {
            auto ship = ship_iterator.second;
            MissionType mission = blackboard.ship_missions[ship->id];

            // CONSTRUCTION
            if (mission == MissionType::CONSTRUCTING) {
                commands.push_back(ship->make_dropoff());
                game.me->halite -= params::DROPOFF_COST;
                continue;
            }

            // MICRO (TEMPORAIRE - A REMPLACER)
            // FSM

            bool moved = false;
            std::vector<hlt::Direction> directions(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
            std::shuffle(directions.begin(), directions.end(), rng);

            for (const auto& direction : directions) {
                hlt::Position target_pos = ship->position;
                // Calcul manuel des coordonnées toriques
                if (direction == hlt::Direction::NORTH) target_pos.y = (ship->position.y - 1 + game.game_map->height) % game.game_map->height;
                if (direction == hlt::Direction::SOUTH) target_pos.y = (ship->position.y + 1) % game.game_map->height;
                if (direction == hlt::Direction::EAST) target_pos.x = (ship->position.x + 1) % game.game_map->width;
                if (direction == hlt::Direction::WEST) target_pos.x = (ship->position.x - 1 + game.game_map->width) % game.game_map->width;

                if (blackboard.is_position_safe(target_pos)) {
                    blackboard.reserve_position(target_pos, ship->id);
                    commands.push_back(ship->move(direction));
                    moved = true;
                    break;
                }
            }
            if (!moved) {
                blackboard.reserve_position(ship->position, ship->id);
                commands.push_back(ship->stay_still());
            }
        }
    }
}