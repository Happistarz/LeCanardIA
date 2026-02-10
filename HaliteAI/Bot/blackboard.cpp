#include "blackboard.hpp"
#include "bot_parameters.hpp"
#include "hlt/constants.hpp"

#include <algorithm>

namespace bot
{

    void Blackboard::init(int width, int height)
    {
        map_width = width;
        map_height = height;
    }

    void Blackboard::assign_mission(hlt::EntityId id, MissionType mission, hlt::Position target)
    {
        ship_missions[id] = mission;
        ship_targets[id] = target;
    }

    // Metriques
    void Blackboard::update_metrics(hlt::Game &game)
    {
        long sum = 0;
        for (int y = 0; y < game.game_map->height; ++y)
            for (int x = 0; x < game.game_map->width; ++x)
                sum += game.game_map->cells[y][x].halite;

        total_halite = sum;
        average_halite = static_cast<int>(sum / (game.game_map->width * game.game_map->height));
    }

    void Blackboard::update_phase(int turn, int total_turns)
    {
        current_turn = turn;
        max_turns = total_turns;
        float progress = static_cast<float>(turn) / static_cast<float>(total_turns);

        if (progress > params::PHASE_ENDGAME)
            current_phase = GamePhase::ENDGAME;
        else if (progress > params::PHASE_LATE)
            current_phase = GamePhase::LATE;
        else if (progress > params::PHASE_MID)
            current_phase = GamePhase::MID;
        else
            current_phase = GamePhase::EARLY;
    }

    // Positions ennemies
    void Blackboard::update_enemy_structures(hlt::Game &game)
    {
        enemy_structures.clear();
        for (const auto &player : game.players)
        {
            if (player->id == game.my_id)
                continue;
            enemy_structures.push_back(player->shipyard->position);
            for (const auto &dp : player->dropoffs)
                enemy_structures.push_back(dp.second->position);
        }
    }

    bool Blackboard::is_enemy_territory(const hlt::Position &pos, hlt::GameMap &game_map) const
    {
        for (const auto &es : enemy_structures)
        {
            if (game_map.calculate_distance(pos, es) < ENEMY_ZONE_RADIUS)
                return true;
        }
        return false;
    }

    // Decision de spawn
    bool Blackboard::should_spawn(const hlt::Player &me)
    {
        if (me.halite < hlt::constants::SHIP_COST)
            return false;

        float progress = static_cast<float>(current_turn) / static_cast<float>(max_turns);
        if (progress > 0.70f)
            return false;

        if (average_halite < params::MIN_HALITE_TO_SPAWN)
            return false;

        int turns_left = max_turns - current_turn;
        int fleet_size = static_cast<int>(me.ships.size());

        // Si la flotte est petite, on peut se permettre de spawn meme si la reserve est basse
        if (fleet_size > 0 && total_halite / fleet_size < hlt::constants::SHIP_COST * 2)
            return false;

        if (turns_left < 50)
            return false;

        return true;
    }

