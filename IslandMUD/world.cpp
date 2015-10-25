/* Jim Viebke
May 15 2015 */

#include <iomanip>
#include <memory>

#include "world.h"
#include "character.h"
#include "npc_enemy.h"

World::World()
{

}

void World::load()
{
	create_world_container();
	load_or_generate_terrain_and_mineral_maps();
}

// access a room given coordinates
unique_ptr<Room>::pointer World::room_at(const int & x, const int & y, const int & z)
{
	if (!U::bounds_check(x, y, z))
	{
		return world.begin()->get();
	}

	return (world.begin() + ((x * C::WORLD_Y_DIMENSION * C::WORLD_Z_DIMENSION) + (y * C::WORLD_Z_DIMENSION) + z))->get();
}
const unique_ptr<Room>::pointer World::room_at(const int & x, const int & y, const int & z) const
{
	if (!U::bounds_check(x, y, z))
	{
		return world.cbegin()->get();
	}

	return (world.cbegin() + ((x * C::WORLD_Y_DIMENSION * C::WORLD_Z_DIMENSION) + (y * C::WORLD_Z_DIMENSION) + z))->get();
}
unique_ptr<Room> & World::room_pointer_at(const int & x, const int & y, const int & z)
{
	return *(world.begin() + ((x * C::WORLD_Y_DIMENSION * C::WORLD_Z_DIMENSION) + (y * C::WORLD_Z_DIMENSION) + z));
}

// debugging
unsigned World::count_loaded_rooms() const
{
	/* This function is only used for debugging/development, to verify rooms are loading in and out as required.

	If only one player is playing (IE, the dev) there should be (2d+1)^2 rooms loaded, where d == view distance.
	For a view distance of 5, (5+1+5)*(5+1+5) (121) rooms should be in memory. */

	unsigned loaded_rooms = 0;
	for (int x = 0; x < C::WORLD_X_DIMENSION; ++x)
	{
		for (int y = 0; y < C::WORLD_Y_DIMENSION; ++y)
		{
			for (int z = 0; z < C::WORLD_Z_DIMENSION; ++z)
			{
				if (room_at(x, y, z) != nullptr)
				{
					++loaded_rooms;
				}
			}
		}
	}
	return loaded_rooms;
}

// load rooms around a player spawning in
void World::load_view_radius_around(const int & x, const int & y, const string & character_ID)
{
	// current x ranges from x-view to x+view
	for (int cx = x - (int)C::VIEW_DISTANCE; cx <= x + (int)C::VIEW_DISTANCE; ++cx)
	{
		// current y ranges from y-view to y+view
		for (int cy = y - (int)C::VIEW_DISTANCE; cy <= y + (int)C::VIEW_DISTANCE; ++cy)
		{
			// if the coordinates are not within world bounds
			if (!U::bounds_check(cx, cy))
			{
				continue; // go to next coordinate
			}

			// if the room is not already in memory
			if (room_at(cx, cy, C::GROUND_INDEX) == nullptr)
			{
				// move the room from disk to world
				load_room_to_world(cx, cy, C::GROUND_INDEX);
			}

			// whoever loaded this can see it
			room_at(cx, cy, C::GROUND_INDEX)->add_viewing_actor(character_ID);
		}
	}
}

// loading and unloading rooms at the edge of vision
void World::remove_viewer_and_attempt_unload(const int & x, const int & y, const int & z, const string & viewer_ID)
{
	// if the referenced room is out of bounds
	if (!U::bounds_check(x, y, z))
	{
		return; // nothing to remove or unload
	}

	// if the referenced room is not loaded
	if (room_at(x, y, z) == nullptr)
	{
		return; // nothing to remove or unload
	}

	// remove the viewer from the room's viewing list
	room_at(x, y, z)->remove_viewing_actor(viewer_ID);

	// if there is no one in the room or able to see it, remove the room from memory
	if (room_at(x, y, z)->is_unloadable())
	{
		unload_room(x, y, z);
	}
}

// test if a room can be removed from memory
bool World::is_unloadable(const int & x, const int & y, const int & z) const
{
	return room_at(x, y, z)->is_unloadable();
}

// move a room from world to disk
void World::unload_room(const int & x, const int & y, const int & z)
{
	// pass the coordinates and a shared_ptr to the room
	unload_room(x, y, z, room_at(x, y, z));

	// set the pointer at x,y,z to null
	erase_room_from_memory(x, y, z);
}

