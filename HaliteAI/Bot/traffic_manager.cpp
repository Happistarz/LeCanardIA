#include "traffic_manager.hpp"
#include "bot_constants.hpp"
#include "hlt/constants.hpp"

#include <algorithm>
#include <unordered_map>

namespace bot
{
    TrafficManager &TrafficManager::instance()
    {
        static TrafficManager inst;
        return inst;
    }

    void TrafficManager::init(
        hlt::GameMap &game_map,
        const std::vector<hlt::Position> &dropoff_positions,
        const std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Ship>> &ships,
        int turns_remaining)
    {
        m_game_map = &game_map;
        m_dropoff_positions = &dropoff_positions;
        m_ships = &ships;
        m_turns_remaining = turns_remaining;
    }

    bool TrafficManager::is_dropoff(const hlt::Position &pos) const
    {
        for (const auto &dp : *m_dropoff_positions)
        {
            if (pos == dp)
                return true;
        }
        return false;
    }

    void TrafficManager::adjust_priorities(std::vector<MoveRequest> &requests)
    {
        for (auto &req : requests)
        {
            // Ship sur un dropoff, HIGH PRIORITY
            if (is_dropoff(m_game_map->normalize(req.m_current)))
            {
                req.m_priority = constants::SHIP_ON_DROPOFF_PRIORITY;
            }
            // Ship en URGENT RETURN
            else if (req.m_priority >= constants::URGENT_RETURN_PRIORITY)
            {
                // Chercher le dropoff le plus proche
                int min_dist = 9999;
                for (const auto &dp : *m_dropoff_positions)
                {
                    int d = m_game_map->calculate_distance(req.m_current, dp);
                    if (d < min_dist)
                        min_dist = d;
                }

                // Si distance <= 2, c'est URGENT RETURN NEAR
                if (min_dist <= 2)
                {
                    req.m_priority = constants::URGENT_RETURN_NEAR_PRIORITY;
                }
            }
        }
    }

    void TrafficManager::resolve_conflicts(
        std::vector<MoveRequest> &requests,
        std::vector<MoveResult> &results,
        std::unordered_set<size_t> &resolved_indices)
    {
        // Index des positions actuelles pour détecter les conflits
        std::unordered_map<hlt::Position, size_t> pos_to_index;
        for (size_t i = 0; i < requests.size(); ++i)
        {
            hlt::Position normalized = m_game_map->normalize(requests[i].m_current);
            pos_to_index[normalized] = i;
        }

        // Pour chaque MoveRequest
        for (size_t i = 0; i < requests.size(); ++i)
        {
            if (resolved_indices.count(i))
                continue;

            // Check si un MoveRequest Y veut aller en X
            hlt::Position desired_norm = m_game_map->normalize(requests[i].m_desired);
            auto it = pos_to_index.find(desired_norm);
            if (it == pos_to_index.end())
                continue;

            // MoveRequest Y trouvé à l'indice j
            size_t j = it->second;
            if (j == i || resolved_indices.count(j))
                continue;

            hlt::Position j_desired_norm = m_game_map->normalize(requests[j].m_desired);
            hlt::Position i_current_norm = m_game_map->normalize(requests[i].m_current);

            // Si Y veut aller en X et X veut aller en Y
            if (j_desired_norm == i_current_norm)
            {
                // Le moins prioritaire reste en STILL
                if (requests[i].m_priority >= requests[j].m_priority)
                {
                    results.push_back({requests[j].m_ship_id, hlt::Direction::STILL});
                    resolved_indices.insert(j);
                }
                else
                {
                    results.push_back({requests[i].m_ship_id, hlt::Direction::STILL});
                    resolved_indices.insert(i);
                }
            }
        }
    }

