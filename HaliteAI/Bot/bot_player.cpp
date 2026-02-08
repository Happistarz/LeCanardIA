#include "bot_player.hpp"
#include "blackboard.hpp"
#include "bot_parameters.hpp"
#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include <algorithm>
#include <vector>
#include <random>

namespace bot
{

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

        // Détection des vaisseaux coincés (pas assez de halite pour bouger)
        for (const auto& ship_entry : game.me->ships) {
            auto ship = ship_entry.second;
            int cell_halite = game.game_map->at(ship->position)->halite;
            int move_cost = cell_halite / hlt::constants::MOVE_COST_RATIO;
            if (ship->halite < move_cost) {
                blackboard.stuck_positions.insert(ship->position);
            }
        }
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
            if (player->id == game.me->id) continue;
            enemy_base_target = player->shipyard->position;

            for (const auto& ship_entry : player->ships) {
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

        // 1. Synchroniser les FSM (créer pour nouveaux vaisseaux, supprimer les morts)
        sync_ship_fsms();

        // 2. Préparer le contexte pour le TrafficManager
        std::vector<hlt::Position> dropoff_positions = get_dropoff_positions();
        int turns_remaining = hlt::constants::MAX_TURNS - game.turn_number;

        TrafficManager::instance().init(*game.game_map, dropoff_positions,
                                        game.me->ships, turns_remaining);

        // 3. Collecter les MoveRequests de chaque vaisseau
        std::vector<MoveRequest> all_requests;

        for (const auto& ship_iterator : game.me->ships) {
            auto ship = ship_iterator.second;
            MissionType mission = blackboard.ship_missions[ship->id];

            // CONSTRUCTING : commande directe, pas de MoveRequest
            if (mission == MissionType::CONSTRUCTING) {
                commands.push_back(ship->make_dropoff());
                game.me->halite -= params::DROPOFF_COST;
                continue;
            }

            hlt::Position nearest_dropoff = find_nearest_dropoff(ship);
            MoveRequest request;

            switch (mission) {
                case MissionType::RETURNING: {
                    // En ENDGAME, priorité urgente
                    int priority = (blackboard.current_phase == GamePhase::ENDGAME)
                        ? MoveRequest::URGENT_RETURN_PRIORITY
                        : MoveRequest::RETURN_PRIORITY;
                    request = make_navigate_request(ship, nearest_dropoff, priority);
                    break;
                }

                case MissionType::DEFENDING:
                    request = make_navigate_request(ship, blackboard.ship_targets[ship->id],
                                                    MoveRequest::FLEE_PRIORITY);
                    break;

                case MissionType::ATTACKING:
                    request = make_navigate_request(ship, blackboard.ship_targets[ship->id],
                                                    MoveRequest::EXPLORE_PRIORITY);
                    break;

                case MissionType::LOOTING:
                    request = make_navigate_request(ship, blackboard.ship_targets[ship->id],
                                                    MoveRequest::EXPLORE_PRIORITY);
                    break;

                case MissionType::MINING:
                default:
                    // MINING : la FSM gère le cycle explore → collect → return
                    if (m_ship_fsms.count(ship->id)) {
                        request = m_ship_fsms[ship->id]->update(
                            ship, *game.game_map, nearest_dropoff, turns_remaining);
                    } else {
                        // Fallback si pas de FSM (ne devrait pas arriver)
                        request = make_navigate_request(ship, blackboard.best_cluster_position,
                                                        MoveRequest::EXPLORE_PRIORITY);
                    }
                    break;
            }

            all_requests.push_back(request);
        }

        // 4. Résolution de conflits via TrafficManager
        std::vector<MoveResult> results = TrafficManager::instance().resolve_all(all_requests);

        // 5. Convertir les MoveResults en commandes
        for (const auto& result : results) {
            if (game.me->ships.count(result.m_ship_id)) {
                auto ship = game.me->ships[result.m_ship_id];
                if (result.m_final_direction == hlt::Direction::STILL) {
                    commands.push_back(ship->stay_still());
                } else {
                    commands.push_back(ship->move(result.m_final_direction));
                }
            }
        }
    }

