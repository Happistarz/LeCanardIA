#pragma once

/// Constantes centralisees du bot.
/// Fichier reutilisable : aucune dependance au moteur de jeu (hlt/).
/// Modifier les valeurs ici impacte tout le bot de maniere coherente.

namespace bot
{
    namespace constants
    {
        // ── Ship FSM ──────────────────────────────────────────────
        /// Marge de tours pour le retour urgent (distance + SAFE_RETURN_TURNS)
        /// Inclut une marge pour les embouteillages au depot
        constexpr int SAFE_RETURN_TURNS = 25;
        /// Seuil de remplissage pour declencher le retour (90% du MAX_HALITE)
        constexpr float HALITE_FILL_THRESHOLD = 0.9f;
        /// Seuil en dessous duquel une case est consideree vide (10% du MAX_HALITE)
        constexpr float HALITE_LOW_THRESHOLD = 0.1f;

        // ── Heatmap / Exploration ─────────────────────────────────
        /// Rayon de la heatmap (somme ponderee de halite autour de chaque case)
        constexpr int HEATMAP_RADIUS = 4;
        /// Rayon de recherche de cible d'exploration
        constexpr int EXPLORE_SEARCH_RADIUS = 10;
        /// Halite minimum pour qu'un persistent target reste valide
        constexpr int PERSISTENT_TARGET_MIN_HALITE = 50;

        // ── Spawn ─────────────────────────────────────────────────
        /// Tours minimum restants pour qu'un nouveau ship soit rentable
        constexpr int SPAWN_MIN_TURNS_LEFT = 80;
        /// Nombre maximum de ships (soft cap, adapte par map size)
        constexpr int SPAWN_MAX_SHIPS_BASE = 25;
        /// Halite moyen minimum sur la map pour continuer a spawner
        constexpr int SPAWN_MIN_AVG_HALITE = 100;

        // ── Dropoff ───────────────────────────────────────────────
        /// Nombre maximum de dropoffs a construire
        constexpr int MAX_DROPOFFS = 2;
        /// Nombre minimum de ships avant de planifier un dropoff
        constexpr int MIN_SHIPS_FOR_DROPOFF = 5;
        /// Ratio pour la distance minimale entre depots (map_size / ratio)
        constexpr int MIN_DROPOFF_DEPOT_DISTANCE_RATIO = 4;

        // ── MoveRequest Priority Levels ───────────────────────────
        /// Ship sur un dropoff, doit sortir
        constexpr int SHIP_ON_DROPOFF_PRIORITY = 100;
        /// Retour urgent, distance <= 2 du depot
        constexpr int URGENT_RETURN_NEAR_PRIORITY = 90;
        /// Retour urgent, distance > 2
        constexpr int URGENT_RETURN_PRIORITY = 80;
        /// Fuite danger
        constexpr int FLEE_PRIORITY = 60;
        /// Chasse d'un enemy riche
        constexpr int HUNT_PRIORITY = 55;
        /// Retour normal avec cargo
        constexpr int RETURN_PRIORITY = 50;
        /// Exploration
        constexpr int EXPLORE_PRIORITY = 20;
        /// Collecte (STILL)
        constexpr int COLLECT_PRIORITY = 10;

        // ── Hunt ──────────────────────────────────────────────────
        /// Rayon de detection d'un enemy riche a chasser
        constexpr int HUNT_RADIUS = 6;
        /// Rayon reduit en phase LATE
        constexpr int HUNT_RADIUS_LATE = 3;
        /// Halite max de notre ship pour pouvoir chasser (ship leger)
        constexpr int HUNT_MAX_OWN_HALITE = 200;
        /// Halite min de l'enemy pour valoir la peine
        constexpr int HUNT_MIN_ENEMY_HALITE = 700;
        /// Rayon de detection de defenders autour d'une cible
        constexpr int HUNT_DEFENDER_RADIUS = 2;
        /// Halite max d'un ship ennemi pour etre considere defender
        constexpr int DEFENDER_MAX_HALITE = 200;

        // ── Flee ──────────────────────────────────────────────────
        /// Rayon de detection de menaces pour le flee
        constexpr int FLEE_THREAT_RADIUS = 2;
        /// Cargo minimum pour declencher le flee (on a quelque chose a perdre)
        constexpr int FLEE_MIN_CARGO = 300;

    } // namespace constants
} // namespace bot
