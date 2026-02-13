#include "bot_player.hpp"
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
    }

    // ── Nettoyage ───────────────────────────────────────────────

    void BotPlayer::cleanup_dead_ships()
    {
        const auto &alive_ships = game.me->ships;
        for (auto it = ship_fsms.begin(); it != ship_fsms.end();)
        {
            if (alive_ships.find(it->first) == alive_ships.end())
                it = ship_fsms.erase(it);
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

            // Rechercher ou creer le ShipFSM (O(1) avec find)
            auto fsm_it = ship_fsms.find(ship->id);
            if (fsm_it == ship_fsms.end())
            {
                fsm_it = ship_fsms.emplace(
                    ship->id, std::unique_ptr<ShipFSM>(new ShipFSM(ship->id))).first;
            }

            requests.push_back(fsm_it->second->update(
                ship, *game_map, me->shipyard->position, turns_remaining));
        }

        return requests;
    }

    // ── Spawn ───────────────────────────────────────────────────

    bool BotPlayer::should_spawn(const std::vector<MoveRequest> &requests,
                                  const std::vector<MoveResult> &results) const
    {
        const Blackboard &bb = Blackboard::get_instance();
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;

        // Plus de spawn en fin de partie
        if (bb.current_phase == GamePhase::LATE ||
            bb.current_phase == GamePhase::ENDGAME)
            return false;

        // Pas assez de halite en banque
        if (me->halite < hlt::constants::SHIP_COST)
            return false;

        // En MID-game, ne spawn que si la map a encore du halite
        if (bb.current_phase == GamePhase::MID && bb.average_halite < 50)
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
        // 1. Nettoyer les FSM des ships morts
        cleanup_dead_ships();

        // 2. Mettre a jour le blackboard
        update_blackboard();

        // 3. Collecter les MoveRequests
        std::vector<MoveRequest> move_requests = collect_move_requests();

        // 4. Resoudre les conflits de mouvement
        std::vector<hlt::Position> dropoff_positions = get_dropoff_positions();
        int turns_remaining = hlt::constants::MAX_TURNS - game.turn_number;

        TrafficManager &traffic = TrafficManager::instance();
        traffic.init(*game.game_map, dropoff_positions, game.me->ships, turns_remaining);
        std::vector<MoveResult> move_results = traffic.resolve_all(move_requests);

        // 5. Convertir en commandes
        std::vector<hlt::Command> commands;
        commands.reserve(move_results.size() + 1);

        for (const auto &result : move_results)
        {
            commands.push_back(hlt::command::move(result.m_ship_id, result.m_final_direction));
        }

        // 6. Spawn si necessaire
        if (should_spawn(move_requests, move_results))
        {
            commands.push_back(game.me->shipyard->spawn());
        }

        return commands;
    }
} // namespace bot