#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "HaliteAI/Bot/bot_player.hpp"
#include "HaliteAI/Bot/blackboard.hpp"
#include "hlt/log.hpp"


int main(int argc, char *argv[])
{
    hlt::Game game;

    // On initialise Blackboard
    bot::Blackboard::get_instance().init(game.game_map->width, game.game_map->height);

    // On crée le joueur
    bot::BotPlayer bot_player(game);

    game.ready("LeCanardIA");

    hlt::log::log("Bot successfully started!");

    for (;;)
    {
        game.update_frame();

        std::vector<hlt::Command> command_queue = bot_player.step();

        if (!game.end_turn(command_queue))
        {
            break;
        }
    }

    return 0;
}