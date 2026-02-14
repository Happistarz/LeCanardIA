#include "bot_player.hpp"
#include "map_utils.hpp"
#include "bot_constants.hpp"
#include "hlt/log.hpp"
#include "hlt/constants.hpp"
#include <algorithm>

namespace bot
{

    BotPlayer::BotPlayer(hlt::Game &game_instance) : game(game_instance)
    {
    }

    // ── Blackboard ──────────────────────────────────────────────

    void BotPlayer::update_blackboard()
    {
        Blackboard &bb = Blackboard::get_instance();
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;

        bb.clear_turn_data();
        bb.total_ships_alive = static_cast<int>(me->ships.size());

        // Halite moyen sur la carte
        long long total_halite = 0;
        int cell_count = game_map->width * game_map->height;
        for (int y = 0; y < game_map->height; ++y)
        {
            for (int x = 0; x < game_map->width; ++x)
            {
                total_halite += game_map->cells[y][x].halite;
            }
        }
        bb.average_halite = (cell_count > 0) ? static_cast<int>(total_halite / cell_count) : 0;

        // Phase de jeu (basee sur le pourcentage de tours ecoules)
        float progress = static_cast<float>(game.turn_number) /
                         static_cast<float>(hlt::constants::MAX_TURNS);

        if (progress < 0.25f)
            bb.current_phase = GamePhase::EARLY;
        else if (progress < 0.60f)
            bb.current_phase = GamePhase::MID;
        else if (progress < 0.85f)
            bb.current_phase = GamePhase::LATE;
        else
            bb.current_phase = GamePhase::ENDGAME;

        // Ships bloques (pas assez de halite pour bouger)
        for (const auto &ship_pair : me->ships)
        {
            const auto &ship = ship_pair.second;
            int cell_halite = game_map->at(ship->position)->halite;
            int move_cost = cell_halite / hlt::constants::MOVE_COST_RATIO;
            if (ship->halite < move_cost)
            {
                bb.stuck_positions.insert(game_map->normalize(ship->position));
            }
        }

        // Calcul de la heatmap pour le clustering
        bb.compute_heatmap(*game_map);

        // Marquer les positions ennemies comme danger zones
        for (const auto &player : game.players)
        {
            if (player->id == game.my_id)
                continue;

            // Ships ennemis
            for (const auto &ship_pair : player->ships)
            {
                hlt::Position norm_pos = game_map->normalize(ship_pair.second->position);
                bb.danger_zones.insert(norm_pos);
                bb.enemy_ships.push_back({ship_pair.first, norm_pos, ship_pair.second->halite});
            }

            // Shipyard ennemi (risque de spawn-kill)
            bb.danger_zones.insert(game_map->normalize(player->shipyard->position));

            // Dropoffs ennemis (zone a forte concentration ennemie)
            for (const auto &dropoff_pair : player->dropoffs)
            {
                bb.danger_zones.insert(game_map->normalize(dropoff_pair.second->position));
            }
        }

        // Pre-peupler targeted_cells avec les persistent_targets
        // pour que find_best_explore_target les prenne en compte
        for (const auto &pt : bb.persistent_targets)
        {
            bb.targeted_cells[pt.second] = pt.first;
        }

        // Calculer les zones d'inspiration (bonus de minage x3 pres des ennemis)
        bb.compute_inspired_zones(game_map->width, game_map->height);

        // Mettre a jour l'historique de positions pour detecter les oscillations
        for (const auto &ship_pair : me->ships)
        {
            bb.update_position_history(ship_pair.first,
                                       game_map->normalize(ship_pair.second->position));
        }
    }

    // ── Nettoyage ───────────────────────────────────────────────

    void BotPlayer::cleanup_dead_ships()
    {
        Blackboard &bb = Blackboard::get_instance();
        const auto &alive_ships = game.me->ships;
        for (auto it = ship_fsms.begin(); it != ship_fsms.end();)
        {
            if (alive_ships.find(it->first) == alive_ships.end())
            {
                bb.persistent_targets.erase(it->first);
                bb.hunt_targets.erase(it->first);
                bb.position_history.erase(it->first);
                it = ship_fsms.erase(it);
            }
            else
                ++it;
        }
    }

    // ── Helpers ─────────────────────────────────────────────────

    std::vector<hlt::Position> BotPlayer::get_dropoff_positions() const
    {
        std::vector<hlt::Position> positions;
        positions.push_back(game.me->shipyard->position);
        for (const auto &dropoff_pair : game.me->dropoffs)
        {
            positions.push_back(dropoff_pair.second->position);
        }
        return positions;
    }
    hlt::Position BotPlayer::closest_depot(const hlt::Position &pos) const
    {
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;
        std::vector<hlt::Position> depots = get_dropoff_positions();

        hlt::Position best = depots[0];
        int best_dist = game_map->calculate_distance(pos, best);

        for (size_t i = 1; i < depots.size(); ++i)
        {
            int d = game_map->calculate_distance(pos, depots[i]);
            if (d < best_dist)
            {
                best_dist = d;
                best = depots[i];
            }
        }
        return best;
    }
    // ── MoveRequests ────────────────────────────────────────────

