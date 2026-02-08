#pragma once

namespace bot {
    namespace params {
        // --- ÉCONOMIE ---
        constexpr int SHIP_COST = 1000;
        constexpr int DROPOFF_COST = 4000;
        constexpr int MIN_HALITE_TO_SPAWN = 50; // Moyenne carte min pour spawn

        // --- CONSTRUCTION (DROPOFF) ---
        constexpr int DROPOFF_MIN_DISTANCE = 15; // Distance min de la base
        constexpr int DROPOFF_SPOT_HALITE = 700; // Richesse case pour construire
        constexpr int MIN_SHIPS_FOR_DROPOFF = 10; // Pas de dropoff si on a une petite flotte

        // --- DÉFENSE ---
        constexpr int DANGER_ZONE_RADIUS = 10;   // Distance détection ennemi
        constexpr int DEFENSE_INTERCEPT_DIST = 15; // Rayon d'action des défenseurs
        constexpr int DEFENSE_HALITE_LIMIT = 500; // Un vaisseau riche ne défend pas

        // --- LOGISTIQUE ---
        constexpr double RETURN_RATIO = 0.90; // Rentre à 90% plein

        // --- ESCOUADE (SQUAD) ---
        constexpr int SQUAD_MIN_FLEET_SIZE = 15; // Pas d'escouade si < 15 vaisseaux
        constexpr int LOOT_DIST_THRESHOLD = 20;  // Si cluster > 20, c'est du loot
        constexpr int BUDDY_MAX_DIST = 10;       // Cherche un copain à moins de 10 cases
    }
}