    // Analyse de clusters
    void Blackboard::update_clusters(hlt::Game &game)
    {
        int w = game.game_map->width;
        int h = game.game_map->height;

        // Pre-calculer la grille de territoire ennemi
        std::vector<bool> enemy_grid(w * h, false);
        for (const auto &es : enemy_structures)
        {
            // Marquer toutes les cells dans le rayon de chaque structure ennemie
            for (int dy = -ENEMY_ZONE_RADIUS; dy <= ENEMY_ZONE_RADIUS; ++dy)
            {
                int max_dx = ENEMY_ZONE_RADIUS - std::abs(dy); // Manhattan distance
                for (int dx = -max_dx; dx <= max_dx; ++dx)
                {
                    int cx = ((es.x + dx) % w + w) % w;
                    int cy = ((es.y + dy) % h + h) % h;
                    enemy_grid[cy * w + cx] = true;
                }
            }
        }

        // Scanner la carte pour trouver les clusters de halite en evitant les zones ennemies
        struct ClusterCandidate
        {
            hlt::Position pos;
            long score;
        };
        std::vector<ClusterCandidate> candidates;
        candidates.reserve(w * h);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                if (enemy_grid[y * w + x])
                    continue;

                // Score du cluster en croix
                long score = game.game_map->at({x, y})->halite
                            + game.game_map->at({x, (y + 1) % h})->halite
                            + game.game_map->at({(x + 1) % w, y})->halite
                            + game.game_map->at({(x - 1 + w) % w, y})->halite
                            + game.game_map->at({x, (y - 1 + h) % h})->halite;

                candidates.push_back({{x, y}, score});
            }
        }

        // Tri par score decroissant
        std::sort(candidates.begin(), candidates.end(),
                  [](const ClusterCandidate &a, const ClusterCandidate &b)
                  { return a.score > b.score; });

        // Extraire les top clusters avec un espacement minimum pour eviter la congestion
        cluster_targets.clear();
        constexpr int MIN_CLUSTER_SPACING = 5;

        for (const auto &c : candidates)
        {
            if (static_cast<int>(cluster_targets.size()) >= MAX_CLUSTERS)
                break;

            bool too_close = false;
            for (const auto &existing : cluster_targets)
            {
                if (game.game_map->calculate_distance(c.pos, existing) < MIN_CLUSTER_SPACING)
                {
                    too_close = true;
                    break;
                }
            }
            if (!too_close)
                cluster_targets.push_back(c.pos);
        }

        // Fallback
        if (!cluster_targets.empty())
            best_cluster_position = cluster_targets[0];
        else
            best_cluster_position = {w / 2, h / 2};
    }

    // Repartition des cibles par ship
    void Blackboard::assign_ship_targets(hlt::Game &game)
    {
        if (cluster_targets.empty())
            return;

        // Compter le nombre de ships assignes a chaque cluster
        std::vector<int> cluster_load(cluster_targets.size(), 0);
        for (const auto &entry : ship_explore_targets)
        {
            // Si le ship est encore vivant et en MINING, compter son allocation
            if (!game.me->ships.count(entry.first))
                continue;
            if (ship_missions.count(entry.first) && ship_missions[entry.first] != MissionType::MINING)
                continue;

            for (size_t i = 0; i < cluster_targets.size(); ++i)
            {
                if (entry.second == cluster_targets[i])
                {
                    cluster_load[i]++;
                    break;
                }
            }
        }

        // Pour chaque ship MINING sans cible valide, assigner le cluster avec le meilleur ratio
        for (const auto &ship_entry : game.me->ships)
        {
            hlt::EntityId id = ship_entry.first;

            if (ship_missions.count(id) && ship_missions[id] != MissionType::MINING)
                continue;

            // Verifier si le ship a deja une cible valide
            bool has_valid_target = false;
            if (ship_explore_targets.count(id))
            {
                for (const auto &ct : cluster_targets)
                {
                    if (ship_explore_targets[id] == ct)
                    {
                        has_valid_target = true;
                        break;
                    }
                }
            }
            if (has_valid_target)
                continue;

            // Trouver le cluster avec le meilleur ratio
            size_t best_idx = 0;
            float best_score = -1.0f;
            for (size_t i = 0; i < cluster_targets.size(); ++i)
            {
                int dist = game.game_map->calculate_distance(
                    ship_entry.second->position, cluster_targets[i]);

                // Favoriser les clusters proches et peu chargés
                float score = 1.0f / (1.0f + cluster_load[i] * 3.0f + dist);
                if (score > best_score)
                {
                    best_score = score;
                    best_idx = i;
                }
            }

            ship_explore_targets[id] = cluster_targets[best_idx];
            cluster_load[best_idx]++;
        }
    }

    // Requetes spatiales
    bool Blackboard::is_position_safe(const hlt::Position &pos) const
    {
        return reserved_positions.count(pos) == 0 && danger_zones.count(pos) == 0;
    }

    bool Blackboard::is_position_reserved(const hlt::Position &pos) const
    {
        return reserved_positions.count(pos) > 0;
    }

    bool Blackboard::is_position_stuck(const hlt::Position &pos) const
    {
        return stuck_positions.count(pos) > 0;
    }

    void Blackboard::reserve_position(const hlt::Position &pos, hlt::EntityId ship_id)
    {
        reserved_positions.insert(pos);
        targeted_cells[pos] = ship_id;
    }

    void Blackboard::clear_turn_data()
    {
        reserved_positions.clear();
        targeted_cells.clear();
        danger_zones.clear();
        stuck_positions.clear();
    }

} // namespace bot