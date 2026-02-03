#pragma once

#include "hlt/game.hpp"
#include "hlt/command.hpp" // Important pour hlt::Command
#include <vector>          // Important pour std::vector

namespace bot {

    class BotPlayer {
    public:

        BotPlayer(hlt::Game& game_instance);

        std::vector<hlt::Command> step();

    private:
        hlt::Game& game;
    };
}