    std::vector<MoveRequest> BotPlayer::collect_move_requests()
    {
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;
        int turns_remaining = hlt::constants::MAX_TURNS - game.turn_number;

        std::vector<MoveRequest> requests;
        requests.reserve(me->ships.size());

        for (const auto &ship_pair : me->ships)
        {
            std::shared_ptr<hlt::Ship> ship = ship_pair.second;

            // Skip si ship est en cours de conversion en dropoff
            if (m_converting_ship_id >= 0 && ship->id == m_converting_ship_id)
                continue;

            // Skip si ship est assigne a la construction d'un dropoff et en route
            // (on ne veut pas que la FSM le redirige vers RETURN)
            const Blackboard &bb_ref = Blackboard::get_instance();
            if (bb_ref.dropoff_ship_id >= 0 && ship->id == bb_ref.dropoff_ship_id)
            {
                // Navigation manuelle vers le dropoff planifie (bypass FSM)
                hlt::Position target = bb_ref.planned_dropoff_pos;
                hlt::Direction best_dir;
                std::vector<hlt::Direction> alternatives;
                map_utils::navigate_toward(ship, *game_map, target,
                                           bb_ref.stuck_positions, bb_ref.danger_zones,
                                           best_dir, alternatives);
                hlt::Position desired = game_map->normalize(ship->position.directional_offset(best_dir));
                requests.push_back(MoveRequest{ship->id, ship->position, desired,
                                               best_dir, constants::RETURN_PRIORITY, alternatives});
                continue;
            }

            // Rechercher ou creer le ShipFSM (O(1) avec find)
            auto fsm_it = ship_fsms.find(ship->id);
            if (fsm_it == ship_fsms.end())
            {
                fsm_it = ship_fsms.emplace(
                    ship->id, std::unique_ptr<ShipFSM>(new ShipFSM(ship->id))).first;
            }

            // Depot le plus proche de ce ship
            hlt::Position depot = closest_depot(ship->position);

            requests.push_back(fsm_it->second->update(
                ship, *game_map, depot, turns_remaining));
        }

        return requests;
    }

    // ── Dropoff ───────────────────────────────────────────

    bool BotPlayer::try_build_dropoff(std::vector<hlt::Command> &commands)
    {
        Blackboard &bb = Blackboard::get_instance();
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;

        // Conditions de base
        int num_dropoffs = static_cast<int>(me->dropoffs.size());
        if (num_dropoffs >= constants::MAX_DROPOFFS)
            return false;
        if (bb.current_phase == GamePhase::LATE || bb.current_phase == GamePhase::ENDGAME)
        {
            // Annuler tout plan de dropoff en fin de partie
            bb.planned_dropoff_pos = hlt::Position(-1, -1);
            bb.dropoff_ship_id = -1;
            return false;
        }
        if (bb.total_ships_alive < constants::MIN_SHIPS_FOR_DROPOFF)
            return false;

        // Distance minimale adaptee a la taille de la map
        int min_depot_dist = std::max(8, game_map->width / constants::MIN_DROPOFF_DEPOT_DISTANCE_RATIO);

        // --- Si un ship assigne est deja mort, reset le plan ---
        if (bb.dropoff_ship_id >= 0 &&
            me->ships.find(bb.dropoff_ship_id) == me->ships.end())
        {
            hlt::log::log("Dropoff: ship assigne " + std::to_string(bb.dropoff_ship_id) + " mort, reset plan");
            bb.planned_dropoff_pos = hlt::Position(-1, -1);
            bb.dropoff_ship_id = -1;
        }

        // === CAS 1 : On a deja un plan en cours ===
        if (bb.planned_dropoff_pos.x >= 0 && bb.dropoff_ship_id >= 0)
        {
            auto ship_it = me->ships.find(bb.dropoff_ship_id);
            if (ship_it == me->ships.end())
                return false; // Ship mort (safety)

            auto ship = ship_it->second;
            hlt::Position target = bb.planned_dropoff_pos;

            // Le ship est arrive sur la position cible ?
            if (ship->position == target)
            {
                int real_cost = hlt::constants::DROPOFF_COST
                                - ship->halite
                                - game_map->at(target)->halite;
                if (real_cost < 0) real_cost = 0;

                if (me->halite >= real_cost)
                {
                    hlt::log::log("Dropoff: conversion du ship " + std::to_string(ship->id)
                                  + " a " + std::to_string(target.x) + "," + std::to_string(target.y)
                                  + " (cout reel: " + std::to_string(real_cost) + ")");
                    commands.push_back(ship->make_dropoff());
                    m_converting_ship_id = ship->id;
                    bb.planned_dropoff_pos = hlt::Position(-1, -1);
                    bb.dropoff_ship_id = -1;
                    return true;
                }
                // Pas assez de halite, on attend sur place
                return false;
            }

            // Pas encore arrive : s'assurer que le persistent_target est toujours correct
            bb.persistent_targets[ship->id] = target;
            return false;
        }

        // === CAS 2 : Pas de plan, en chercher un ===

        // Assez de halite pour que ca vaille le coup ? (seuil bas : on aura le temps d'accumuler)
        if (me->halite < hlt::constants::DROPOFF_COST / 2)
            return false;

        // Trouver la meilleure position
        std::vector<hlt::Position> depots = get_dropoff_positions();
        hlt::Position best_pos = bb.find_best_dropoff_position(*game_map, depots, min_depot_dist);
        if (best_pos.x < 0)
            return false;

        // Trouver le ship le plus proche de cette position (max 20 cases)
        std::shared_ptr<hlt::Ship> best_ship = nullptr;
        int best_dist = 9999;
        int max_assign_dist = game_map->width / 3; // ~21 sur 64x64

        for (const auto &ship_pair : me->ships)
        {
            const auto &ship = ship_pair.second;
            int d = game_map->calculate_distance(ship->position, best_pos);
            if (d < best_dist && d <= max_assign_dist)
            {
                best_dist = d;
                best_ship = ship;
            }
        }

        if (!best_ship)
            return false;

        // Assigner le plan
        bb.planned_dropoff_pos = best_pos;
        bb.dropoff_ship_id = best_ship->id;
        bb.persistent_targets[best_ship->id] = best_pos;

        hlt::log::log("Dropoff: plan cree - ship " + std::to_string(best_ship->id)
                      + " -> " + std::to_string(best_pos.x) + "," + std::to_string(best_pos.y)
                      + " (dist: " + std::to_string(best_dist) + ")");

        return false;
    }