// room information
bool World::room_has_surface(const int & x, const int & y, const int & z, const string & direction_ID) const
{
	// if the room is outside of bounds
	if (!U::bounds_check(x, y, z)) { return false; }

	// if the room is not loaded
	if (room_at(x, y, z) == nullptr) { return false; }

	// test if the passed direction_ID exists as a wall
	return room_at(x, y, z)->has_surface(direction_ID);
}



/* Private member functions */



void World::create_world_container()
{
	cout << "\nCreating world container...";

	world = vector<unique_ptr<Room>>(C::WORLD_X_DIMENSION * C::WORLD_Y_DIMENSION * C::WORLD_Z_DIMENSION);
}
void World::load_or_generate_terrain_and_mineral_maps()
{
	// terrain and mineral maps are only used to generate rooms that do not already exist on disk

	cout << "\nLoading world terrain and mineral maps...";

	// generate a new world terrain map if needed
	if (load_existing_world_terrain())
	{
		cout << "\nLoaded existing world terrain map...";
	}
	else
	{
		// create the generator object
		Generator gen("world terrain map");

		// generate a biome map
		vector<vector<char_type>> biome_map = gen.generate_biome_map(C::LAND_CHAR, C::FOREST_CHAR, 3, 1, 25); // hardcoding a bit here
		gen.to_file(biome_map, gen.get_generated_terrain_dir() + "/biome_map.txt");

		// use the biome map to generate static in a full size map
		vector<vector<char_type>> world_map = gen.generate_static_using_biome_map(biome_map, 25, C::LAND_CHAR, C::FOREST_CHAR); // hardcoding again
		gen.to_file(world_map, gen.get_generated_terrain_dir() + "/static.txt");

		gen.game_of_life(world_map, 5, C::LAND_CHAR, C::FOREST_CHAR); gen.save_intermediate_map(world_map);
		gen.fill(world_map, 2, C::LAND_CHAR, C::FOREST_CHAR);  gen.save_intermediate_map(world_map);
		gen.clean(world_map, 3, C::LAND_CHAR, C::FOREST_CHAR); gen.save_intermediate_map(world_map);
		gen.fill(world_map, 4, C::LAND_CHAR, C::FOREST_CHAR);  gen.save_intermediate_map(world_map); // this is the same as fill(12), but each call has a seperate printout this way
		gen.fill(world_map, 4, C::LAND_CHAR, C::FOREST_CHAR);  gen.save_intermediate_map(world_map);
		gen.fill(world_map, 4, C::LAND_CHAR, C::FOREST_CHAR);  gen.save_intermediate_map(world_map);

		// save the final terrain to disk
		gen.to_file(world_map, C::world_terrain_file_location);

		this->terrain = U::make_unique<vector<vector<char_type>>>(world_map);
	}

	// generate a new iron ore mineral map if needed
	if (load_existing_iron_deposit_map())
	{
		cout << "\nLoaded existing iron ore mineral map...";
	}
	else
	{
		// create the generator object
		Generator gen("iron ore mineral map");

		// generate a biome map
		vector<vector<char_type>> biome_map = gen.generate_biome_map(C::LAND_CHAR, C::GENERIC_MINERAL_CHAR, 1, 19, 25); // hardcoding a bit here

		// use the biome map to generate static in a full size map
		vector<vector<char_type>> mineral_map = gen.generate_static_using_biome_map(biome_map, 25, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR); // hardcoding again

		gen.game_of_life(mineral_map, 5, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR); gen.save_intermediate_map(mineral_map);
		gen.fill(mineral_map, 2, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR);  gen.save_intermediate_map(mineral_map);
		gen.clean(mineral_map, 3, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR); gen.save_intermediate_map(mineral_map);
		gen.fill(mineral_map, 4, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR);  gen.save_intermediate_map(mineral_map); // this is the same as fill(12), but each call has a seperate printout this way
		gen.fill(mineral_map, 4, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR);  gen.save_intermediate_map(mineral_map);
		gen.fill(mineral_map, 4, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR);  gen.save_intermediate_map(mineral_map);

		// save the mineral map to disk
		gen.to_file(mineral_map, C::iron_deposit_map_file_location);

		this->iron_deposit_map = U::make_unique<vector<vector<char_type>>>(mineral_map);
	}

	// generate a new limestone mineral map if needed
	if (load_existing_limestone_deposit_map())
	{
		cout << "\nLoaded existing limestone mineral map...";
	}
	else
	{
		// create the generator object
		Generator gen("limestone mineral map");

		// generate a biome map
		vector<vector<char_type>> biome_map = gen.generate_biome_map(C::LAND_CHAR, C::GENERIC_MINERAL_CHAR, 1, 39, 25); // hardcoding a bit here

		// use the biome map to generate static in a full size map
		vector<vector<char_type>> mineral_map = gen.generate_static_using_biome_map(biome_map, 25, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR); // hardcoding again

		gen.game_of_life(mineral_map, 5, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR); gen.save_intermediate_map(mineral_map);
		gen.fill(mineral_map, 2, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR);  gen.save_intermediate_map(mineral_map);
		gen.clean(mineral_map, 3, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR); gen.save_intermediate_map(mineral_map);
		gen.fill(mineral_map, 4, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR);  gen.save_intermediate_map(mineral_map); // this is the same as fill(12), but each call has a seperate printout this way
		gen.fill(mineral_map, 4, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR);  gen.save_intermediate_map(mineral_map);
		gen.fill(mineral_map, 4, C::LAND_CHAR, C::GENERIC_MINERAL_CHAR);  gen.save_intermediate_map(mineral_map);

		// save the mineral map to disk
		gen.to_file(mineral_map, C::limestone_deposit_map_file_location);

		this->limestone_deposit_map = U::make_unique<vector<vector<char_type>>>(mineral_map);
	}
}