    // =========================================================
    // HELPERS D'INTÉGRATION MACRO / MICRO
    // =========================================================

    void BotPlayer::sync_ship_fsms() {
        // Supprimer les FSM des vaisseaux morts
        for (auto it = m_ship_fsms.begin(); it != m_ship_fsms.end(); ) {
            if (game.me->ships.find(it->first) == game.me->ships.end()) {
                it = m_ship_fsms.erase(it);
            } else {
                ++it;
            }
        }

        // Créer des FSM pour les nouveaux vaisseaux
        for (const auto& ship_entry : game.me->ships) {
            if (m_ship_fsms.find(ship_entry.first) == m_ship_fsms.end()) {
                m_ship_fsms[ship_entry.first] = std::make_unique<ShipFSM>(ship_entry.first);
            }
        }
    }

    MoveRequest BotPlayer::make_navigate_request(std::shared_ptr<hlt::Ship> ship,
                                                  const hlt::Position& target, int priority) {
        const Blackboard& bb = Blackboard::get_instance();

        // Déjà à destination → rester sur place
        if (ship->position == target) {
            std::vector<hlt::Direction> alternatives(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
            return MoveRequest{ship->id, ship->position, ship->position,
                               hlt::Direction::STILL, priority, alternatives};
        }

        // Directions optimales vers la destination (unsafe = sans anti-collision)
        std::vector<hlt::Direction> unsafe_moves = game.game_map->get_unsafe_moves(ship->position, target);

        struct ScoredDir {
            hlt::Direction dir;
            int distance;
            bool is_stuck;
            bool is_optimal;
        };

        std::vector<ScoredDir> scored;
        for (const auto& dir : hlt::ALL_CARDINALS) {
            hlt::Position target_pos = game.game_map->normalize(ship->position.directional_offset(dir));
            int dist = game.game_map->calculate_distance(target_pos, target);
            bool stuck = bb.is_position_stuck(target_pos);
            bool optimal = false;

            for (const auto& um : unsafe_moves) {
                if (um == dir) { optimal = true; break; }
            }

            scored.push_back({dir, dist, stuck, optimal});
        }

        // Tri : non-stuck > optimal > distance la plus courte
        std::sort(scored.begin(), scored.end(),
                  [](const ScoredDir& a, const ScoredDir& b) {
                      if (a.is_stuck != b.is_stuck) return !a.is_stuck;
                      if (a.is_optimal != b.is_optimal) return a.is_optimal;
                      return a.distance < b.distance;
                  });

        hlt::Direction best_dir = scored[0].dir;
        std::vector<hlt::Direction> alternatives;
        for (size_t i = 1; i < scored.size(); ++i)
            alternatives.push_back(scored[i].dir);

        hlt::Position desired = game.game_map->normalize(ship->position.directional_offset(best_dir));
        return MoveRequest{ship->id, ship->position, desired, best_dir, priority, alternatives};
    }

    std::vector<hlt::Position> BotPlayer::get_dropoff_positions() {
        std::vector<hlt::Position> positions;
        positions.push_back(game.me->shipyard->position);
        for (const auto& dp : game.me->dropoffs) {
            positions.push_back(dp.second->position);
        }
        return positions;
    }

    hlt::Position BotPlayer::find_nearest_dropoff(std::shared_ptr<hlt::Ship> ship) {
        hlt::Position nearest = game.me->shipyard->position;
        int min_dist = game.game_map->calculate_distance(ship->position, nearest);

        for (const auto& dp : game.me->dropoffs) {
            int d = game.game_map->calculate_distance(ship->position, dp.second->position);
            if (d < min_dist) {
                min_dist = d;
                nearest = dp.second->position;
            }
        }
        return nearest;
    }

} // namespace bot