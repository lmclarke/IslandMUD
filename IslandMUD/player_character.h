/* Jim Viebke
Feb 14, 2015 */

#ifndef PLAYER_CHARACTER_H
#define PLAYER_CHARACTER_H

#include "character.h"

class Player_Character;

using PC = Player_Character;

class Player_Character : public Character
{
public:
	Player_Character(const std::string & name, World & world) : Character(name, C::PC_FACTION_ID, world) {}

	std::string get_inventory_info() const;

	std::string get_equipped_item_info() const;

	std::string generate_area_map(const World & world, const std::map<std::string, std::shared_ptr<Character>> & actors) const;
};

#endif