// three functions for loading and verifying the world map and the two mineral maps

bool World::load_existing_world_terrain()
{
	// load the contents of the terrain file, if it exists
	{
		vector<vector<char_type>> temp_terrain;
		if (U::file_exists(C::world_terrain_file_location))
		{
			fstream terrain_file;
			terrain_file.open(C::world_terrain_file_location);
			string row;
			while (getline(terrain_file, row)) // for each row
			{
				if (row.length() > 1) // if the row is not empty
				{
#ifdef _WIN32
					temp_terrain.push_back(vector<char_type>(row.begin(), row.end())); // copy the contents of the row into an anonymous vector
#else
					vector<char_type> vec;
					// for each character in the string
					for (string::iterator it = row.begin(); it != row.end(); ++it)
					{
						// add it to the vector as a string of its own
						vec.push_back(string(1, *it));
					}
					// add the vector as the next row in the terrain file
					temp_terrain.push_back(vec);
#endif
				}
			}
		}

		this->terrain = U::make_unique<vector<vector<char_type>>>(temp_terrain);
	}

	// test if the loaded terrain is the correct dimensions
	bool terrain_loaded_from_file_good = false;
	if (terrain->size() == C::WORLD_X_DIMENSION)
	{
		terrain_loaded_from_file_good = true;
		for (unsigned i = 0; i < terrain->size(); ++i)
		{
			if (terrain->operator[](i).size() != C::WORLD_Y_DIMENSION)
			{
				terrain_loaded_from_file_good = false;
				break;
			}
		}
	}

	return terrain_loaded_from_file_good;
}
bool World::load_existing_iron_deposit_map()
{
	// load the contents of the terrain file, if it exists
	{
		vector<vector<char_type>> temp_terrain;
		if (U::file_exists(C::iron_deposit_map_file_location))
		{
			fstream terrain_file;
			terrain_file.open(C::iron_deposit_map_file_location);
			string row;
			while (getline(terrain_file, row)) // for each row
			{
				if (row.length() > 1) // if the row is not empty
				{
#ifdef _WIN32
					temp_terrain.push_back(vector<char_type>(row.begin(), row.end())); // copy the contents of the row into an anonymous vector
#else
					vector<char_type> vec;
					// for each character in the string
					for (string::iterator it = row.begin(); it != row.end(); ++it)
					{
						// add it to the vector as a string of its own
						vec.push_back(string(1, *it));
					}
					// add the vector as the next row in the terrain file
					temp_terrain.push_back(vec);
#endif
				}
			}
		}

		this->iron_deposit_map = U::make_unique<vector<vector<char_type>>>(temp_terrain);
	}

	// test if the loaded terrain is the correct dimensions
	bool terrain_loaded_from_file_good = false;
	if (iron_deposit_map->size() == C::WORLD_X_DIMENSION)
	{
		terrain_loaded_from_file_good = true;
		for (unsigned i = 0; i < iron_deposit_map->size(); ++i)
		{
			if (iron_deposit_map->operator[](i).size() != C::WORLD_Y_DIMENSION)
			{
				terrain_loaded_from_file_good = false;
				break;
			}
		}
	}

	return terrain_loaded_from_file_good;
}
bool World::load_existing_limestone_deposit_map()
{
	// load the contents of the terrain file, if it exists
	{
		vector<vector<char_type>> temp_terrain;
		if (U::file_exists(C::limestone_deposit_map_file_location))
		{
			fstream terrain_file;
			terrain_file.open(C::limestone_deposit_map_file_location);
			string row;
			while (getline(terrain_file, row)) // for each row
			{
				if (row.length() > 1) // if the row is not empty
				{
#ifdef _WIN32
					temp_terrain.push_back(vector<char_type>(row.begin(), row.end())); // copy the contents of the row into an anonymous vector
#else
					vector<char_type> vec;
					// for each character in the string
					for (string::iterator it = row.begin(); it != row.end(); ++it)
					{
						// add it to the vector as a string of its own
						vec.push_back(string(1, *it));
					}
					// add the vector as the next row in the terrain file
					temp_terrain.push_back(vec);
#endif
				}
			}
		}

		this->limestone_deposit_map = U::make_unique<vector<vector<char_type>>>(temp_terrain);
	}

	// test if the loaded terrain is the correct dimensions
	bool terrain_loaded_from_file_good = false;
	if (limestone_deposit_map->size() == C::WORLD_X_DIMENSION)
	{
		terrain_loaded_from_file_good = true;
		for (unsigned i = 0; i < limestone_deposit_map->size(); ++i)
		{
			if (limestone_deposit_map->operator[](i).size() != C::WORLD_Y_DIMENSION)
			{
				terrain_loaded_from_file_good = false;
				break;
			}
		}
	}

	return terrain_loaded_from_file_good;
}

