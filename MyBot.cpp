#include "hlt/game.hpp"
#include "HaliteAI/Bot/bot_player.hpp"

int main(int argc, char *argv[])
{
    hlt::Game game;
    game.ready("LeCanardIA");

    bot::BotPlayer player(game);

    for (;;)
    {
        game.update_frame();

        if (!game.end_turn(player.play_turn()))
        {
            break;
        }
    }

    return 0;
}