    // ── Spawn ───────────────────────────────────────────────────

    bool BotPlayer::should_spawn(const std::vector<MoveRequest> &requests,
                                  const std::vector<MoveResult> &results) const
    {
        const Blackboard &bb = Blackboard::get_instance();
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;
        int turns_remaining = hlt::constants::MAX_TURNS - game.turn_number;

        // Plus de spawn en LATE / ENDGAME
        if (bb.current_phase == GamePhase::LATE ||
            bb.current_phase == GamePhase::ENDGAME)
            return false;

        // Un ship a besoin de ~80 tours pour faire aller-retour + miner + ROI
        if (turns_remaining < constants::SPAWN_MIN_TURNS_LEFT)
            return false;

        // Pas assez de halite en banque
        if (me->halite < hlt::constants::SHIP_COST)
            return false;

        // Reserver du halite pour un eventuel dropoff
        if (bb.planned_dropoff_pos.x >= 0 && me->halite < hlt::constants::SHIP_COST + hlt::constants::DROPOFF_COST)
            return false;

        // Map trop epuisee
        if (bb.average_halite < constants::SPAWN_MIN_AVG_HALITE)
            return false;

        // Soft cap de ships adapte a la taille de la map
        // 64x64 → 25 ships max, 32x32 → ~12
        int max_ships = constants::SPAWN_MAX_SHIPS_BASE * game_map->width / 64;
        if (bb.total_ships_alive >= max_ships)
            return false;

        // Verifier si le shipyard sera occupe apres les moves
        hlt::Position shipyard_pos = me->shipyard->position;
        for (size_t i = 0; i < results.size(); ++i)
        {
            for (const auto &req : requests)
            {
                if (req.m_ship_id == results[i].m_ship_id)
                {
                    hlt::Position final_pos = game_map->normalize(
                        req.m_current.directional_offset(results[i].m_final_direction));
                    if (final_pos == shipyard_pos)
                        return false;
                    break;
                }
            }
        }

        return true;
    }

    // ── Tour principal ──────────────────────────────────────────

    std::vector<hlt::Command> BotPlayer::play_turn()
    {
        // 1. Reset
        m_converting_ship_id = -1;

        // 2. Nettoyer les FSM des ships morts
        cleanup_dead_ships();

        // 3. Mettre a jour le blackboard
        update_blackboard();

        // 4. Commandes pre-mouvement (dropoff)
        std::vector<hlt::Command> commands;
        bool built_dropoff = try_build_dropoff(commands);

        // 5. Collecter les MoveRequests
        std::vector<MoveRequest> move_requests = collect_move_requests();

        // 6. Resoudre les conflits de mouvement
        std::vector<hlt::Position> dropoff_positions = get_dropoff_positions();
        int turns_remaining = hlt::constants::MAX_TURNS - game.turn_number;

        TrafficManager &traffic = TrafficManager::instance();
        traffic.init(*game.game_map, dropoff_positions, game.me->ships, turns_remaining);
        std::vector<MoveResult> move_results = traffic.resolve_all(move_requests);

        // 7. Convertir en commandes
        commands.reserve(commands.size() + move_results.size() + 1);

        for (const auto &result : move_results)
        {
            commands.push_back(hlt::command::move(result.m_ship_id, result.m_final_direction));
        }

        // 8. Spawn si necessaire (et si on n'a pas construit de dropoff ce tour)
        if (!built_dropoff && should_spawn(move_requests, move_results))
        {
            commands.push_back(game.me->shipyard->spawn());
        }

        return commands;
    }
} // namespace bot