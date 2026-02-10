#include "traffic_manager.hpp"
#include "utils.hpp"
#include "hlt/constants.hpp"

#include <algorithm>
#include <unordered_map>

namespace bot
{
    // Singleton
    TrafficManager &TrafficManager::instance()
    {
        static TrafficManager inst;
        return inst;
    }

    // Initialisation par tour
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
            if (pos == dp)
                return true;
        return false;
    }

    // Ajustement des priorites
    void TrafficManager::adjust_priorities(std::vector<MoveRequest> &requests)
    {
        for (auto &req : requests)
        {
            if (is_dropoff(m_game_map->normalize(req.m_current)))
            {
                req.m_priority = MoveRequest::SHIP_ON_DROPOFF_PRIORITY;
            }
            else if (req.m_priority >= MoveRequest::URGENT_RETURN_PRIORITY)
            {
                int min_dist = 9999;
                for (const auto &dp : *m_dropoff_positions)
                {
                    int d = m_game_map->calculate_distance(req.m_current, dp);
                    if (d < min_dist)
                        min_dist = d;
                }
                if (min_dist <= 2)
                    req.m_priority = MoveRequest::URGENT_RETURN_NEAR_PRIORITY;
            }
        }
    }

    // Detection de conflits de swap
    void TrafficManager::resolve_conflicts(
        std::vector<MoveRequest> &requests,
        std::vector<MoveResult> &results,
        std::unordered_set<size_t> &resolved_indices)
    {
        // Map de position actuelle normalisee -> index dans requests
        std::unordered_map<hlt::Position, size_t> pos_to_index;
        for (size_t i = 0; i < requests.size(); ++i)
            pos_to_index[m_game_map->normalize(requests[i].m_current)] = i;

        for (size_t i = 0; i < requests.size(); ++i)
        {
            if (resolved_indices.count(i))
                continue;

            hlt::Position desired_norm = m_game_map->normalize(requests[i].m_desired);
            auto it = pos_to_index.find(desired_norm);
            if (it == pos_to_index.end())
                continue;

            size_t j = it->second;
            if (j == i || resolved_indices.count(j))
                continue;

            hlt::Position j_desired_norm = m_game_map->normalize(requests[j].m_desired);
            hlt::Position i_current_norm = m_game_map->normalize(requests[i].m_current);

            // Swap : la low priority reste en STILL
            if (j_desired_norm == i_current_norm)
            {
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

    // Resolve principal
    std::vector<MoveResult> TrafficManager::resolve_all(std::vector<MoveRequest> &requests)
    {
        std::vector<MoveResult> results;
        results.reserve(requests.size());
        if (requests.empty())
            return results;

        adjust_priorities(requests);

        // Tri des indices par priorite decroissante
        std::vector<size_t> sorted_indices(requests.size());
        for (size_t i = 0; i < requests.size(); ++i)
            sorted_indices[i] = i;
        std::sort(sorted_indices.begin(), sorted_indices.end(),
                  [&requests](size_t a, size_t b)
                  { return requests[a].m_priority > requests[b].m_priority; });

        // Resolve des conflits de swap
        std::unordered_set<size_t> resolved_indices;
        resolve_conflicts(requests, results, resolved_indices);

        // Suivi des positions occupees par les resultats resolus / futurs
        std::unordered_set<hlt::Position> occupied_positions;

        // Map de ship_id -> position actuelle normalisee pour les ships non resolus
        std::unordered_map<hlt::EntityId, hlt::Position> ship_current_positions;
        std::unordered_map<hlt::Position, hlt::EntityId> pos_to_current_ship;

        ship_current_positions.reserve(requests.size());
        for (const auto &req : requests)
        {
            hlt::Position cur = m_game_map->normalize(req.m_current);
            ship_current_positions[req.m_ship_id] = cur;
            pos_to_current_ship[cur] = req.m_ship_id;
        }

        // Marquer les positions futures des ships deja resolus (swaps)
        for (const auto &result : results)
        {
            auto it = ship_current_positions.find(result.m_ship_id);
            if (it != ship_current_positions.end())
            {
                // Si STILL, reste sur place. Si MOVE, nouvelle position.
                hlt::Position target;
                if (result.m_final_direction == hlt::Direction::STILL)
                    target = it->second;
                else
                    target = m_game_map->normalize(it->second.directional_offset(result.m_final_direction));
                
                occupied_positions.insert(target);
            }
        }

        // Force STILL pour les ships stuck
        manage_stuck_ships(requests, results, resolved_indices, occupied_positions);

        // Assigner les directions et alternatives pour les requests restantes
        for (size_t idx : sorted_indices)
        {
            if (resolved_indices.count(idx))
                continue;

            MoveRequest &req = requests[idx];
            hlt::Position current_pos = m_game_map->normalize(req.m_current);

            // Fonction locale de verification de securite
            auto is_safe_target = [&](const hlt::Position &target) -> bool {
                // La cell cible n'est pas deja occupee par un ship resolu ou a priorite plus haute
                if (occupied_positions.find(target) != occupied_positions.end())
                    return false;
                
                // Si la cell cible est la position actuelle d'un ship non resolu, ce ship doit soit rester sur place, soit bouger ailleurs. Sinon BLOCKED.
                if (pos_to_current_ship.count(target))
                {
                    hlt::EntityId occupant_id = pos_to_current_ship[target];
                    if (occupant_id != req.m_ship_id)
                    {
                        // Verifier si le ship occupant a une request qui le fait rester sur place ou bouger ailleurs
                        bool occupant_resolved = false;
                        for(const auto& res : results) {
                            if(res.m_ship_id == occupant_id) {
                                occupant_resolved = true;
                                break;
                            }
                        }
                        
                        // Si le ship occupant n'est pas encore resolu, on ne peut pas garantir sa direction. Considérer la cell comme bloquante pour eviter les collisions.
                        if (!occupant_resolved)
                            return false;
                    }
                }
                return true;
            };

            hlt::Position desired_pos = m_game_map->normalize(req.m_desired);

            // Essayer la direction souhaitee
            if (is_safe_target(desired_pos))
            {
                results.push_back({req.m_ship_id, req.m_desired_direction});
                resolved_indices.insert(idx);
                occupied_positions.insert(desired_pos);
                continue;
            }

            // Essayer les alternatives
            bool found_alt = false;
            for (const auto &alt_dir : req.m_alternatives)
            {
                hlt::Position alt_pos = m_game_map->normalize(
                    req.m_current.directional_offset(alt_dir));
                
                if (is_safe_target(alt_pos))
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

            // Repli : rester STILL
            results.push_back({req.m_ship_id, hlt::Direction::STILL});
            resolved_indices.insert(idx);
            occupied_positions.insert(current_pos);
        }

        return results;
    }

    // Gestion des ships stuck
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

            auto ship_it = m_ships->find(requests[i].m_ship_id);
            if (ship_it == m_ships->end())
                continue;

            const auto &ship = ship_it->second;
            int cell_halite = m_game_map->at(ship->position)->halite;

            if (ship->halite < move_cost(cell_halite))
            {
                results.push_back({requests[i].m_ship_id, hlt::Direction::STILL});
                resolved_indices.insert(i);
                occupied_positions.insert(m_game_map->normalize(requests[i].m_current));
            }
        }
    }
} // namespace bot
