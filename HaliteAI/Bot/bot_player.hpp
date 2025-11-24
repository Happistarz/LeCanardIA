#pragma once

#include "hlt/types.hpp"
#include "hlt/game.hpp"
#include "hlt/command.hpp"
#include "hlt/ship.hpp"
#include "hlt/position.hpp"
#include "hlt/direction.hpp"
#include "hlt/constants.hpp"
#include <vector>
#include <memory>

namespace bot {
    class BotPlayer {
    private:
        hlt::Game& game;

    public:
        BotPlayer(hlt::Game& game_instance);
    };
}