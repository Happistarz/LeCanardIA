#include "bot_player.hpp"
#include "blackboard.hpp"
#include "bot_parameters.hpp"
#include "utils.hpp"
#include "hlt/game.hpp"
#include "hlt/constants.hpp"

#include <algorithm>
#include <vector>

namespace bot
{
    // Requete de position stuck pour le blackboard
    static bool blackboard_is_stuck(const hlt::Position &pos)
    {
        return Blackboard::get_instance().is_position_stuck(pos);
    }

    // Constructeur
    BotPlayer::BotPlayer(hlt::Game &game_instance) : m_game(game_instance) {}

    // Boucle principale
    std::vector<hlt::Command> BotPlayer::step()
    {
        std::vector<hlt::Command> commands;

        perform_analysis();
        assign_missions_to_ships();
        execute_missions(commands); // Resout les mouvements ET spawn si possible

        return commands;
    }

    // Analyse
    void BotPlayer::perform_analysis()
    {
        Blackboard &bb = Blackboard::get_instance();
        bb.clear_turn_data();
        bb.update_metrics(m_game);
        bb.update_phase(m_game.turn_number, hlt::constants::MAX_TURNS);
        bb.update_enemy_structures(m_game);
        bb.update_clusters(m_game);
        bb.assign_ship_targets(m_game);

        // Nettoyer les donnees des ships morts
        for (auto it = bb.ship_missions.begin(); it != bb.ship_missions.end();)
        {
            if (!m_game.me->ships.count(it->first))
            {
                bb.ship_targets.erase(it->first);
                bb.squad_links.erase(it->first);
                bb.ship_explore_targets.erase(it->first);
                it = bb.ship_missions.erase(it);
            }
            else
                ++it;
        }

        // Marquer les ships stuck
        for (const auto &entry : m_game.me->ships)
        {
            if (is_ship_stuck(*entry.second, *m_game.game_map))
                bb.stuck_positions.insert(entry.second->position);
        }
    }

    // Dispatch des missions
    int BotPlayer::compute_nearest_enemy_distance() const
    {
        int min_dist = 9999;
        for (const auto &player : m_game.players)
        {
            if (player->id == m_game.me->id)
                continue;
            for (const auto &ship_entry : player->ships)
            {
                int d = m_game.game_map->calculate_distance(
                    ship_entry.second->position, m_game.me->shipyard->position);

                if (d < min_dist)
                    min_dist = d;
            }
        }
        return min_dist;
    }

    void BotPlayer::assign_missions_to_ships()
    {
        bool under_threat = compute_nearest_enemy_distance() < params::DANGER_ZONE_RADIUS;

        for (auto &entry : m_game.me->ships)
            determine_mission(entry.second, under_threat);
    }

    void BotPlayer::determine_mission(std::shared_ptr<hlt::Ship> ship, bool under_threat)
    {
        Blackboard &bb = Blackboard::get_instance();
        hlt::EntityId id = ship->id;
        hlt::Position nearest = find_nearest_dropoff(ship);
        int dist_to_dropoff = m_game.game_map->calculate_distance(ship->position, nearest);
        int turns_remaining = hlt::constants::MAX_TURNS - m_game.turn_number;

        // Reset de la mission de retour
        if (bb.ship_missions[id] == MissionType::RETURNING && ship->halite == 0)
            bb.assign_mission(id, MissionType::MINING);

        // ENDGAME URGENT : seulement si le ship doit partir MAINTENANT pour rentrer a temps
        if (turns_remaining < dist_to_dropoff + params::SAFE_RETURN_TURNS)
        {
            bb.assign_mission(id, MissionType::RETURNING, nearest);
            return;
        }

        // Si le ship est riche et proche du dropoff, le faire retourner
        if (bb.ship_missions.count(id) && bb.ship_missions[id] == MissionType::MINING)
            return;

        // DEFAUT : nouveau ship -> MINING
        if (bb.ship_missions.find(id) == bb.ship_missions.end())
        {
            bb.assign_mission(id, MissionType::MINING, bb.best_cluster_position);
        }
    }

