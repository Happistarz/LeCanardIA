#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"
#include "HaliteAI/Bot/bot_player.hpp"
#include "HaliteAI/Bot/ship_fsm.hpp"
#include "HaliteAI/Bot/traffic_manager.hpp"
#include "HaliteAI/Bot/blackboard.hpp"

#include <random>
#include <ctime>

#ifdef _DEBUG
#define LOG(X) log::log(X);
#else
#define LOG(X)
#endif // DEBUG

int main(int argc, char *argv[])
{
    hlt::Game game;
    game.ready("LeCanardIA");

    std::unordered_map<hlt::EntityId, std::unique_ptr<bot::ShipFSM>> ship_fsms;

    for (;;)
    {
        game.update_frame();
        std::shared_ptr<hlt::Player> me = game.me;
        std::unique_ptr<hlt::GameMap> &game_map = game.game_map;

        int turns_remaining = hlt::constants::MAX_TURNS - game.turn_number;

        // Blackboard reset
        bot::Blackboard &blackboard = bot::Blackboard::get_instance();
        blackboard.clear_turn_data();
        blackboard.total_ships_alive = static_cast<int>(me->ships.size());

        // Récupérer les positions des dropoffs et du shipyard
        std::vector<hlt::Position> dropoff_positions;
        dropoff_positions.push_back(me->shipyard->position);
        for (const auto &dropoff_pair : me->dropoffs)
        {
            dropoff_positions.push_back(dropoff_pair.second->position);
        }

        // Trouver les ships bloqués (halite insuffisant pour bouger)
        for (const auto &ship_pair : me->ships)
        {
            const auto &ship = ship_pair.second;
            int cell_halite = game_map->at(ship->position)->halite;
            int move_cost = cell_halite / hlt::constants::MOVE_COST_RATIO;
            if (ship->halite < move_cost)
            {
                blackboard.stuck_positions.insert(game_map->normalize(ship->position));
            }
        }

        // Movement Requests
        std::vector<bot::MoveRequest> move_requests;
        move_requests.reserve(me->ships.size());

        for (const auto &ship_iterator : me->ships)
        {
            std::shared_ptr<hlt::Ship> ship = ship_iterator.second;

            // Rechercher un ShipFSM existant
            bot::ShipFSM *fsm = nullptr;
            for (const auto &pair : ship_fsms)
            {
                if (pair.first == ship->id)
                {
                    fsm = pair.second.get();
                    break;
                }
            }

            // Créer un nouveau ShipFSM si nécessaire
            if (fsm == nullptr)
            {
                fsm = new bot::ShipFSM(ship->id);
                ship_fsms[ship->id] = std::unique_ptr<bot::ShipFSM>(fsm);
            }

            // production d'une MoveRequest pour ce ship
            bot::MoveRequest request = fsm->update(ship, *game_map,
                                                   me->shipyard->position,
                                                   turns_remaining);
            move_requests.push_back(request);
        }

        // Résolution des conflits de MoveRequest
        auto &traffic = bot::TrafficManager::instance();
        traffic.init(*game_map, dropoff_positions, me->ships, turns_remaining);
        std::vector<bot::MoveResult> move_results = traffic.resolve_all(move_requests);

        // Conversion des MoveResult en Command
        std::vector<hlt::Command> command_queue;
        command_queue.reserve(move_results.size() + 1);

        for (const auto &result : move_results)
        {
            command_queue.push_back(hlt::command::move(result.m_ship_id, result.m_final_direction));
        }

        // Vérifier si le shipyard sera occupé après les move
        bool shipyard_will_be_occupied = false;
        hlt::Position shipyard_pos = me->shipyard->position;
        for (size_t i = 0; i < move_results.size(); ++i)
        {
            // Trouver la MoveRequest correspondante pour ce MoveResult
            for (const auto &req : move_requests)
            {
                if (req.m_ship_id == move_results[i].m_ship_id)
                {
                    hlt::Position final_pos = game_map->normalize(
                        req.m_current.directional_offset(move_results[i].m_final_direction));
                    if (final_pos == shipyard_pos)
                    {
                        shipyard_will_be_occupied = true;
                    }
                    break;
                }
            }
            if (shipyard_will_be_occupied)
                break;
        }

        if (game.turn_number <= 200 && me->halite >= hlt::constants::SHIP_COST &&
            !shipyard_will_be_occupied)
        {
            command_queue.push_back(me->shipyard->spawn());
        }

        if (!game.end_turn(command_queue))
        {
            break;
        }
    }

    // Cleanup
    ship_fsms.clear();

    return 0;
}
