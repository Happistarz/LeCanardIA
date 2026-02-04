#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"
#include "HaliteAI/Bot/bot_player.hpp"
#include "HaliteAI/Bot/ship_fsm.hpp"

#include <random>
#include <ctime>

using namespace std;
using namespace hlt;

#ifdef _DEBUG
#define LOG(X) log::log(X);
#else
#define LOG(X)
#endif // DEBUG

int main(int argc, char *argv[])
{
    unsigned int rng_seed;
    if (argc > 1)
    {
        rng_seed = static_cast<unsigned int>(stoul(argv[1]));
    }
    else
    {
        rng_seed = static_cast<unsigned int>(time(nullptr));
    }
    mt19937 rng(rng_seed);

    Game game;
    game.ready("LeCanardIA");

    std::vector<bot::ShipFSM *> ship_fsms;

    for (;;)
    {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap> &game_map = game.game_map;

        vector<Command> command_queue;

        bot::BotPlayer bot_player(game);

        for (const auto &ship_iterator : me->ships)
        {
            shared_ptr<Ship> ship = ship_iterator.second;

            // Rechercher un ShipFSM existant
            bot::ShipFSM *fsm = nullptr;
            for (bot::ShipFSM *existing_fsm : ship_fsms)
            {
                if (existing_fsm->get_ship_id() == ship->id)
                {
                    fsm = existing_fsm;
                    break;
                }
            }

            // Créer un nouveau ShipFSM si nécessaire
            if (fsm == nullptr)
            {
                fsm = new bot::ShipFSM(ship->id);
                ship_fsms.push_back(fsm);
            }

            Command command = fsm->update(ship, *game_map,
                                          me->shipyard->position,
                                          hlt::constants::MAX_TURNS - game.turn_number);
            command_queue.push_back(command);
        }

        // TODO: Améliorer la logique de spawn
        if (game.turn_number <= 200 && me->halite >= hlt::constants::SHIP_COST)
        {
            command_queue.push_back(me->shipyard->spawn());
        }

        if (!game.end_turn(command_queue))
        {
            break;
        }
    }

    // Cleanup
    for (const bot::ShipFSM *fsm : ship_fsms)
    {
        delete fsm;
    }
    ship_fsms.clear();

    return 0;
}
