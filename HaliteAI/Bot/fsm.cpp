#include "fsm.hpp"

FSM_TRANSITION::FSM_TRANSITION()
{
    m_output_state = nullptr;
    m_transition_cbk = nullptr;
}

FSM_TRANSITION::FSM_TRANSITION(TRANSITION_CBK cbk, FSM_STATE *outputState)
{
    m_transition_cbk = cbk;
    m_output_state = outputState;
}

FSM_TRANSITION::~FSM_TRANSITION()
{
}

float FSM_TRANSITION::Evaluate(void *data)
{
    if (m_transition_cbk)
        return m_transition_cbk(data);
    return 0.f;
}

FSM_STATE *FSM_TRANSITION::GetOutputState()
{
    return m_output_state;
}

FSM_STATE::FSM_STATE()
{
    m_behavior_cbk = nullptr;
    m_transitions_count = 0;
    m_sub_fsm = 0;

    m_transitions = nullptr;
}

FSM_STATE::~FSM_STATE()
{
    if (m_transitions)
        delete[] m_transitions;
}

FSM_STATE::FSM_STATE(BEHAVIOR_CBK cbk)
{
    m_behavior_cbk = cbk;
    m_transitions_count = 0;
    m_sub_fsm = 0;
    m_transitions = nullptr;
}

FSM_STATE::FSM_STATE(FSM *subFSM)
{
    m_behavior_cbk = 0;
    m_transitions_count = 0;
    m_sub_fsm = subFSM;
    m_transitions = nullptr;
}

void FSM_STATE::InitTransitions(size_t count, ...)
{
    if (m_transitions_count) // Deja initialise
        return;

    m_transitions_count = count;
    va_list transitionsList;
    va_start(transitionsList, count);
    m_transitions = new FSM_TRANSITION *[m_transitions_count];
    for (size_t iTransition = 0; iTransition < m_transitions_count; ++iTransition)
    {
        m_transitions[iTransition] = va_arg(transitionsList, FSM_TRANSITION *);
    }
    va_end(transitionsList);
}

FSM_STATE *FSM_STATE::Evaluate(void *data)
{
    float bestTransitionScore = 0.f;
    FSM_TRANSITION *bestTransition = 0;
    for (size_t iTransition = 0; iTransition < m_transitions_count; ++iTransition)
    {
        FSM_TRANSITION *transition = m_transitions[iTransition];
        float score = transition->Evaluate(data);
        if (score > bestTransitionScore)
        {
            bestTransitionScore = score;
            bestTransition = transition;
        }
    }
    if (bestTransition)
        return bestTransition->GetOutputState();
    return this; // Pas de transition
}

FSM_STATE *FSM_STATE::Behave(void *data)
{
    if (m_behavior_cbk)
        m_behavior_cbk(data);
    else if (m_sub_fsm)
        return m_sub_fsm->Behave(data);
    return this;
}

void FSM_STATE::Reset()
{
    if (m_sub_fsm)
        m_sub_fsm->Reset();
}

FSM::FSM()
{
    m_current_state = nullptr;
    m_states = nullptr;
    m_states_count = 0;
};

FSM::~FSM()
{
    if (m_states)
        delete[] m_states;
};

FSM::FSM(size_t count, ...)
{
    m_states_count = count;
    m_current_state = 0;
    va_list statesList;
    va_start(statesList, count);
    m_states = new FSM_STATE *[m_states_count];
    for (size_t iState = 0; iState < m_states_count; ++iState)
    {
        m_states[iState] = va_arg(statesList, FSM_STATE *);
    }
    va_end(statesList);
};

FSM_STATE *FSM::Evaluate(void *data)
{
    if (!m_current_state)
    {
        if (!m_states_count)
            return 0;
        m_current_state = m_states[0];
    }
    FSM_STATE *newCurrentState = m_current_state->Evaluate(data);

    if (m_current_state != newCurrentState)
        m_current_state->Reset();
    m_current_state = newCurrentState;

    return m_current_state;
}

void FSM::Reset()
{
    if (m_current_state)
        m_current_state->Reset();
    m_current_state = 0;
}

FSM_STATE *FSM::Behave(void *data)
{
    if (m_current_state)
        return m_current_state->Behave(data);
    return m_current_state;
}