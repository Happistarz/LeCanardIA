#include "ship_fsm.hpp"
#include "ship_states.hpp"
#include "blackboard.hpp"
#include "hlt/constants.hpp"

namespace bot
{
    // TRANSITION CALLBACKS

    float ShipFSM::transition_is_full(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);

        // Classique : ship plein a 90%
        if (ctx->ship->halite >= hlt::constants::MAX_HALITE * constants::HALITE_FILL_THRESHOLD)
            return 1.0f;

        // Si le rendement du trajet retour (cargo / dist) est meilleur que 2x le rendement moyen d'extraction par tour, return
        const Blackboard &bb = Blackboard::get_instance();

        // Distance au dropoff
        int dist = ctx->game_map->calculate_distance(ctx->ship->position, ctx->drop_position);
        if (dist <= 0 || ctx->ship->halite <= 0)
            return 0.0f;

        // Halite par tour en retournant maintenant
        int halite_per_turn_returning = ctx->ship->halite / dist;
        int avg_mining_yield = bb.average_halite / hlt::constants::EXTRACT_RATIO;

        if (halite_per_turn_returning > avg_mining_yield * 2)
            return 1.0f;

        return 0.0f;
    }

    // Si on est proche du dropoff et qu'on a un bon cargo, on peut retourner vite
    float ShipFSM::transition_close_and_loaded(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);

        int dist = ctx->game_map->calculate_distance(ctx->ship->position, ctx->drop_position);

        if (dist <= constants::SMART_RETURN_MAX_DIST &&
            ctx->ship->halite >= hlt::constants::MAX_HALITE * constants::SMART_RETURN_CARGO_RATIO)
            return 0.6f;

        return 0.0f;
    }

    // Si la cell a du halite, c'est qu'elle n'est pas encore epuisee, on peut rester miner
    float ShipFSM::transition_cell_has_halite(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        const Blackboard &bb = Blackboard::get_instance();

        int cell_halite = ctx->game_map->at(ctx->ship->position)->halite;
        int extract_ratio = hlt::constants::EXTRACT_RATIO;
        bool inspired = bb.inspired_zones.find(ctx->game_map->normalize(ctx->ship->position)) != bb.inspired_zones.end();

        if (inspired)
            extract_ratio = hlt::constants::INSPIRED_EXTRACT_RATIO;

        // Marginal = halite qu'on extrairait ce tour
        int marginal = cell_halite / extract_ratio;
        if (inspired)
            marginal += static_cast<int>(marginal * hlt::constants::INSPIRED_BONUS_MULTIPLIER);

        // Rendement moyen par tour d'un trip complet (6 tours : aller, miner, retour)
        int avg_yield = (bb.average_halite > 0 ? bb.average_halite : 1) / 6;

        // Cell encore rentable, on peut rester
        if (marginal > avg_yield)
            return 0.5f;

        return 0.0f;
    }

    // Inverse de transition_cell_has_halite : cell epuisee, faut bouger
    float ShipFSM::transition_cell_empty(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        const Blackboard &bb = Blackboard::get_instance();

        int cell_halite = ctx->game_map->at(ctx->ship->position)->halite;
        int extract_ratio = hlt::constants::EXTRACT_RATIO;

        bool inspired = bb.inspired_zones.find(ctx->game_map->normalize(ctx->ship->position)) != bb.inspired_zones.end();
        if (inspired)
            extract_ratio = hlt::constants::INSPIRED_EXTRACT_RATIO;

        int marginal = cell_halite / extract_ratio;
        if (inspired)
            marginal += static_cast<int>(marginal * hlt::constants::INSPIRED_BONUS_MULTIPLIER);

        int avg_yield = (bb.average_halite > 0 ? bb.average_halite : 1) / 6;

        // Marginal trop faible, cell epuisee, faut partir
        if (marginal < avg_yield)
            return 0.5f;

        return 0.0f;
    }

    // Si on est sur le dropoff, on peut switch de suite en explore pour repartir miner
    float ShipFSM::transition_at_shipyard(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        if (ctx->ship->position == ctx->drop_position)
            return 1.0f;

        return 0.0f;
    }

    // Si on est a moins de SAFE_RETURN_TURNS du game end, il faut retourner absolument
    float ShipFSM::transition_urgent_return(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        int dist = ctx->game_map->calculate_distance(ctx->ship->position, ctx->drop_position);

        if (ctx->turns_remaining < dist + constants::SAFE_RETURN_TURNS)
            return 2.0f;

        return 0.0f;
    }

    // Transition toujours valide
    float ShipFSM::transition_always(void *data)
    {
        return 1.0f;
    }

    // Si on est en phase de chasse et qu'on a une cible valide a portee, switch en hunt
    float ShipFSM::transition_should_hunt(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        const Blackboard &bb = Blackboard::get_instance();

        // Pas de chasse en ENDGAME
        if (bb.current_phase == GamePhase::ENDGAME)
            return 0.0f;

        // Notre ship doit etre leger
        if (ctx->ship->halite > constants::HUNT_MAX_OWN_HALITE)
            return 0.0f;

        int search_radius = (bb.current_phase == GamePhase::LATE)
                                ? constants::HUNT_RADIUS_LATE
                                : constants::HUNT_RADIUS;

        // Chercher un enemy plein a portee
        for (const auto &enemy : bb.enemy_ships)
        {
            if (enemy.halite < constants::HUNT_MIN_ENEMY_HALITE)
                continue;

            int dist = ctx->game_map->calculate_distance(ctx->ship->position, enemy.position);

            if (dist <= search_radius)
                return 0.8f;
        }

        return 0.0f;
    }

    // Si on est en chasse mais qu'on n'a plus de cible valide, retourner en explore
    float ShipFSM::transition_should_flee(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        const Blackboard &bb = Blackboard::get_instance();

        if (bb.has_nearby_threat(*ctx->game_map, ctx->ship->position, ctx->ship->halite))
            return 1.2f;

        return 0.0f;
    }

    // Si on est en chasse mais qu'on a plus de cible valide, retourner en explore
    float ShipFSM::transition_no_hunt_target(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        const Blackboard &bb = Blackboard::get_instance();

        // Si plus de target valide, return to explore
        auto ht_it = bb.hunt_targets.find(ctx->ship->id);
        if (ht_it == bb.hunt_targets.end())
            return 0.5f;

        // Verif si la target existe encore et est toujours plein
        for (const auto &enemy : bb.enemy_ships)
        {
            if (enemy.id == ht_it->second && enemy.halite >= constants::HUNT_MIN_ENEMY_HALITE / 2)
                return 0.0f; // target encore valide
        }

        return 0.5f; // target perdue
    }

    // Si on est en flee mais qu'il y a plus de menace proche, on peut retourner en explore
    float ShipFSM::transition_no_threat(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        const Blackboard &bb = Blackboard::get_instance();

        if (!bb.has_nearby_threat(*ctx->game_map, ctx->ship->position, ctx->ship->halite))
            return 0.5f;

        return 0.0f;
    }

    void ShipFSM::behavior_explore(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipExploreState::execute(ctx->ship, *ctx->game_map, ctx->drop_position);
    }

    void ShipFSM::behavior_collect(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipCollectState::execute(ctx->ship, *ctx->game_map, ctx->drop_position);
    }

    void ShipFSM::behavior_return(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipReturnState::execute(ctx->ship, *ctx->game_map, ctx->drop_position);
    }

    void ShipFSM::behavior_urgent_return(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipUrgentReturnState::execute(ctx->ship, *ctx->game_map, ctx->drop_position);
    }

    void ShipFSM::behavior_flee(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipFleeState::execute(ctx->ship, *ctx->game_map, ctx->drop_position);
    }

    void ShipFSM::behavior_hunt(void *data)
    {
        auto *ctx = static_cast<ShipFSMContext *>(data);
        ctx->result_move_request = ShipHuntState::execute(ctx->ship, *ctx->game_map, ctx->drop_position);
    }

    ShipFSM::ShipFSM(hlt::EntityId ship_id)
        : m_ship_id(ship_id), m_fsm(nullptr)
    {
        m_state_explore = new FSM_STATE(&ShipFSM::behavior_explore);
        m_state_collect = new FSM_STATE(&ShipFSM::behavior_collect);
        m_state_return = new FSM_STATE(&ShipFSM::behavior_return);
        m_state_urgent_return = new FSM_STATE(&ShipFSM::behavior_urgent_return);
        m_state_flee = new FSM_STATE(&ShipFSM::behavior_flee);
        m_state_hunt = new FSM_STATE(&ShipFSM::behavior_hunt);

        // Transitions depuis EXPLORE
        m_trans_explore_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);
        m_trans_explore_to_flee = new FSM_TRANSITION(&ShipFSM::transition_should_flee, m_state_flee);
        m_trans_explore_to_return = new FSM_TRANSITION(&ShipFSM::transition_is_full, m_state_return);
        m_trans_explore_to_close_return = new FSM_TRANSITION(&ShipFSM::transition_close_and_loaded, m_state_return);
        m_trans_explore_to_hunt = new FSM_TRANSITION(&ShipFSM::transition_should_hunt, m_state_hunt);
        m_trans_explore_to_collect = new FSM_TRANSITION(&ShipFSM::transition_cell_has_halite, m_state_collect);

        // Transitions depuis COLLECT
        m_trans_collect_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);
        m_trans_collect_to_flee = new FSM_TRANSITION(&ShipFSM::transition_should_flee, m_state_flee);
        m_trans_collect_to_return = new FSM_TRANSITION(&ShipFSM::transition_is_full, m_state_return);
        m_trans_collect_to_close_return = new FSM_TRANSITION(&ShipFSM::transition_close_and_loaded, m_state_return);
        m_trans_collect_to_hunt = new FSM_TRANSITION(&ShipFSM::transition_should_hunt, m_state_hunt);
        m_trans_collect_to_explore = new FSM_TRANSITION(&ShipFSM::transition_cell_empty, m_state_explore);

        // Transitions depuis RETURN
        m_trans_return_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);
        m_trans_return_to_explore = new FSM_TRANSITION(&ShipFSM::transition_at_shipyard, m_state_explore);

        // Transitions depuis FLEE
        m_trans_flee_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);
        m_trans_flee_to_explore = new FSM_TRANSITION(&ShipFSM::transition_no_threat, m_state_explore);

        // Transitions depuis HUNT
        m_trans_hunt_to_urgent = new FSM_TRANSITION(&ShipFSM::transition_urgent_return, m_state_urgent_return);
        m_trans_hunt_to_flee = new FSM_TRANSITION(&ShipFSM::transition_should_flee, m_state_flee);
        m_trans_hunt_to_explore = new FSM_TRANSITION(&ShipFSM::transition_no_hunt_target, m_state_explore);

        // Initialisation des transitions dans les etats
        m_state_explore->InitTransitions(6,
                                         m_trans_explore_to_urgent,
                                         m_trans_explore_to_flee,
                                         m_trans_explore_to_return,
                                         m_trans_explore_to_close_return,
                                         m_trans_explore_to_hunt,
                                         m_trans_explore_to_collect);

        m_state_collect->InitTransitions(6,
                                         m_trans_collect_to_urgent,
                                         m_trans_collect_to_flee,
                                         m_trans_collect_to_return,
                                         m_trans_collect_to_close_return,
                                         m_trans_collect_to_hunt,
                                         m_trans_collect_to_explore);

        m_state_return->InitTransitions(2,
                                        m_trans_return_to_urgent,
                                        m_trans_return_to_explore);

        m_state_urgent_return->InitTransitions(1,
                                               m_trans_return_to_explore);

        m_state_flee->InitTransitions(2,
                                      m_trans_flee_to_urgent,
                                      m_trans_flee_to_explore);

        m_state_hunt->InitTransitions(3,
                                      m_trans_hunt_to_urgent,
                                      m_trans_hunt_to_flee,
                                      m_trans_hunt_to_explore);

        m_fsm = new FSM(6,
                        m_state_explore,
                        m_state_collect,
                        m_state_return,
                        m_state_urgent_return,
                        m_state_flee,
                        m_state_hunt);
    }

    ShipFSM::~ShipFSM()
    {
        delete m_fsm;

        delete m_state_explore;
        delete m_state_collect;
        delete m_state_return;
        delete m_state_urgent_return;
        delete m_state_flee;
        delete m_state_hunt;

        delete m_trans_explore_to_return;
        delete m_trans_explore_to_close_return;
        delete m_trans_explore_to_collect;
        delete m_trans_explore_to_urgent;
        delete m_trans_explore_to_hunt;
        delete m_trans_explore_to_flee;

        delete m_trans_collect_to_return;
        delete m_trans_collect_to_close_return;
        delete m_trans_collect_to_explore;
        delete m_trans_collect_to_urgent;
        delete m_trans_collect_to_hunt;
        delete m_trans_collect_to_flee;

        delete m_trans_return_to_explore;
        delete m_trans_return_to_urgent;

        delete m_trans_flee_to_explore;
        delete m_trans_flee_to_urgent;

        delete m_trans_hunt_to_explore;
        delete m_trans_hunt_to_urgent;
        delete m_trans_hunt_to_flee;
    }

    // Update le FSM et execute le behavior du current state, retourne le MoveRequest genere
    MoveRequest ShipFSM::update(std::shared_ptr<hlt::Ship> ship, hlt::GameMap &game_map,
                                const hlt::Position &depot_position, int turns_remaining)
    {
        ShipFSMContext context;
        context.ship = ship;
        context.game_map = &game_map;
        context.drop_position = depot_position;
        context.turns_remaining = turns_remaining;
        context.result_move_request = MoveRequest{};

        m_fsm->Evaluate(&context);

        m_fsm->Behave(&context);

        return context.result_move_request;
    }
} // namespace bot