#pragma once

#include "hlt/direction.hpp"
#include "hlt/entity.hpp"
#include "hlt/game_map.hpp"
#include "hlt/position.hpp"
#include "hlt/ship.hpp"
#include "hlt/constants.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace bot
{
    enum class MissionType
    {
        MINING,       // Defaut : recolter les clusters d'halite
        RETURNING,    // Cargo pleine, retour au depot
        CONSTRUCTING, // Construction d'un nouveau depot
        ATTACKING,    // Attaque de ships ennemis
        DEFENDING,    // Interception de menaces pres de la base
        LOOTING       // Pillage de clusters distants en escouade
    };

    enum class GamePhase
    {
        EARLY,  // Ouverture : spawn agressif
        MID,    // Milieu : expansion et controle de carte
        LATE,   // Fin : formation d'escouade, offensif
        ENDGAME // Rush final : tous les ships rentrent
    };

    struct PositionComparator
    {
        bool operator()(const hlt::Position &a, const hlt::Position &b) const
        {
            if (a.y != b.y)
                return a.y < b.y;
            return a.x < b.x;
        }
    };

    // Cout en halite pour qu'un ship quitte sa cell
    inline int move_cost(const int cell_halite)
    {
        return cell_halite / hlt::constants::MOVE_COST_RATIO;
    }

    // Verifie si un ship n'a pas assez d'halite pour bouger
    inline bool is_ship_stuck(const hlt::Ship &ship, hlt::GameMap &game_map)
    {
        const int cell_halite = game_map.at(ship.position)->halite;
        return ship.halite < move_cost(cell_halite);
    }

    // Direction cardinale avec score pour tri / selection
    struct ScoredDirection
    {
        hlt::Direction dir;
        int score;       // Score generique (halite, distance negative, etc.)
        bool is_stuck;   // cell cible bloquee par un ship immobile
        bool is_optimal; // Fait partie de get_unsafe_moves()
    };

    // Tri des directions : meilleur score d'abord, non-bloque prefere, optimal prefere
    inline void sort_scored_directions(std::vector<ScoredDirection> &dirs)
    {
        std::sort(dirs.begin(), dirs.end(),
                  [](const ScoredDirection &a, const ScoredDirection &b)
                  {
                      if (a.is_stuck != b.is_stuck)
                          return !a.is_stuck;
                      if (a.is_optimal != b.is_optimal)
                          return a.is_optimal;
                      return a.score > b.score; // score plus haut en premier
                  });
    }

    // Score les directions cardinales par halite presente, du plus riche au plus pauvre.
    inline std::vector<ScoredDirection> score_directions_by_halite(
        const hlt::Position &origin,
        hlt::GameMap &game_map)
    {
        std::vector<ScoredDirection> dirs;
        dirs.reserve(4);
        for (const auto &d : hlt::ALL_CARDINALS)
        {
            hlt::Position target = game_map.normalize(origin.directional_offset(d));
            int halite = game_map.at(target)->halite;
            dirs.push_back({d, halite, false, false});
        }
        return dirs;
    }

    // Extraction de la meilleure direction et des alternatives a partir d'une liste triee
    inline void extract_best_and_alternatives(
        const std::vector<ScoredDirection> &sorted,
        hlt::Direction &out_best,
        std::vector<hlt::Direction> &out_alternatives)
    {
        out_best = sorted[0].dir;
        out_alternatives.clear();
        out_alternatives.reserve(sorted.size() - 1);
        for (size_t i = 1; i < sorted.size(); ++i)
            out_alternatives.push_back(sorted[i].dir);
    }

    // Type de fonction pour verifier si une position est stuck
    using StuckPositionQuery = bool (*)(const hlt::Position &);

    // Score les directions cardinales vers une destination, en preferant les moves optimaux de get_unsafe_moves() et en penalisant les positions stuck.
    inline std::vector<ScoredDirection> score_directions_toward(
        const hlt::Position &origin,
        const hlt::Position &destination,
        hlt::GameMap &game_map,
        StuckPositionQuery is_stuck_fn = nullptr)
    {
        std::vector<hlt::Direction> unsafe = game_map.get_unsafe_moves(origin, destination);

        std::vector<ScoredDirection> dirs;
        dirs.reserve(4);
        for (const auto &d : hlt::ALL_CARDINALS)
        {
            hlt::Position target = game_map.normalize(origin.directional_offset(d));
            int dist = game_map.calculate_distance(target, destination);
            bool stuck = is_stuck_fn ? is_stuck_fn(target) : false;
            bool optimal = false;
            for (const auto &um : unsafe)
                if (um == d)
                {
                    optimal = true;
                    break;
                }

            dirs.push_back({d, -dist, stuck, optimal}); // distance plus petite = meilleur score, mais score negatif pour tri decroissant
        }

        sort_scored_directions(dirs);
        return dirs;
    }

    // Navigue de origin vers destination en preferant les moves optimaux et en evitant les positions stuck si possible. Retourne la direction choisie et les alternatives.
    inline void navigate_toward(
        const hlt::Position &origin,
        const hlt::Position &destination,
        hlt::GameMap &game_map,
        hlt::Direction &out_best,
        std::vector<hlt::Direction> &out_alternatives,
        StuckPositionQuery is_stuck_fn = nullptr)
    {
        if (origin == destination)
        {
            out_best = hlt::Direction::STILL;
            out_alternatives.assign(hlt::ALL_CARDINALS.begin(), hlt::ALL_CARDINALS.end());
            return;
        }

        auto scored = score_directions_toward(origin, destination, game_map, is_stuck_fn);
        extract_best_and_alternatives(scored, out_best, out_alternatives);
    }

    struct MoveRequest;

    // Construit un MoveRequest pour rester STILL
    MoveRequest make_still_request(hlt::EntityId ship_id, const hlt::Position &pos, int priority);

    // Construit un MoveRequest pour naviguer vers une destination, avec gestion des positions stuck
    MoveRequest make_move_request(
        std::shared_ptr<hlt::Ship> ship,
        const hlt::Position &target,
        int priority,
        hlt::GameMap &game_map,
        StuckPositionQuery is_stuck_fn = nullptr);

} // namespace bot
