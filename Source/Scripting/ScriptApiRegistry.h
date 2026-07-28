#pragma once

#include <functional>
#include <string>
#include <vector>

class asIScriptEngine;

namespace EGE
{
	class ScriptApiRegistry final
	{
	public:
		using Registrar = std::function<bool(asIScriptEngine&, std::string&)>;

		[[nodiscard]] bool Add(
			std::string name,
			Registrar registrar,
			std::string& error);
		[[nodiscard]] bool RegisterAll(
			asIScriptEngine& engine,
			std::string& error) const;
		void Clear();

	private:
		struct Entry
		{
			std::string name;
			Registrar registrar;
		};

		std::vector<Entry> entries_;
	};
}
