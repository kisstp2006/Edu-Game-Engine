#include "ScriptApiRegistry.h"

#include <algorithm>
#include <utility>

namespace EGE
{
	bool ScriptApiRegistry::Add(
		std::string name,
		Registrar registrar,
		std::string& error)
	{
		error.clear();
		if (name.empty())
		{
			error = "A script API registrar needs a name.";
			return false;
		}
		if (!registrar)
		{
			error = "The script API registrar '" + name + "' is empty.";
			return false;
		}
		if (std::any_of(
				entries_.begin(), entries_.end(),
				[&name](const Entry& entry)
				{
					return entry.name == name;
				}))
		{
			error = "The script API registrar '" + name +
				"' is already registered.";
			return false;
		}

		entries_.push_back({std::move(name), std::move(registrar)});
		return true;
	}

	bool ScriptApiRegistry::RegisterAll(
		asIScriptEngine& engine,
		std::string& error) const
	{
		for (const Entry& entry : entries_)
		{
			if (entry.registrar(engine, error))
				continue;

			if (error.empty())
			{
				error = "The script API registrar '" + entry.name +
					"' failed.";
			}
			else
			{
				error = entry.name + ": " + error;
			}
			return false;
		}
		return true;
	}

	void ScriptApiRegistry::Clear()
	{
		entries_.clear();
	}
}
