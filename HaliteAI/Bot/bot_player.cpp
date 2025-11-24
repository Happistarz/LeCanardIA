#include "bot_player.hpp"
#include "hlt/log.hpp"
#include "hlt/constants.hpp"
#include <algorithm>

namespace bot {
    
    BotPlayer::BotPlayer(hlt::Game& game_instance) : game(game_instance) {
    }
}