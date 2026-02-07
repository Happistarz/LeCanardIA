#include "blackboard.hpp"
#include "hlt/constants.hpp"

namespace bot {

    // 1. SCANNER : On calcule combien d'argent il reste sur toute la carte
    void Blackboard::update_metrics(hlt::Game& game) {
        long current_total_halite = 0;

        for (int y = 0; y < game.game_map->height; ++y) {
            for (int x = 0; x < game.game_map->width; ++x) {
                current_total_halite += game.game_map->cells[y][x].halite;
            }
        }

        this->total_halite = current_total_halite;
        // Moyenne = Total / Nombre de cases
        this->average_halite = current_total_halite / (game.game_map->width * game.game_map->height);
    }

    // 2. PHASE : On détermine où on en est dans la partie
    void Blackboard::update_phase(int turn, int total_turns) {
        this->current_turn = turn;
        this->max_turns = total_turns;

        float progress = (float)turn / (float)total_turns;

        if (progress > 0.90) {
            current_phase = GamePhase::ENDGAME; // Reste 10% du temps : URGENCE
        } else if (progress > 0.60) {
            current_phase = GamePhase::LATE;
        } else if (progress > 0.30) {
            current_phase = GamePhase::MID;
        } else {
            current_phase = GamePhase::EARLY;
        }
    }

    // 3. DECISION : Est-ce qu'on achète un vaisseau ?
    bool Blackboard::should_spawn(const hlt::Player& me) {
        // Condition 1 : A-t-on les sous ?
        if (me.halite < hlt::constants::SHIP_COST) return false;

        // Condition 2 : La base est-elle libre ? (Crucial !)
        if (is_position_reserved(me.shipyard->position)) return false;

        // Condition 3 : Si c'est la fin du jeu, on arrête de dépenser !
        if (current_phase == GamePhase::ENDGAME || current_phase == GamePhase::LATE) {
            return false;
        }

        // Condition 4 : Est-ce qu'il reste assez à manger sur la carte ?
        // Si la moyenne est trop basse, les nouveaux vaisseaux ne seront jamais rentables.
        if (average_halite < 50) { // Seuil arbitraire, tu pourras le tuner
            return false;
        }

        return true;
    }

    void Blackboard::update_best_cluster(hlt::Game& game) {
        long max_score = -1;
        hlt::Position best_pos = {0, 0};
        int w = game.game_map->width;  // Raccourci
        int h = game.game_map->height; // Raccourci

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                long local_halite = 0;

                // Scan de la case et voisines
                local_halite += game.game_map->at({x, y})->halite;
                local_halite += game.game_map->at({x, (y+1)%h})->halite; // Sud (Safe)
                local_halite += game.game_map->at({(x+1)%w, y})->halite; // Est (Safe)
                local_halite += game.game_map->at({(x-1+w)%w, y})->halite; // Ouest (Safe)
                local_halite += game.game_map->at({x, (y-1+h)%h})->halite; // Nord (Safe)

                if (local_halite > max_score) {
                    max_score = local_halite;
                    best_pos = {x, y};
                }
            }
        }
        this->best_cluster_position = best_pos;
    }

    // Vérifie si une position est sûre pour s'y déplacer
    bool Blackboard::is_position_safe(const hlt::Position& pos) const {
        // 1. Est-ce que un de nos vaisseaux a déjà réservé cette case ce tour-ci ?
        if (reserved_positions.count(pos) > 0) {
            return false;
        }
        // 2. Est-ce que la case est marquée comme zone dangereuse (ex: proximité ennemi) ?
        if (danger_zones.count(pos) > 0) {
            return false;
        }
        // Si tout est bon, la case est safe
        return true;
    }

    // Vérifie simplement si la case est réservée
    bool Blackboard::is_position_reserved(const hlt::Position& pos) const {
        return reserved_positions.count(pos) > 0;
    }

    // Marque une position comme occupée pour ce tour
    void Blackboard::reserve_position(const hlt::Position& pos, hlt::EntityId ship_id) {
        // On l'ajoute au set pour la recherche rapide (collisions)
        reserved_positions.insert(pos);
        // On l'ajoute à la map pour savoir QUI va là (utile pour le debug ou la logique avancée)
        targeted_cells[pos] = ship_id;
    }

    // Nettoyage au début de chaque tour
    void Blackboard::clear_turn_data() {
        reserved_positions.clear();
        targeted_cells.clear();
        danger_zones.clear();
    }
}