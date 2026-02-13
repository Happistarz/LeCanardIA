#pragma once

/// Structures de requetes et resultats de mouvement.
/// Reutilisable dans tout systeme de gestion de deplacement
/// base sur des priorites avec alternatives.

#include "bot_constants.hpp"
#include "hlt/entity.hpp"
#include "hlt/position.hpp"
#include "hlt/direction.hpp"

#include <vector>

namespace bot
{
    /// Requete de deplacement emise par un ship via sa FSM.
    /// Contient la position souhaitee, la priorite et les alternatives.
    struct MoveRequest
    {
        hlt::EntityId m_ship_id;                    // ID du vaisseau
        hlt::Position m_current;                    // Position actuelle
        hlt::Position m_desired;                    // Position souhaitée
        hlt::Direction m_desired_direction;         // Direction souhaitée
        int m_priority;                             // Priorité de traitement
        std::vector<hlt::Direction> m_alternatives; // Directions secondaires
    };

    /// Resultat final de deplacement apres resolution des conflits.
    struct MoveResult
    {
        hlt::EntityId m_ship_id;
        hlt::Direction m_final_direction;
    };

} // namespace bot
