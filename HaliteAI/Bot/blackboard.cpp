#include "blackboard.hpp"
#include "bot_parameters.hpp"
#include "hlt/constants.hpp"

namespace bot
{

    void Blackboard::init(int width, int height)
    {
        map_width = width;
        map_height = height;
    }

    void Blackboard::assign_mission(hlt::EntityId id, MissionType mission, hlt::Position target)
    {
        ship_missions[id] = mission;
        ship_targets[id] = target;
    }

    // Metriques

    void Blackboard::update_metrics(hlt::Game &game)
    {
        long sum = 0;
        for (int y = 0; y < game.game_map->height; ++y)
            for (int x = 0; x < game.game_map->width; ++x)
                sum += game.game_map->cells[y][x].halite;

        total_halite = sum;
        average_halite = static_cast<int>(sum / (game.game_map->width * game.game_map->height));
    }

    void Blackboard::update_phase(int turn, int total_turns)
    {
        current_turn = turn;
        max_turns = total_turns;
        float progress = static_cast<float>(turn) / static_cast<float>(total_turns);

        if (progress > params::PHASE_ENDGAME)
            current_phase = GamePhase::ENDGAME;
        else if (progress > params::PHASE_LATE)
            current_phase = GamePhase::LATE;
        else if (progress > params::PHASE_MID)
            current_phase = GamePhase::MID;
        else
            current_phase = GamePhase::EARLY;
    }

    // Decision de spawn

    bool Blackboard::should_spawn(const hlt::Player &me)
    {
        if (me.halite < hlt::constants::SHIP_COST)
            return false;
        if (is_position_reserved(me.shipyard->position))
            return false;
        if (current_phase == GamePhase::ENDGAME ||
            current_phase == GamePhase::LATE)
            return false;
        if (average_halite < params::MIN_HALITE_TO_SPAWN)
            return false;
        return true;
    }

    // Analyse de cluster

    void Blackboard::update_best_cluster(hlt::Game &game)
    {
        long max_score = -1;
        hlt::Position best_pos = {0, 0};
        int w = game.game_map->width;
        int h = game.game_map->height;

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                // Somme en croix (centre + 4 cardinaux)
                long score = game.game_map->at({x, y})->halite
                            + game.game_map->at({x, (y + 1) % h})->halite
                            + game.game_map->at({(x + 1) % w, y})->halite
                            + game.game_map->at({(x - 1 + w) % w, y})->halite
                            + game.game_map->at({x, (y - 1 + h) % h})->halite;

                if (score > max_score)
                {
                    max_score = score;
                    best_pos = {x, y};
                }
            }
        }
        best_cluster_position = best_pos;
    }

    // Requetes spatiales

    bool Blackboard::is_position_safe(const hlt::Position &pos) const
    {
        return reserved_positions.count(pos) == 0 && danger_zones.count(pos) == 0;
    }

    bool Blackboard::is_position_reserved(const hlt::Position &pos) const
    {
        return reserved_positions.count(pos) > 0;
    }

    bool Blackboard::is_position_stuck(const hlt::Position &pos) const
    {
        return stuck_positions.count(pos) > 0;
    }

    void Blackboard::reserve_position(const hlt::Position &pos, hlt::EntityId ship_id)
    {
        reserved_positions.insert(pos);
        targeted_cells[pos] = ship_id;
    }

    void Blackboard::clear_turn_data()
    {
        reserved_positions.clear();
        targeted_cells.clear();
        danger_zones.clear();
        stuck_positions.clear();
    }

} // namespace bot