// a room at x,y,z does not exist on the disk; create it and add it to the world
void World::generate_room_at(const int & x, const int & y, const int & z)
{
	// ensure the folder exists
	string z_stack_path = C::room_directory + "/" + U::to_string(x);
	U::create_path_if_not_exists(z_stack_path);

	// extend the path to include the file
	z_stack_path += "/" + U::to_string(x) + "-" + U::to_string(y) + ".xml";

	// create an XML document to store the Z stack
	xml_document z_stack;

	// if the file exists
	if (U::file_exists(z_stack_path))
	{
		// load the z-stack to the document
		load_vertical_rooms_to_XML(x, y, z_stack);

		// attempt to extract the specified room
		const xml_node room_node = z_stack.child(("room-" + U::to_string(z)).c_str());

		// if the specified room is not in the stack
		if (!room_node)
		{
			// create the room
			unique_ptr<Room> room = create_room(x, y, z);

			// add it to the world...
			room_pointer_at(x, y, z) = std::move(room);

			// ...and the z-stack
			this->add_room_to_z_stack(z, room_at(x, y, z), z_stack);
		}
	}
	else
	{
		// the entire z-stack does not exist on the disk
		// create specified room and add it to it

		// create the room
		unique_ptr<Room> room = create_room(x, y, z);

		// add it to the world...
		room_pointer_at(x, y, z) = std::move(room);

		// ...and the z-stack
		this->add_room_to_z_stack(z, room_at(x, y, z), z_stack);

		// save the stack to disk
		z_stack.save_file(z_stack_path.c_str());
	}

	// save the stack to disk
	z_stack.save_file(z_stack_path.c_str());
}

// load in all rooms at x,y to an xml_document
void World::load_vertical_rooms_to_XML(const int & ix, const int & iy, xml_document & vertical_rooms)
{
	// convert integers to strings, since they'll be used multiple times
	const string x = U::to_string(ix);
	const string y = U::to_string(iy);

	// populate the document using the file for the vertical stack of rooms at x,y
	vertical_rooms.load_file((C::room_directory + "/" + x + "/" + x + "-" + y + ".xml").c_str());
}

