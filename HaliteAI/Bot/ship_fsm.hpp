#pragma once

#include "fsm.hpp"
#include "move_request.hpp"
#include "bot_constants.hpp"
#include "hlt/command.hpp"
#include "hlt/entity.hpp"
#include "hlt/game_map.hpp"
#include "hlt/position.hpp"
#include "hlt/ship.hpp"

#include <memory>

namespace bot
{
  struct ShipFSMContext
  {
    std::shared_ptr<hlt::Ship> ship;
    hlt::GameMap *game_map;
    hlt::Position depot_position; // Dropoff ou shipyard le plus proche
    int turns_remaining;
    MoveRequest result_move_request;
  };

  class ShipFSM
  {
  private:
    hlt::EntityId m_ship_id;

    FSM *m_fsm;

    // STATES
    FSM_STATE *m_state_explore;
    FSM_STATE *m_state_collect;
    FSM_STATE *m_state_return;
    FSM_STATE *m_state_urgent_return;
    FSM_STATE *m_state_flee;

    // TRANSITIONS
    FSM_TRANSITION *m_trans_explore_to_return;
    FSM_TRANSITION *m_trans_explore_to_collect;
    FSM_TRANSITION *m_trans_explore_to_urgent;

    FSM_TRANSITION *m_trans_collect_to_return;
    FSM_TRANSITION *m_trans_collect_to_explore;
    FSM_TRANSITION *m_trans_collect_to_urgent;

    FSM_TRANSITION *m_trans_return_to_explore;
    FSM_TRANSITION *m_trans_return_to_urgent;

    FSM_TRANSITION *m_trans_flee_to_explore;
    FSM_TRANSITION *m_trans_flee_to_urgent;

    // TRANSITION CALLBACKS
    static float transition_is_full(void *data);
    static float transition_cell_has_halite(void *data);
    static float transition_cell_empty(void *data);
    static float transition_at_shipyard(void *data);
    static float transition_urgent_return(void *data);
    static float transition_always(void *data);

    // BEHAVIOR CALLBACKS
    static void behavior_explore(void *data);
    static void behavior_collect(void *data);
    static void behavior_return(void *data);
    static void behavior_urgent_return(void *data);

  public:
    explicit ShipFSM(hlt::EntityId ship_id);
    ~ShipFSM();

    MoveRequest update(std::shared_ptr<hlt::Ship> ship,
                        hlt::GameMap &game_map, const hlt::Position &depot_position,
                        int turns_remaining);

    hlt::EntityId get_ship_id() const { return m_ship_id; }
  };
} // namespace bot