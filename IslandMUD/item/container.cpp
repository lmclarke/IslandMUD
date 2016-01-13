
#include "container.h"

#include "../utilities.h"
#include "../craft.h"

// public

std::string Container::contents_to_string() const
{
	// The calling function could construct the beginning of the sentence, like
	// "You have " or "The chest contains ".

	if (contents.size() == 0)
	{
		return "";
	}

	std::stringstream output;
	output << " ";

	// record an iterator to the last item
	const auto last_it = --contents.end();

	for (std::multimap<std::string, std::shared_ptr<Item>>::const_iterator it = contents.begin();
		it != contents.end(); ++it)
	{
		// if the item is stackable
		if (const std::shared_ptr<Stackable> stackable = U::convert_to<Stackable>(it->second))
		{
			if (stackable->amount != 1) // and the amount is != 1
			{
				output << stackable->amount << " " << U::get_plural_for(stackable->name);
			}
			else
			{
				output << U::get_article_for(it->second->name) << " " << it->second->name; // this is duplicated below, but I'm not sure how to fix this
			}
		}
		else // the item is not stackable
		{
			output << U::get_article_for(it->second->name) << " " << it->second->name;
		}

		// append a period or a comma
		output << ((it == last_it) ? "." : ", ");
	}

	return output.str();
}

const std::multimap<std::string, std::shared_ptr<Item>> & Container::get_contents() const
{
	return contents;
}

bool Container::contains(const std::string & item_id) const
{
	// return a boolean indicating if chest contains at least one instance of the specified item
	return this->contents.find(item_id) != contents.cend();
}
bool Container::contains(const std::string & item_id, const unsigned & count) const
{
	const std::multimap<std::string, std::shared_ptr<Item>>::const_iterator item_it = contents.find(item_id);

	// if the item does not exist in the contents
	if (item_it == contents.cend()) return false;

	// if the item is stackable
	if (const std::shared_ptr<Stackable> stackable = U::convert_to<Stackable>(item_it->second))
	{
		// test if the present amount is greater than or equal to the count
		return stackable->amount >= count;
	}
	else // the item is not stackable
	{
		// test if at least [count] instances exist
		return contents.count(item_id) >= count;
	}
}
unsigned Container::count(const std::string & item_id) const
{
	// count the number of items in the container that match the item ID
	const unsigned count = contents.count(item_id);

	// if the count is 1, the single item could be a stackable type
	if (count == 1)
	{
		// if the item is a stackable type
		if (const std::shared_ptr<Stackable> & stackable = U::convert_to<Stackable>(contents.find(item_id)->second))
		{
			// the amount of the stackable is the correct value
			return stackable->amount;
		}
	}

	// the item occurs 0 or more than 1 times; return count as the correct value
	return count;
}
unsigned Container::size() const
{
	return contents.size();
}

void Container::insert(const std::shared_ptr<Item> & item)
{
	if (item == nullptr) return; // well, something went wrong

	if (U::is<Stackable>(item) && this->contains(item->name)) // if the item is stackable and already exists in the chest
	{
		// increment the existing item in the chest
		U::convert_to<Stackable>(contents.find(item->name)->second)->amount++;
	}
	else // the item is either not stackable, or is not in the chest
	{
		contents.insert(make_pair(item->name, item));
	}
}
std::shared_ptr<Item> Container::erase(const std::string & item_id)
{
	// get an iterator to the item in question
	const std::multimap<std::string, std::shared_ptr<Item>>::iterator item_it = contents.find(item_id);

	// if the item does not exist to take, return a null pointer
	if (item_it == contents.cend()) return nullptr;

	// create a new shared_ptr to the stored item
	std::shared_ptr<Item> item = contents.find(item_id)->second;

	// if the item is a stackable type
	if (std::shared_ptr<Stackable> stackable = U::convert_to<Stackable>(item))
	{
		// count one less item in storage
		stackable->amount--;

		// if there are no items remaining in storage
		if (stackable->amount < 1)
		{
			// destroy the container's reference to the item
			contents.erase(item_id);
		}

		// return a new instance of the stackable item
		return Craft::make(item_id);
	}
	else // the item is not stackable
	{
		// destroy the container's reference to the item
		contents.erase(item_id);

		// return the local shared_ptr to the item
		return item;
	}
}
void Container::erase(const std::string & item_id, const unsigned & count)
{
	// get an iterator to the item in question
	const std::multimap<std::string, std::shared_ptr<Item>>::iterator item_it = contents.find(item_id);

	// if the item is not present in the chest, remove the item
	if (item_it == contents.cend()) return;

	// if the item is stackable
	if (std::shared_ptr<Stackable> stackable = U::convert_to<Stackable>(item_it->second))
	{
		// erase the amount required
		stackable->amount -= std::min(stackable->amount, count);

		// if the container no longer contains at least one instance of the item
		if (stackable->amount < 1)
		{
			// erase the container's reference to the item
			contents.erase(item_id);
		}
	}
	else // the item is not stackable
	{
		// for as many times as specified
		for (unsigned i = 0; i < count; ++i)
		{
			// erase the container's reference to the item
			contents.erase(item_id);
		}
	}
}

// protected

void Container::set_contents(const std::multimap<std::string, std::shared_ptr<Item>> & set_contents)
{
	contents = set_contents;
}