// build a room given an XML node, add to world at x,y,z
void World::add_room_to_world(xml_node & room_node, const int & x, const int & y, const int & z)
{
	// create an empty room
	unique_ptr<Room> room(U::make_unique<Room>());

	// set whether or not the room is water (off-island or river/lake)
	room->set_water_status(room_node.attribute(C::XML_IS_WATER.c_str()).as_bool());

	// add a boolean representing if the room is water (off-island or a lake/river)
	room_node.append_attribute(C::XML_IS_WATER.c_str()).as_bool(room->is_water());

	// for each item in the room
	for (const xml_node & item_node : room_node.children(C::XML_ITEM.c_str()))
	{
		// use the item ID to make a new item and add it to the room

		// create the item
		shared_ptr<Item> item = Craft::make(item_node.child_value());

		// set the item's health
		item->set_health(item_node.attribute(C::XML_ITEM_HEALTH.c_str()).as_int());

		// add the item to the room
		room->add_item(item);
	}

	// for each surface in the room
	for (const xml_node & surface : room_node.children(C::XML_SURFACE.c_str()))
	{
		// extract the attribute containing the health/integrity of the surface
		const xml_attribute health_attribute = surface.attribute(C::XML_SURFACE_HEALTH.c_str());

		// construct a new surface to add to the room
		room->add_surface(
			surface.child(C::XML_SURFACE_DIRECTION.c_str()).child_value(),
			surface.child(C::XML_SURFACE_MATERIAL.c_str()).child_value(),

			(!health_attribute.empty()) // if the health attribute exists
			? health_attribute.as_int() // set the surface health to the attribute's value
			: C::MAX_SURFACE_HEALTH // else set the room to full health

			);

		// select the door node
		const xml_node door_node = surface.child(C::XML_DOOR.c_str());

		// if the door node exists
		if (!door_node.empty())
		{
			// extract values
			const int health = door_node.attribute(C::XML_DOOR_HEALTH.c_str()).as_int();
			const string material_ID = door_node.attribute(C::XML_DOOR_MATERIAL.c_str()).value();
			const string faction_ID = door_node.attribute(C::XML_DOOR_FACTION.c_str()).value();

			// add a door to the surface
			room->add_door(surface.child(C::XML_SURFACE_DIRECTION.c_str()).child_value(),
				health, material_ID, faction_ID);
		}
	}

	// extract the chest node
	const xml_node chest_node = room_node.child(C::XML_CHEST.c_str());

	// if the extracted chest node exists
	if (!chest_node.empty())
	{
		// create the chest's internal structure for equipment
		multimap<string, shared_ptr<Equipment>> equipment_contents = {};
		// for each equipment node
		for (const xml_node & equipment_item_node : chest_node.child(C::XML_CHEST_EQUIPMENT.c_str()).children())
		{
			// create the equipment item
			shared_ptr<Equipment> equipment = U::convert_to<Equipment>(Craft::make(equipment_item_node.name()));

			// add the equipment to the equipment multimap
			equipment_contents.insert(make_pair(equipment->name, equipment));
		}

		// create the chest's internal structure for materials
		map<string, shared_ptr<Material>> material_contents = {};
		// for each material node
		for (const xml_node & material_item_node : chest_node.child(C::XML_CHEST_MATERIALS.c_str()).children())
		{
			// create the material item
			shared_ptr<Material> material = U::convert_to<Material>(Craft::make(material_item_node.name()));

			// set the amount
			material->amount = material_item_node.attribute(C::XML_CHEST_MATERIALS_COUNT.c_str()).as_int();

			// add the material to the materials multimap
			material_contents.insert(make_pair(material->name, material));
		}

		// create an anonymous chest object and add the chest to the room
		room->set_chest(make_shared<Chest>(Chest(
			chest_node.attribute(C::XML_CHEST_FACTION_ID.c_str()).as_string(), // the chest's faction ID
			chest_node.attribute(C::XML_CHEST_HEALTH.c_str()).as_int(), // the chest's health
			equipment_contents, material_contents) // the chest's contents
			));
	}

	// add room to world
	room_pointer_at(x, y, z) = std::move(room);
}

