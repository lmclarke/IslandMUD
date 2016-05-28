﻿/* Jim Viebke
April 1, 2014 */

#ifndef WORLD_H
#define WORLD_H

#include <vector>
#include <memory>
#include <map>

#include "XML/pugixml.hpp"

#include "room.h"
#include "generator.h"

class World
{
private:
	
	// 2d terrain (biome) map
	std::unique_ptr<std::vector<std::vector<char>>> terrain;
	std::unique_ptr<std::vector<std::vector<char>>> iron_deposit_map;
	std::unique_ptr<std::vector<std::vector<char>>> limestone_deposit_map;

	std::vector<std::unique_ptr<Room>> world;

public:

	World();

	// access a room given coordinates
	std::unique_ptr<Room>::pointer room_at(const int & x, const int & y, const int & z);
	const std::unique_ptr<Room>::pointer room_at(const int & x, const int & y, const int & z) const;
	std::unique_ptr<Room> & room_pointer_at(const int & x, const int & y, const int & z);

	// debugging
	unsigned count_loaded_rooms() const;

	// load rooms around a player spawning in
	void load_view_radius_around(const int & x, const int & y, const std::string & character_ID);

	// loading and unloading rooms at the edge of vision
	void remove_viewer_and_attempt_unload(const int & x, const int & y, const int & z, const std::string & viewer_ID);

	// unloading of all rooms in view distance (for logging out or dying)
	void attempt_unload_radius(const int & x, const int & y, const std::string & player_ID);

	// test if a room can be removed from memory
	bool is_unloadable(const int & x, const int & y, const int & z) const;

	// move a room from world to disk
	void unload_room(const int & x, const int & y, const int & z);

	// room information
	bool room_has_surface(const int & x, const int & y, const int & z, const std::string & direction_ID) const;



private:

	void create_world_container();
	void load_or_generate_terrain_and_mineral_maps();

	// three functions for loading and verifying the world map and the two mineral maps

	bool load_existing_world_terrain();
	bool load_existing_iron_deposit_map();
	bool load_existing_limestone_deposit_map();

	// a room at x,y,z does not exist on the disk; create it
	void generate_room_at(const int & x, const int & y, const int & z);

	// load in all rooms at x,y to an xml_document
	void load_vertical_rooms_to_XML(const int & ix, const int & iy, pugi::xml_document & vertical_rooms);

	// build a room given an XML node, add to world at x,y,z
	void add_room_to_world(pugi::xml_node & room_node, const int & x, const int & y, const int & z);

	// move specific room into memory
	void load_room_to_world(const int & x, const int & y, const int & z);

	// move a passed room to disk
	void unload_room(const int & x, const int & y, const int & z, const std::unique_ptr<Room>::pointer room);

	// add a room to a z_stack at a given index
	void add_room_to_z_stack(const int & z, const std::unique_ptr<Room>::pointer room, pugi::xml_document & z_stack) const;

	// create a new empty room given its coordinates and the world terrain
	std::unique_ptr<Room> create_room(const int & x, const int & y, const int & z) const;

	// remove a room from memory
	void erase_room_from_memory(const int & x, const int & y, const int & z);
};

#endif
