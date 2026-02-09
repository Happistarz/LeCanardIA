#pragma once

#include <cstdarg>

class FSM_STATE;

// Transition entre deux etats du FSM
class FSM_TRANSITION
{

public:
    typedef float (*TRANSITION_CBK)(void *data);

private:
    TRANSITION_CBK m_transition_cbk; // Callback d'evaluation
    FSM_STATE *m_output_state;       // Etat cible si la transition est choisie
    FSM_TRANSITION();

public:
    FSM_TRANSITION(TRANSITION_CBK cbk, FSM_STATE *outputState);

    ~FSM_TRANSITION();

    // Evalue la transition et retourne un score
    float Evaluate(void *data);

    // Retourne l'etat cible de cette transition
    FSM_STATE *GetOutputState();
};

class FSM;

// Etat du FSM avec comportement et transitions sortantes
class FSM_STATE
{

public:
    typedef void (*BEHAVIOR_CBK)(void *data);

private:
    BEHAVIOR_CBK m_behavior_cbk;       // Callback de comportement
    FSM *m_sub_fsm;                    // Sous-FSM optionnel
    FSM_TRANSITION **m_transitions;    // Tableau de transitions sortantes
    size_t m_transitions_count;

    FSM_STATE();

public:
    ~FSM_STATE();

    FSM_STATE(BEHAVIOR_CBK cbk);
    FSM_STATE(FSM *subFSM);

    // Initialise les transitions sortantes de cet etat
    void InitTransitions(size_t count, ...);

    // Evalue les transitions et retourne le meilleur etat suivant
    FSM_STATE *Evaluate(void *data);

    // Execute le comportement de cet etat
    FSM_STATE *Behave(void *data);

    void Reset();
};

// Machine a etats finis generique
class FSM
{

    FSM_STATE **m_states;       // Tableau des etats
    FSM_STATE *m_current_state; // Etat courant
    size_t m_states_count;

    FSM();

public:
    ~FSM();

    FSM(size_t count, ...);

    // Evalue les transitions depuis l'etat courant
    FSM_STATE *Evaluate(void *data);

    void Reset();

    // Execute le comportement de l'etat courant
    FSM_STATE *Behave(void *data);
};