    std::vector<MoveResult> TrafficManager::resolve_all(std::vector<MoveRequest> &requests)
    {
        std::vector<MoveResult> results;
        results.reserve(requests.size());

        if (requests.empty())
            return results;

        // Ajuste les PRIORITY des MoveRequest
        adjust_priorities(requests);

        // Tri des indices par PRIORITY décroissante
        std::vector<size_t> sorted_indices(requests.size());
        for (size_t i = 0; i < requests.size(); ++i)
            sorted_indices[i] = i;

        // Tri par PRIORITY décroissante
        std::sort(sorted_indices.begin(), sorted_indices.end(),
                  [&requests](size_t a, size_t b)
                  {
                      return requests[a].m_priority > requests[b].m_priority;
                  });

        // Détection de conflits
        std::unordered_set<size_t> resolved_indices;
        resolve_conflicts(requests, results, resolved_indices);

        // Pos deja réservées par les finals MoveResult
        std::unordered_set<hlt::Position> occupied_positions;

        // Réserver les positions des MoveResult déjà résolus
        for (const auto &result : results)
        {
            for (const auto &req : requests)
            {
                if (req.m_ship_id == result.m_ship_id)
                {
                    occupied_positions.insert(m_game_map->normalize(req.m_current));
                    break;
                }
            }
        }

        // Forcer le STILL des ships stuck
        manage_stuck_ships(requests, results, resolved_indices, occupied_positions);

        // Choisir entre desired et alternatives pour les MoveRequest restants
        for (size_t idx : sorted_indices)
        {
            if (resolved_indices.count(idx))
                continue;

            MoveRequest &req = requests[idx];
            hlt::Position desired_pos = m_game_map->normalize(req.m_desired);

            // ENDGAME CASE : Autoriser les collisions sur dropoff
            bool on_dropoff_collision_ok = (m_turns_remaining <= 2) && is_dropoff(desired_pos);

            // Desired direction
            if (occupied_positions.find(desired_pos) == occupied_positions.end() || on_dropoff_collision_ok)
            {
                results.push_back({req.m_ship_id, req.m_desired_direction});
                resolved_indices.insert(idx);

                if (!on_dropoff_collision_ok)
                    occupied_positions.insert(desired_pos);

                continue;
            }

            // Alternatives
            bool found_alt = false;
            for (const auto &alt_dir : req.m_alternatives)
            {
                hlt::Position alt_pos = m_game_map->normalize(req.m_current.directional_offset(alt_dir));

                if (occupied_positions.find(alt_pos) == occupied_positions.end())
                {
                    results.push_back({req.m_ship_id, alt_dir});
                    resolved_indices.insert(idx);
                    occupied_positions.insert(alt_pos);
                    found_alt = true;
                    break;
                }
            }

            if (found_alt)
                continue;

            // Fallback STILL
            results.push_back({req.m_ship_id, hlt::Direction::STILL});
            resolved_indices.insert(idx);
            occupied_positions.insert(m_game_map->normalize(req.m_current));
        }

        return results;
    }

    void TrafficManager::manage_stuck_ships(
        std::vector<MoveRequest> &requests,
        std::vector<MoveResult> &results,
        std::unordered_set<size_t> &resolved_indices,
        std::unordered_set<hlt::Position> &occupied_positions)
    {
        for (size_t i = 0; i < requests.size(); ++i)
        {
            if (resolved_indices.count(i))
                continue;

            MoveRequest &req = requests[i];

            auto ship_it = m_ships->find(req.m_ship_id);
            if (ship_it == m_ships->end())
                continue;

            // Si pas assez de halite pour bouger
            const auto &ship = ship_it->second;
            int cell_halite = m_game_map->at(ship->position)->halite;
            int move_cost = cell_halite / hlt::constants::MOVE_COST_RATIO;

            bool is_ship_stuck = (ship->halite < move_cost);

            // Forcer STILL
            if (is_ship_stuck)
            {
                results.push_back({req.m_ship_id, hlt::Direction::STILL});
                resolved_indices.insert(i);
                occupied_positions.insert(m_game_map->normalize(req.m_current));
            }
        }
    }
} // namespace bot
