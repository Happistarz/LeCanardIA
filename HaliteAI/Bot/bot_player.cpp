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
        handle_spawn(commands);
        assign_missions_to_ships();
        execute_missions(commands);

        return commands;
    }

    // Analyse
    void BotPlayer::perform_analysis()
    {
        Blackboard &bb = Blackboard::get_instance();
        bb.clear_turn_data();
        bb.update_metrics(m_game);
        bb.update_phase(m_game.turn_number, hlt::constants::MAX_TURNS);
        bb.update_best_cluster(m_game);

        // Marquer les ships stuck
        for (const auto &entry : m_game.me->ships)
        {
            if (is_ship_stuck(*entry.second, *m_game.game_map))
                bb.stuck_positions.insert(entry.second->position);
        }
    }

    // Spawn
    void BotPlayer::handle_spawn(std::vector<hlt::Command> &commands)
    {
        Blackboard &bb = Blackboard::get_instance();
        if (bb.should_spawn(*m_game.me))
        {
            commands.push_back(m_game.me->shipyard->spawn());
            bb.reserve_position(m_game.me->shipyard->position, -1);
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

        // Reset auto apres depot
        if (bb.ship_missions[id] == MissionType::RETURNING && ship->halite == 0)
            bb.assign_mission(id, MissionType::MINING);

        // CONSTRUCTION
        int dist_base = m_game.game_map->calculate_distance(ship->position, m_game.me->shipyard->position);
        bool has_halite = m_game.me->halite >= params::DROPOFF_COST;
        bool fleet_big_enough = m_game.me->ships.size() >= static_cast<size_t>(params::MIN_SHIPS_FOR_DROPOFF);
        bool dist_sufficient = dist_base > params::DROPOFF_MIN_DISTANCE;
        bool spot_rich_enough = m_game.game_map->at(ship->position)->halite > params::DROPOFF_SPOT_HALITE;
        bool not_endgame = bb.current_phase != GamePhase::ENDGAME;
        bool can_build = has_halite && fleet_big_enough && dist_sufficient && spot_rich_enough && not_endgame;
        if (can_build)
        {
            bb.assign_mission(id, MissionType::CONSTRUCTING);
            return;
        }

        // RETURN
        if (ship->halite > hlt::constants::MAX_HALITE * params::RETURN_RATIO || bb.current_phase == GamePhase::ENDGAME)
        {
            bb.assign_mission(id, MissionType::RETURNING, m_game.me->shipyard->position);
            return;
        }

        // DEFENSE
        if (under_threat && dist_base < params::DEFENSE_INTERCEPT_DIST && ship->halite < params::DEFENSE_HALITE_LIMIT)
        {
            bb.assign_mission(id, MissionType::DEFENDING, m_game.me->shipyard->position);
            return;
        }

        // ESCOUADE OFFENSIVE
        if (bb.current_phase == GamePhase::LATE && m_game.me->ships.size() > static_cast<size_t>(params::SQUAD_MIN_FLEET_SIZE))
        {
            if (bb.ship_missions[id] == MissionType::LOOTING)
            {
                bb.assign_mission(id, MissionType::LOOTING, bb.target_loot_zone);
                return;
            }
            if (bb.squad_links.count(id))
            {
                hlt::EntityId protected_id = bb.squad_links[id];
                if (m_game.me->ships.count(protected_id))
                    bb.assign_mission(id, MissionType::ATTACKING, m_game.me->ships[protected_id]->position);
                else
                {
                    bb.squad_links.erase(id);
                    bb.assign_mission(id, MissionType::RETURNING);
                }
                return;
            }
            if (bb.ship_missions[id] == MissionType::MINING)
            {
                int dist_cluster = m_game.game_map->calculate_distance(
                    m_game.me->shipyard->position, bb.best_cluster_position);

                if (dist_cluster > params::LOOT_DIST_THRESHOLD)
                {
                    bb.target_loot_zone = bb.best_cluster_position;
                    bb.assign_mission(id, MissionType::LOOTING, bb.target_loot_zone);

                    // Recruter le mineur inactif le plus proche comme garde du corps
                    hlt::EntityId buddy_id = -1;
                    int min_d = 9999;
                    for (auto &other : m_game.me->ships)
                    {
                        if (other.first == id)
                            continue;
                        if (bb.ship_missions[other.first] != MissionType::MINING)
                            continue;
                        int d = m_game.game_map->calculate_distance(ship->position, other.second->position);
                        if (d < min_d && d < params::BUDDY_MAX_DIST)
                        {
                            min_d = d;
                            buddy_id = other.first;
                        }
                    }
                    if (buddy_id != -1)
                    {
                        bb.squad_links[buddy_id] = id;
                        bb.assign_mission(buddy_id, MissionType::ATTACKING, ship->position);
                    }
                    return;
                }
            }
        }

        // DEFAUT : MINING
        if (bb.ship_missions.find(id) == bb.ship_missions.end() || bb.ship_missions[id] == MissionType::DEFENDING)
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

        std::vector<MoveRequest> requests;
        requests.reserve(m_game.me->ships.size());

        for (const auto &entry : m_game.me->ships)
        {
            auto ship = entry.second;
            MissionType mission = bb.ship_missions[ship->id];

            // CONSTRUCTION : pas de resolve de trafic
            if (mission == MissionType::CONSTRUCTING)
            {
                commands.push_back(ship->make_dropoff());
                m_game.me->halite -= params::DROPOFF_COST;
                continue;
            }

            hlt::Position nearest = find_nearest_dropoff(ship);
            MoveRequest req;

            switch (mission)
            {
            case MissionType::RETURNING:
            {
                int prio = (bb.current_phase == GamePhase::ENDGAME)
                               ? MoveRequest::URGENT_RETURN_PRIORITY
                               : MoveRequest::RETURN_PRIORITY;
                req = make_move_request(ship, nearest, prio, *m_game.game_map, blackboard_is_stuck);
                break;
            }
            case MissionType::DEFENDING:
                req = make_move_request(ship, bb.ship_targets[ship->id],
                                        MoveRequest::FLEE_PRIORITY, *m_game.game_map, blackboard_is_stuck);
                break;

            case MissionType::ATTACKING:
            case MissionType::LOOTING:
                req = make_move_request(ship, bb.ship_targets[ship->id],
                                        MoveRequest::EXPLORE_PRIORITY, *m_game.game_map, blackboard_is_stuck);
                break;

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