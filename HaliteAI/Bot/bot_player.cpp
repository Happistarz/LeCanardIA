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

    // UPDATE BLACKBOARD

    void BotPlayer::update_blackboard()
    {
        Blackboard& bb = Blackboard::get_instance();
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap>& game_map = game.game_map;

        bb.clear_turn_data();
        bb.total_ships_alive = static_cast<int>(me->ships.size());
        bb.drop_positions = get_drops_positions();

        update_map_stats(bb, game_map);

        update_game_phase(bb);

        update_stuck_ships(bb, game_map, me);

        // Calcul de la heatmap pour le clustering
        bb.compute_heatmap(*game_map);

        update_enemy_info(bb, game_map);

        update_persistent_targets(bb);

        // Calculer les zones d'inspiration
        bb.compute_inspired_zones(game_map->width, game_map->height);

        update_position_history(bb, game_map, me);

    }

    // FONCTIONS DE BLACKBOARD

    // Stats de la map
    void BotPlayer::update_map_stats(Blackboard& bb, std::unique_ptr<hlt::GameMap>& game_map) {

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
    }

    // Phase de jeu
    void BotPlayer::update_game_phase(Blackboard& bb) {

        // Phase de jeu
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
    }

    // Ships bloqués
    void BotPlayer::update_stuck_ships(Blackboard& bb, std::unique_ptr<hlt::GameMap>& game_map, std::shared_ptr<hlt::Player> me) {
        // Ship stuck : pas assez de halite pour bouger hors de la cell
        for (const auto& ship_pair : me->ships)
        {
            const auto& ship = ship_pair.second;
            int cell_halite = game_map->at(ship->position)->halite;
            int move_cost = cell_halite / hlt::constants::MOVE_COST_RATIO;
            if (ship->halite < move_cost)
            {
                bb.stuck_positions.insert(game_map->normalize(ship->position));
            }
        }
    }

    // Infos ennemis
    void BotPlayer::update_enemy_info(Blackboard& bb, std::unique_ptr<hlt::GameMap>& game_map) {
        for (const auto& player : game.players)
        {
            if (player->id == game.my_id)
                continue;

            for (const auto& ship_pair : player->ships)
            {
                hlt::Position norm_pos = game_map->normalize(ship_pair.second->position);
                bb.danger_zones.insert(norm_pos);
                bb.enemy_ships.push_back({ ship_pair.first, norm_pos, ship_pair.second->halite });
            }

            bb.danger_zones.insert(game_map->normalize(player->shipyard->position));

            for (const auto& dropoff_pair : player->dropoffs)
            {
                bb.danger_zones.insert(game_map->normalize(dropoff_pair.second->position));
            }
        }
    }

    // Marquer les persistent_targets comme targeted_cells
    void BotPlayer::update_persistent_targets(Blackboard& bb) {
        for (const auto& pt : bb.persistent_targets)
        {
            bb.targeted_cells[pt.second] = pt.first;
        }
    }

    // Update l'historique de positions pour detecter les oscillations
    void BotPlayer::update_position_history(Blackboard& bb, std::unique_ptr<hlt::GameMap>& game_map, std::shared_ptr<hlt::Player> me) {
        for (const auto& ship_pair : me->ships)
        {
            bb.update_position_history(ship_pair.first,
                game_map->normalize(ship_pair.second->position));
        }
    }
    // _____________________________________

    // NETTOYAGE

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

    // HELPERS

    // Get Drops Positions
    std::vector<hlt::Position> BotPlayer::get_drops_positions() const
    {
        std::vector<hlt::Position> positions;
        positions.push_back(game.me->shipyard->position);
        for (const auto &dropoff_pair : game.me->dropoffs)
        {
            positions.push_back(dropoff_pair.second->position);
        }
        return positions;
    }
    // Closest drop
    hlt::Position BotPlayer::closest_drop(const hlt::Position &pos) const
    {
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;
        std::vector<hlt::Position> depots = get_drops_positions();

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

    // MOVE REQUESTS

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

            // Skip si ship en cours de conversion en dropoff
            if (should_skip_ship(*ship))
                continue;

            // Skip si ship est en mission de dropoff
            const Blackboard &bb_ref = Blackboard::get_instance();
            if (is_dropoff_ship(*ship, bb_ref))
            {
                // Navigation manuelle vers la pos du dropoff
                requests.push_back(handle_dropoff_ship(ship, *game_map, bb_ref));
            }
            else {

            requests.push_back(handle_normal_ship(ship, *game_map, turns_remaining));
            }
        }

        return requests;
    }

    // FONCTIONS DE MOVE REQUESTS

    // Should Skip Ship
    bool BotPlayer::should_skip_ship(const hlt::Ship& ship) const
    {
        if (m_converting_ship_id >= 0 && ship.id == m_converting_ship_id)
            return true;

        return false;
    }

    // Is Dropoff Ship
    bool BotPlayer::is_dropoff_ship(const hlt::Ship& ship, const Blackboard& bb) const
    {
        return (bb.dropoff_ship_id >= 0 && ship.id == bb.dropoff_ship_id);
    }

    // Navigation spéciale dropoff
    MoveRequest BotPlayer::handle_dropoff_ship(std::shared_ptr<hlt::Ship> ship,
        hlt::GameMap& map,
        const Blackboard& bb)
    {
        hlt::Direction best_dir;
        std::vector<hlt::Direction> alternatives;

        map_utils::navigate_toward(ship, map, bb.planned_dropoff_pos,
            bb.stuck_positions, bb.danger_zones,
            best_dir, alternatives);

        hlt::Position desired = map.normalize(ship->position.directional_offset(best_dir));

        return { ship->id, ship->position, desired, best_dir,
                constants::RETURN_PRIORITY, alternatives };
    }

    // Ship normal (FSM)
    MoveRequest BotPlayer::handle_normal_ship(std::shared_ptr<hlt::Ship> ship,
        hlt::GameMap& map,
        int turns_remaining)
    {
        auto fsm_it = ship_fsms.find(ship->id);
        if (fsm_it == ship_fsms.end())
            fsm_it = ship_fsms.emplace(ship->id, std::make_unique<ShipFSM>(ship->id)).first;

        hlt::Position depot = closest_drop(ship->position);
        return fsm_it->second->update(ship, map, depot, turns_remaining);
    }
    // _____________________________________

    // DROPOFF

    bool BotPlayer::try_build_dropoff(std::vector<hlt::Command> &commands)
    {
        Blackboard &bb = Blackboard::get_instance();
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;

        if (!can_build_dropoff(bb, *me, *game_map))
            return false;

        reset_dead_dropoff_plan(bb, *me);

        if (has_active_dropoff_plan(bb))
            return execute_dropoff_plan(commands, bb, *me, *game_map);

        return create_new_dropoff_plan(bb, *me, *game_map);
    }

    // FONCTIONS DE DROPOFF

    // Conditions globales
    bool BotPlayer::can_build_dropoff(const Blackboard& bb,
        const hlt::Player& me,
        const hlt::GameMap& map) const
    {
        if ((int)me.dropoffs.size() >= constants::MAX_DROPOFFS)
            return false;

        if (bb.current_phase == GamePhase::LATE || bb.current_phase == GamePhase::ENDGAME)
            return false;

        if (bb.total_ships_alive < constants::MIN_SHIPS_FOR_DROPOFF)
            return false;

        if (me.halite < hlt::constants::DROPOFF_COST / 2)
            return false;

        return true;
    }
    
    // Reset si ship mort
    void BotPlayer::reset_dead_dropoff_plan(Blackboard& bb, const hlt::Player& me)
    {
        if (bb.dropoff_ship_id >= 0 &&
            me.ships.find(bb.dropoff_ship_id) == me.ships.end())
        {
            hlt::log::log("Dropoff: ship mort, reset plan");
            bb.planned_dropoff_pos = { -1, -1 };
            bb.dropoff_ship_id = -1;
        }
    }
    
    // Plan actif ?
    bool BotPlayer::has_active_dropoff_plan(const Blackboard& bb) const
    {
        return (bb.planned_dropoff_pos.x >= 0 && bb.dropoff_ship_id >= 0);
    }

    // Exécuter le plan (navigation + conversion) 
    bool BotPlayer::execute_dropoff_plan(std::vector<hlt::Command>& commands,
        Blackboard& bb,
        hlt::Player& me,
        hlt::GameMap& map)
    {
        auto ship_it = me.ships.find(bb.dropoff_ship_id);
        if (ship_it == me.ships.end())
            return false;

        auto ship = ship_it->second;
        hlt::Position target = bb.planned_dropoff_pos;

        if (map.at(target)->has_structure())
        {
            hlt::log::log("Dropoff: position bloquee par structure, reset plan");
            bb.persistent_targets.erase(ship->id);
            bb.planned_dropoff_pos = { -1, -1 };
            bb.dropoff_ship_id = -1;
            return false;
        }

        if (bb.is_ship_oscillating(ship->id))
        {
            hlt::log::log("Dropoff: ship oscille, reset plan");
            bb.persistent_targets.erase(ship->id);
            bb.planned_dropoff_pos = { -1, -1 };
            bb.dropoff_ship_id = -1;
            return false;
        }

        if (ship->position != target)
        {
            bb.persistent_targets[ship->id] = target;
            return false;
        }
        // Ship arrived → try convert
        int real_cost = hlt::constants::DROPOFF_COST - ship->halite - map.at(target)->halite;
        if (real_cost < 0) real_cost = 0;

        if (me.halite < real_cost)
            return false;

        hlt::log::log("Dropoff: conversion ship " + std::to_string(ship->id));
        commands.push_back(ship->make_dropoff());
        m_converting_ship_id = ship->id;

        bb.planned_dropoff_pos = { -1, -1 };
        bb.dropoff_ship_id = -1;

        return true;
    }

    // Créer un nouveau plan
    bool BotPlayer::create_new_dropoff_plan(Blackboard& bb,
        hlt::Player& me,
        hlt::GameMap& map)
    {
        int min_depot_dist = std::max(8, map.width / constants::MIN_DROPOFF_DEPOT_DISTANCE_RATIO);

        std::vector<hlt::Position> depots = get_drops_positions();
        hlt::Position best_pos = bb.find_best_dropoff_position(map, depots, min_depot_dist);

        if (best_pos.x < 0)
            return false;

        auto best_ship = find_best_dropoff_ship(me, map, best_pos);
        if (!best_ship)
            return false;

        bb.planned_dropoff_pos = best_pos;
        bb.dropoff_ship_id = best_ship->id;
        bb.persistent_targets[best_ship->id] = best_pos;

        hlt::log::log("Dropoff plan created ship " + std::to_string(best_ship->id));
        return false;
    }
    
    // Trouver le meilleur ship
    std::shared_ptr<hlt::Ship> BotPlayer::find_best_dropoff_ship(hlt::Player& me,
        hlt::GameMap& map,
        const hlt::Position& pos)
    {
        std::shared_ptr<hlt::Ship> best_ship = nullptr;
        int best_dist = 9999;
        int max_assign_dist = map.width / 3;

        for (const auto& p : me.ships)
        {
            const auto& ship = p.second;
            int d = map.calculate_distance(ship->position, pos);

            if (d < best_dist && d <= max_assign_dist)
            {
                best_dist = d;
                best_ship = ship;
            }
        }
        return best_ship;
    }
    
    // _____________________________________
    
    // SPAWN

    bool BotPlayer::should_spawn(const std::vector<MoveRequest> &requests,
                                  const std::vector<MoveResult> &results) const
    {
        const Blackboard &bb = Blackboard::get_instance();
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;
        int turns_remaining = hlt::constants::MAX_TURNS - game.turn_number;

        if (!can_spawn_phase(bb, turns_remaining))
            return false;

        if (!has_spawn_halite(bb, *me))
            return false;

        if (!map_is_rich_enough(bb))
            return false;

        if (!below_max_ships(bb, *game_map))
            return false;

        if (shipyard_will_be_occupied(*me, game_map, requests, results))
            return false;

        if (shipyard_congested(*me, *game_map))
            return false;

        return true;
    }

    // FONCTIONS DE SPAWN

    // Phase + tours restants
    bool BotPlayer::can_spawn_phase(const Blackboard& bb, int turns_remaining) const
    {
        if (bb.current_phase == GamePhase::LATE ||
            bb.current_phase == GamePhase::ENDGAME)
            return false;

        if (turns_remaining < constants::SPAWN_MIN_TURNS_LEFT)
            return false;

        return true;
    }

    // Halite disponible (spawn + dropoff reserve)
    bool BotPlayer::has_spawn_halite(const Blackboard& bb, const hlt::Player& me) const
    {
        if (me.halite < hlt::constants::SHIP_COST)
            return false;

        if (bb.planned_dropoff_pos.x >= 0 &&
            me.halite < hlt::constants::SHIP_COST + hlt::constants::DROPOFF_COST)
            return false;

        return true;
    }

    // Map assez riche ?
    bool BotPlayer::map_is_rich_enough(const Blackboard& bb) const
    {
        return bb.average_halite >= constants::SPAWN_MIN_AVG_HALITE;
    }

    // Limite de ships
    bool BotPlayer::below_max_ships(const Blackboard& bb, const hlt::GameMap& map) const
    {
        int max_ships = constants::SPAWN_MAX_SHIPS_BASE * map.width / 64;
        return bb.total_ships_alive < max_ships;
    }

    // Collision sur le shipyard
    bool BotPlayer::shipyard_will_be_occupied(const hlt::Player& me,
        std::unique_ptr<hlt::GameMap>& map,
        const std::vector<MoveRequest>& requests,
        const std::vector<MoveResult>& results) const
    {
        hlt::Position shipyard_pos = me.shipyard->position;

        for (const auto& res : results)
        {
            for (const auto& req : requests)
            {
                if (req.m_ship_id == res.m_ship_id)
                {
                    hlt::Position final_pos =
                        map->normalize(req.m_current.directional_offset(res.m_final_direction));

                    if (final_pos == shipyard_pos)
                        return true;

                    break;
                }
            }
        }

        return false;
    }

    // _____________________________________

    // MAIN

    bool BotPlayer::shipyard_congested(const hlt::Player& me, const hlt::GameMap& map) const
    {
        hlt::Position yard_pos = me.shipyard->position;
        int nearby = 0;
        for (const auto& ship_pair : me.ships)
        {
            int d = map_utils::toroidal_distance(ship_pair.second->position, yard_pos, map.width, map.height);
            if (d <= constants::SPAWN_CONGESTION_RADIUS)
                ++nearby;
        }
        return nearby >= constants::SPAWN_CONGESTION_LIMIT;
    }

    std::vector<hlt::Command> BotPlayer::play_turn()
    {
        m_converting_ship_id = -1;

        cleanup_dead_ships();

        update_blackboard();

        std::vector<hlt::Command> commands;
        bool built_dropoff = try_build_dropoff(commands);

        std::vector<MoveRequest> move_requests = collect_move_requests();

        std::vector<hlt::Position> drops_positions = get_drops_positions();
        int turns_remaining = hlt::constants::MAX_TURNS - game.turn_number;

        TrafficManager &traffic = TrafficManager::instance();
        traffic.init(*game.game_map, drops_positions, game.me->ships, turns_remaining);
        std::vector<MoveResult> move_results = traffic.resolve_all(move_requests);

        commands.reserve(commands.size() + move_results.size() + 1);

        for (const auto &result : move_results)
        {
            commands.push_back(hlt::command::move(result.m_ship_id, result.m_final_direction));
        }

        // Tenter de spawn un nouveau ship si les conditions sont reunies
        if (!built_dropoff && should_spawn(move_requests, move_results))
        {
            commands.push_back(game.me->shipyard->spawn());
        }

        return commands;
    }
} // namespace bot