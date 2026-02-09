#include "ship_fsm.hpp"
#include "ship_states.hpp"
#include "hlt/constants.hpp"

namespace bot
{
    // Callbacks de transition

    float ShipFSM::transition_is_full(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        return (ctx->ship->halite >= hlt::constants::MAX_HALITE * params::HALITE_FILL_THRESHOLD)
                   ? 1.0f
                   : 0.0f;
    }

    float ShipFSM::transition_cell_has_halite(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        return (ctx->game_map->at(ctx->ship->position)->halite > hlt::constants::MAX_HALITE * params::HALITE_LOW_THRESHOLD)
                   ? 0.5f
                   : 0.0f;
    }

    float ShipFSM::transition_cell_empty(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        return (ctx->game_map->at(ctx->ship->position)->halite < hlt::constants::MAX_HALITE * params::HALITE_LOW_THRESHOLD)
                   ? 0.5f
                   : 0.0f;
    }

    float ShipFSM::transition_at_shipyard(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        return (ctx->ship->position == ctx->shipyard_position) ? 1.0f : 0.0f;
    }

    float ShipFSM::transition_urgent_return(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        int dist = ctx->game_map->calculate_distance(ctx->ship->position, ctx->shipyard_position);
        return (ctx->turns_remaining < dist + params::SAFE_RETURN_TURNS) ? 2.0f : 0.0f;
    }

    float ShipFSM::transition_always(void *data)
    {
        return 1.0f;
    }

    // Callbacks de comportement
    void ShipFSM::behavior_explore(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipExploreState::execute(
            ctx->ship, *ctx->game_map, ctx->shipyard_position);
    }

    void ShipFSM::behavior_collect(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipCollectState::execute(
            ctx->ship, *ctx->game_map, ctx->shipyard_position);
    }

    void ShipFSM::behavior_return(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipReturnState::execute(
            ctx->ship, *ctx->game_map, ctx->shipyard_position);
    }

    void ShipFSM::behavior_urgent_return(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipUrgentReturnState::execute(
            ctx->ship, *ctx->game_map, ctx->shipyard_position);
    }

    // Constructeur / Destructeur
    ShipFSM::ShipFSM(hlt::EntityId ship_id)
        : m_ship_id(ship_id), m_fsm(nullptr)
    {
        // Etats
        m_state_explore = new FSM_STATE(&ShipFSM::behavior_explore);
        m_state_collect = new FSM_STATE(&ShipFSM::behavior_collect);
        m_state_return = new FSM_STATE(&ShipFSM::behavior_return);
        m_state_urgent_return = new FSM_STATE(&ShipFSM::behavior_urgent_return);
        m_state_flee = new FSM_STATE(&ShipFSM::behavior_explore);

        // Transitions
        m_trans_explore_to_return = new FSM_TRANSITION(&ShipFSM::transition_is_full, m_state_return);
        m_trans_explore_to_collect = new FSM_TRANSITION(&ShipFSM::transition_cell_has_halite, m_state_collect);
        m_trans_explore_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);

        m_trans_collect_to_return = new FSM_TRANSITION(&ShipFSM::transition_is_full, m_state_return);
        m_trans_collect_to_explore = new FSM_TRANSITION(&ShipFSM::transition_cell_empty, m_state_explore);
        m_trans_collect_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);

        m_trans_return_to_explore = new FSM_TRANSITION(&ShipFSM::transition_at_shipyard, m_state_explore);
        m_trans_return_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);

        m_trans_flee_to_explore = new FSM_TRANSITION(&ShipFSM::transition_always, m_state_explore);
        m_trans_flee_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);

        // Cablage des transitions
        m_state_explore->InitTransitions(3,
                                         m_trans_explore_to_urgent,
                                         m_trans_explore_to_return,
                                         m_trans_explore_to_collect);

        m_state_collect->InitTransitions(3,
                                         m_trans_collect_to_urgent,
                                         m_trans_collect_to_return,
                                         m_trans_collect_to_explore);

        m_state_return->InitTransitions(2,
                                        m_trans_return_to_urgent,
                                        m_trans_return_to_explore);

        m_state_urgent_return->InitTransitions(1,
                                               m_trans_return_to_explore);

        m_state_flee->InitTransitions(2,
                                      m_trans_flee_to_urgent,
                                      m_trans_flee_to_explore);

        // FSM principal
        m_fsm = new FSM(5,
                        m_state_explore, m_state_collect, m_state_return,
                        m_state_urgent_return, m_state_flee);
    }

    ShipFSM::~ShipFSM()
    {
        delete m_fsm;

        delete m_state_explore;
        delete m_state_collect;
        delete m_state_return;
        delete m_state_urgent_return;
        delete m_state_flee;

        delete m_trans_explore_to_return;
        delete m_trans_explore_to_collect;
        delete m_trans_explore_to_urgent;
        delete m_trans_collect_to_return;
        delete m_trans_collect_to_explore;
        delete m_trans_collect_to_urgent;
        delete m_trans_return_to_explore;
        delete m_trans_return_to_urgent;
        delete m_trans_flee_to_explore;
        delete m_trans_flee_to_urgent;
    }

    // Tick public
    MoveRequest ShipFSM::update(std::shared_ptr<hlt::Ship> ship, hlt::GameMap &game_map,
                                const hlt::Position &shipyard_position, int turns_remaining)
    {
        ShipFSMContext context;
        context.ship = ship;
        context.game_map = &game_map;
        context.shipyard_position = shipyard_position;
        context.turns_remaining = turns_remaining;
        context.result_move_request = MoveRequest{};

        m_fsm->Evaluate(&context);
        m_fsm->Behave(&context);

        return context.result_move_request;
    }

} // namespace bot