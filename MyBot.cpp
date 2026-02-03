#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "HaliteAI/Bot/bot_player.hpp"
#include "HaliteAI/Bot/blackboard.hpp"

#include <random>
#include <ctime>

using namespace std;
using namespace hlt;

#ifdef _DEBUG
# define LOG(X) log::log(X);
#else
# define LOG(X)
#endif // DEBUG

int main(int argc, char* argv[]) {
    unsigned int rng_seed;
    if (argc > 1) {
        rng_seed = static_cast<unsigned int>(stoul(argv[1]));
    } else {
        rng_seed = static_cast<unsigned int>(time(nullptr));
    }
    mt19937 rng(rng_seed);

    Game game;

    // On initialise Blackboard
    bot::Blackboard::get_instance().init(game.game_map->width, game.game_map->height);

    // On crée le joueur
    bot::BotPlayer bot_player(game);

    game.ready("LeCanardIA");

    LOG("Toto")

    for (;;) {
        game.update_frame();

        vector<Command> command_queue = bot_player.step();

        if (!game.end_turn(command_queue)) {
            break;
        }
    }

    return 0;
}