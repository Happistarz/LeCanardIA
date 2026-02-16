#pragma once

namespace bot
{
    namespace constants
    {
        // SHIP FSM

        /// Marge de turns pour le return des ships, + marge pour le trafic
        constexpr int SAFE_RETURN_TURNS = 25;
        /// Seuil max du cargo, 90% du MAX_HALITE
        constexpr float HALITE_FILL_THRESHOLD = 0.9f;
        /// Seuil en min pour ne pas la considerer comme cell vide
        constexpr float HALITE_LOW_THRESHOLD = 0.1f;

        // HEATMAP / EXPLORE

        /// Rayon de la heatmap
        constexpr int HEATMAP_RADIUS = 4;
        /// Rayon de recherche pour l'explore
        constexpr int EXPLORE_SEARCH_RADIUS = 10;
        /// Halite min pour qu'un target reste valide
        constexpr int TARGET_MIN_HALITE = 50;

        // SPAWN

        /// Turns min restants pour qu'un nouveau ship soit rentable
        constexpr int SPAWN_MIN_TURNS_LEFT = 80;
        /// Nombre max de ships (adaptable selon la taille de la map)
        constexpr int SPAWN_MAX_SHIPS_BASE = 25;
        /// Halite moyen min sur la map pour continuer a spawner
        constexpr int SPAWN_MIN_AVG_HALITE = 100;

        // DROPOFF

        /// Nombre max de dropoffs
        constexpr int MAX_DROPOFFS = 2;
        /// Nombre min de ships avant de planifier un dropoff
        constexpr int MIN_SHIPS_FOR_DROPOFF = 5;
        /// Ratio distance depot / distance spawn pour considerer un dropoff rentable
        constexpr int MIN_DROPOFF_DEPOT_DISTANCE_RATIO = 4;

        // MOVE REQUEST PRIORITY LEVELS

        /// Ship sur un dropoff, doit sortir
        constexpr int SHIP_ON_DROPOFF_PRIORITY = 100;
        /// Retour urgent, distance <= 2 du depot
        constexpr int URGENT_RETURN_NEAR_PRIORITY = 90;
        /// Retour urgent, distance > 2
        constexpr int URGENT_RETURN_PRIORITY = 80;
        /// Fuite du danger
        constexpr int FLEE_PRIORITY = 60;
        /// Chasse d'un enemy plein
        constexpr int HUNT_PRIORITY = 55;
        /// Retour normal avec cargo
        constexpr int RETURN_PRIORITY = 50;
        /// Exploration
        constexpr int EXPLORE_PRIORITY = 20;
        /// Collecte
        constexpr int COLLECT_PRIORITY = 10;

        // HUNT

        /// Rayon de detection d'un enemy plein a chasser
        constexpr int HUNT_RADIUS = 6;
        /// Rayon reduit en phase LATE
        constexpr int HUNT_RADIUS_LATE = 3;
        /// Halite max de notre ship pour pouvoir chasser (HIGH CARGO -> NO RISK)
        constexpr int HUNT_MAX_OWN_HALITE = 200;
        /// Halite min de l'enemy pour etre considere comme target de chasse
        constexpr int HUNT_MIN_ENEMY_HALITE = 700;
        /// Rayon de detection de defenders autour de la target
        constexpr int HUNT_DEFENDER_RADIUS = 2;
        /// Halite max d'un ship ennemi pour etre considere defender
        constexpr int DEFENDER_MAX_HALITE = 200;

        // FLEE

        /// Rayon de detection de menaces pour le flee
        constexpr int FLEE_THREAT_RADIUS = 2;
        /// Cargo min pour declencher le flee (LOW CARGO -> NO RISK)
        constexpr int FLEE_MIN_CARGO = 300;

    } // namespace constants
} // namespace bot
