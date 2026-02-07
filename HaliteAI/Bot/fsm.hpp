#pragma once

#include <cstdarg>

class FSM_STATE;

class FSM_TRANSITION
{

public:
    typedef float (*TRANSITION_CBK)(void *data);

private:
    TRANSITION_CBK m_transition_cbk;
    FSM_STATE *m_output_state;
    FSM_TRANSITION();

public:
    FSM_TRANSITION(TRANSITION_CBK cbk, FSM_STATE *outputState);

    ~FSM_TRANSITION();

    float Evaluate(void *data);

    FSM_STATE *GetOutputState();
};

class FSM;

class FSM_STATE
{

public:
    typedef void (*BEHAVIOR_CBK)(void *data);

private:
    BEHAVIOR_CBK m_behavior_cbk;
    FSM *m_sub_fsm;
    FSM_TRANSITION **m_transitions;
    size_t m_transitions_count;

    FSM_STATE();

public:
    ~FSM_STATE();

    FSM_STATE(BEHAVIOR_CBK cbk);
    FSM_STATE(FSM *subFSM);

    void InitTransitions(size_t count, ...);

    FSM_STATE *Evaluate(void *data);

    FSM_STATE *Behave(void *data);

    void Reset();
};

class FSM
{

    FSM_STATE **m_states;
    FSM_STATE *m_current_state;
    size_t m_states_count;

    FSM();

public:
    ~FSM();

    FSM(size_t count, ...);

    FSM_STATE *Evaluate(void *data);

    void Reset();

    FSM_STATE *Behave(void *data);
};