#pragma once

namespace bot
{
    namespace params
    {
        // Economie
        constexpr int SHIP_COST = 1000;
        constexpr int DROPOFF_COST = 4000;
        constexpr int MIN_HALITE_TO_SPAWN = 50; // Moyenne min de la carte pour spawn

        // Construction de dropoff
        constexpr int DROPOFF_MIN_DISTANCE = 7;  // Distance min du shipyard et des autres dropoffs
        constexpr int DROPOFF_SPOT_HALITE = 700;  // Richesse min de la cell
        constexpr int MIN_SHIPS_FOR_DROPOFF = 10; // Taille de flotte requise

        // Defense
        constexpr int DANGER_ZONE_RADIUS = 10;     // Rayon de detection ennemi
        constexpr int DEFENSE_INTERCEPT_DIST = 15; // Rayon d'action du defenseur
        constexpr int DEFENSE_HALITE_LIMIT = 500;  // Les ships riches ne defendent pas

        // Logistique
        constexpr double RETURN_RATIO = 0.90; // Retour a ce ratio de remplissage

        // Escouade / Pillage
        constexpr int SQUAD_MIN_FLEET_SIZE = 8; // Flotte min pour former une escouade
        constexpr int LOOT_DIST_THRESHOLD = 20;  // Distance cluster pour mission pillage
        constexpr int BUDDY_MAX_DIST = 10;       // Portee max pour recruter un ally

        // Seuils de phase (fraction de MAX_TURNS)
        constexpr float PHASE_ENDGAME = 0.98f;
        constexpr float PHASE_LATE = 0.60f;
        constexpr float PHASE_MID = 0.30f;

        // Seuils micro FSM
        constexpr float HALITE_FILL_THRESHOLD = 0.9f; // Ship considere plein
        constexpr float HALITE_LOW_THRESHOLD = 0.1f;  // Cell considere vide
        constexpr int SAFE_RETURN_TURNS = 15;         // Marge pour retour urgent

        // Analyse de cluster
        constexpr int CLUSTER_RADIUS = 1; // Rayon du scan en croix

    } // namespace params
} // namespace bot