// move specific room into memory
void World::load_room_to_world(const int & x, const int & y, const int & z)
{
	// if the room is already loaded
	if (room_at(x, y, z) != nullptr)
	{
		// nothing left to do here
		return;
	}

	// create an XML document to hold the vertical stack of rooms
	xml_document vertical_rooms;

	// get the path to the z_stack
	const string str_x = U::to_string(x);
	const string str_y = U::to_string(y);
	const string z_stack_path = C::room_directory + "/" + str_x + "/" + str_x + "-" + str_y + ".xml";

	// if the z_stack does not exist
	if (!U::file_exists(z_stack_path))
	{
		// generate the room (adds it to disk and the world)
		generate_room_at(x, y, z);
		return; // we're done
	}

	// the file exists, load it
	vertical_rooms.load_file(z_stack_path.c_str());

	// attempt to extract the room from the file
	xml_node room_node = vertical_rooms.child(("room-" + U::to_string(z)).c_str());

	// if the room nodes exists in the file
	if (room_node)
	{
		// add it to the world
		add_room_to_world(room_node, x, y, z);
	}
	else // the node does not exist on the disk
	{
		// create the room and add it to disk and the world
		generate_room_at(x, y, z);
	}
}

// move a passed room to disk
void World::unload_room(const int & x, const int & y, const int & z, const unique_ptr<Room>::pointer room)
{
	// Unloads passed room. Can be called even if the room doesn't exist in the world structure

	// ensure the path exists up to \x
	string room_path = (C::room_directory + "/" + U::to_string(x) + "/");
	U::create_path_if_not_exists(room_path);

	// ensure the path exists up to \x-y.xml
	room_path += (U::to_string(x) + "-" + U::to_string(y) + ".xml");

	// create an XML document to represent the stack of rooms
	xml_document z_stack;

	// load the stack of rooms into the XML
	load_vertical_rooms_to_XML(x, y, z_stack);

	// process the room into the XML
	add_room_to_z_stack(z, room, z_stack);

	// save the document
	z_stack.save_file(room_path.c_str()); // returns an unused boolean
}