    // Execution des missions
    void BotPlayer::execute_missions(std::vector<hlt::Command> &commands)
    {
        Blackboard &bb = Blackboard::get_instance();

        sync_ship_fsms();

        std::vector<hlt::Position> dropoffs = get_dropoff_positions();
        int turns_remaining = hlt::constants::MAX_TURNS - m_game.turn_number;

        TrafficManager::instance().init(*m_game.game_map, dropoffs,
                                        m_game.me->ships, turns_remaining);

        // Determiner si on veut spawn avant de resoudre les mouvements, pour forcer les ships sur le shipyard a degager
        bool want_spawn = bb.should_spawn(*m_game.me);
        hlt::Position shipyard_pos = m_game.me->shipyard->position;

        std::vector<MoveRequest> requests;
        requests.reserve(m_game.me->ships.size());

        for (const auto &entry : m_game.me->ships)
        {
            auto ship = entry.second;
            MissionType mission = bb.ship_missions[ship->id];

            hlt::Position nearest = find_nearest_dropoff(ship);
            MoveRequest req;

            // Si on veut spawn et que le ship est sur le shipyard, forcer un move pour liberer le spawn
            bool on_shipyard = (m_game.game_map->normalize(ship->position) == m_game.game_map->normalize(shipyard_pos));
            if (want_spawn && on_shipyard)
            {
                // Si la cell adjacente la plus riche est meilleure que le shipyard, y aller directement
                auto scored = score_directions_by_halite(ship->position, *m_game.game_map);
                sort_scored_directions(scored);

                hlt::Direction best_dir;
                std::vector<hlt::Direction> alternatives;
                extract_best_and_alternatives(scored, best_dir, alternatives);

                hlt::Position desired = m_game.game_map->normalize(ship->position.directional_offset(best_dir));
                req = MoveRequest{ship->id, ship->position, desired,
                                  best_dir, MoveRequest::SHIP_ON_DROPOFF_PRIORITY, alternatives};
                requests.push_back(req);
                continue;
            }

            switch (mission)
            {
            case MissionType::RETURNING:
            {
                int dist = m_game.game_map->calculate_distance(ship->position, nearest);
                int prio;
                if (turns_remaining < dist + params::SAFE_RETURN_TURNS)
                    prio = (dist <= 2) ? MoveRequest::URGENT_RETURN_NEAR_PRIORITY
                                       : MoveRequest::URGENT_RETURN_PRIORITY;
                else
                    prio = MoveRequest::RETURN_PRIORITY;
                req = make_move_request(ship, nearest, prio, *m_game.game_map, blackboard_is_stuck);
                break;
            }
            case MissionType::MINING:
            default:
                if (m_ship_fsms.count(ship->id))
                    req = m_ship_fsms[ship->id]->update(ship, *m_game.game_map, nearest, turns_remaining);
                else
                    req = make_move_request(ship, bb.best_cluster_position,
                                            MoveRequest::EXPLORE_PRIORITY, *m_game.game_map, blackboard_is_stuck);
                break;
            }

            requests.push_back(req);
        }

        // Resolve des mouvements
        std::vector<MoveResult> results = TrafficManager::instance().resolve_all(requests);

        // Verifier si le shipyard sera libre apres resolve
        bool shipyard_free = true;
        for (const auto &res : results)
        {
            auto ship_it = m_game.me->ships.find(res.m_ship_id);
            if (ship_it == m_game.me->ships.end())
                continue;

            hlt::Position final_pos;
            if (res.m_final_direction == hlt::Direction::STILL)
                final_pos = m_game.game_map->normalize(ship_it->second->position);
            else
                final_pos = m_game.game_map->normalize(
                    ship_it->second->position.directional_offset(res.m_final_direction));

            if (final_pos == m_game.game_map->normalize(shipyard_pos))
            {
                shipyard_free = false;
                break;
            }
        }

        // Spawn seulement si le shipyard est libre et que la mission de spawn est validee
        if (want_spawn && shipyard_free)
            commands.push_back(m_game.me->shipyard->spawn());

        for (const auto &res : results)
        {
            if (!m_game.me->ships.count(res.m_ship_id))
                continue;
            auto ship = m_game.me->ships[res.m_ship_id];
            commands.push_back(res.m_final_direction == hlt::Direction::STILL
                                   ? ship->stay_still()
                                   : ship->move(res.m_final_direction));
        }
    }

    // Utilitaires
    void BotPlayer::sync_ship_fsms()
    {
        // Deleter les FSM des ships morts
        for (auto it = m_ship_fsms.begin(); it != m_ship_fsms.end();)
        {
            if (m_game.me->ships.find(it->first) == m_game.me->ships.end())
                it = m_ship_fsms.erase(it);
            else
                ++it;
        }

        // Creer un FSM pour les nouveaux ships
        for (const auto &entry : m_game.me->ships)
        {
            if (!m_ship_fsms.count(entry.first))
                m_ship_fsms[entry.first] = std::make_unique<ShipFSM>(entry.first);
        }
    }

    std::vector<hlt::Position> BotPlayer::get_dropoff_positions() const
    {
        std::vector<hlt::Position> positions;
        positions.reserve(1 + m_game.me->dropoffs.size());
        positions.push_back(m_game.me->shipyard->position);
        for (const auto &dp : m_game.me->dropoffs)
            positions.push_back(dp.second->position);
        return positions;
    }

    hlt::Position BotPlayer::find_nearest_dropoff(std::shared_ptr<hlt::Ship> ship) const
    {
        hlt::Position nearest = m_game.me->shipyard->position;
        int min_dist = m_game.game_map->calculate_distance(ship->position, nearest);

        for (const auto &dp : m_game.me->dropoffs)
        {
            int d = m_game.game_map->calculate_distance(ship->position, dp.second->position);
            if (d < min_dist)
            {
                min_dist = d;
                nearest = dp.second->position;
            }
        }
        return nearest;
    }

} // namespace bot