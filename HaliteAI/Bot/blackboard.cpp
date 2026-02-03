#include "blackboard.hpp"

namespace bot {

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

        // On remet le flag de spawn à false par défaut
        should_spawn = false;
    }
}