// add a room to a z_stack at a given index
void World::add_room_to_z_stack(const int & z, const unique_ptr<Room>::pointer room, xml_document & z_stack) const
{
	// delete the room node if it already exists
	z_stack.remove_child(("room-" + U::to_string(z)).c_str());

	// create a new node for the room
	xml_node room_node = z_stack.append_child(("room-" + U::to_string(z)).c_str());

	// add a boolean representing if the room is water (off-island or a lake/river)
	room_node.append_attribute(C::XML_IS_WATER.c_str()).set_value(room->is_water());

	// for each item in the room
	const multimap<string, shared_ptr<Item>> room_item_contents = room->get_contents();
	for (multimap<string, shared_ptr<Item>>::const_iterator item_it = room_item_contents.cbegin();
		item_it != room_item_contents.cend(); ++item_it)
	{
		// create a node for an item
		xml_node item_node = room_node.append_child(C::XML_ITEM.c_str());

		// append the item's health as an attribute
		item_node.append_attribute(C::XML_ITEM_HEALTH.c_str()).set_value(item_it->second->get_health());

		// append the item's ID
		item_node.append_child(node_pcdata).set_value(item_it->first.c_str());
	}

	// for each side of the room
	const map<string, Room_Side> room_sides = room->get_room_sides();
	for (map<string, Room_Side>::const_iterator surface_it = room_sides.cbegin();
		surface_it != room_sides.cend(); ++surface_it)
	{
		// create a node for a surface
		xml_node surface_node = room_node.append_child(C::XML_SURFACE.c_str());

		// add an attribute to the surface containing the health of the surface (example: health="90")
		surface_node.append_attribute(C::XML_SURFACE_HEALTH.c_str()).set_value(surface_it->second.get_health());

		// create nodes for surface direction and surface material
		xml_node direction_node = surface_node.append_child(C::XML_SURFACE_DIRECTION.c_str());
		xml_node material_node = surface_node.append_child(C::XML_SURFACE_MATERIAL.c_str());

		// append the surface direction and material
		direction_node.append_child(node_pcdata).set_value(surface_it->first.c_str()); // direction ID
		material_node.append_child(node_pcdata).set_value(surface_it->second.get_material_id().c_str()); // material ID

		// conditionally create a node for a door
		if (surface_it->second.has_door())
		{
			// create a node to hold the door
			xml_node door_node = surface_node.append_child(C::XML_DOOR.c_str());

			// retrive the door
			shared_ptr<Door> door = surface_it->second.get_door();

			// add three attributes for health, material, and faction

			// health="55"
			door_node.append_attribute(C::XML_DOOR_HEALTH.c_str()).set_value(door->get_health());
			// material="wood"
			door_node.append_attribute(C::XML_DOOR_MATERIAL.c_str()).set_value(door->get_material_ID().c_str());
			// faction="hostile_NPC"
			door_node.append_attribute(C::XML_DOOR_FACTION.c_str()).set_value(door->get_faction_ID().c_str());
		}
	}

	// if there is a chest in this room
	if (room->has_chest())
	{
		// create a chest node on the room node and name the chest node
		xml_node chest_node = room_node.append_child(C::XML_CHEST.c_str());
		chest_node.set_name(C::XML_CHEST.c_str());

		// extract the chest from the room
		const shared_ptr<Chest> chest = room->get_chest(); // ****** HERE

		// add an attribute for the chest's faction ID
		chest_node.append_attribute(C::XML_CHEST_FACTION_ID.c_str()).set_value(chest->get_faction_id().c_str());

		// add a health attribute to the chest node
		chest_node.append_attribute(C::XML_CHEST_HEALTH.c_str()).set_value(chest->get_health());

		// add equipment and material nodes to the chest node
		xml_node equipment_node = chest_node.append_child(C::XML_CHEST_EQUIPMENT.c_str());
		xml_node material_node = chest_node.append_child(C::XML_CHEST_MATERIALS.c_str());

		// for each equipment item
		const multimap<string, shared_ptr<Equipment>> chest_equipment_contents = chest->get_equipment_contents(); // extract contents
		for (multimap<string, shared_ptr<Equipment>>::const_iterator equipment_it = chest_equipment_contents.cbegin();
			equipment_it != chest_equipment_contents.cend(); ++equipment_it)
		{
			// append a node where name is the equipment's ID
			/* xml_node item_node = */ equipment_node.append_child(equipment_it->first.c_str());
		}

		// for each material item
		const map<string, shared_ptr<Material>> chest_material_contents = chest->get_material_contents(); // extract contents
		for (map<string, shared_ptr<Material>>::const_iterator material_it = chest_material_contents.cbegin();
			material_it != chest_material_contents.cend(); ++material_it)
		{
			// append a node where name is the material's ID, with an attribute with a name of "XML_CHEST_MATERIALS_COUNT", and a value of the material's count
			/* xml_node item_node = */ material_node.append_child(material_it->first.c_str()).append_attribute(C::XML_CHEST_MATERIALS_COUNT.c_str()).set_value(material_it->second->amount);
		}
	}

}

// create a new empty room given its coordinates and the world terrain
unique_ptr<Room> World::create_room(const int & x, const int & y, const int & z) const
{
	unique_ptr<Room> room = U::make_unique<Room>();

	// if the room is ground level
	if (z == C::GROUND_INDEX)
	{
		// if the terrain map indicates the room is forest
		if (terrain->operator[](x)[y] == C::FOREST_CHAR)
		{
			room->add_item(Craft::make(C::TREE_ID)); // add a tree
		}

		// if the terrain map indicates the room is water
		if (terrain->operator[](x)[y] == C::WATER_CHAR)
		{
			room->set_water_status(true);
		}

		// if the iron ore map indicates the room contains an iron deposit
		if (iron_deposit_map->operator[](x)[y] == C::GENERIC_MINERAL_CHAR)
		{
			room->add_item(Craft::make(C::IRON_DEPOSIT_ID)); // add an iron deposit item
		}

		// if the limestone map indicates the room contains limestone
		if (limestone_deposit_map->operator[](x)[y] == C::GENERIC_MINERAL_CHAR)
		{
			room->add_item(Craft::make(C::LIMESTONE_DEPOSIT_ID)); // add a limestone item
		}
	}

	return room;
}

void World::erase_room_from_memory(const int & x, const int & y, const int & z)
{
	(world.begin() + ((x * C::WORLD_Y_DIMENSION * C::WORLD_Z_DIMENSION) + (y * C::WORLD_Z_DIMENSION) + z))->reset();
}