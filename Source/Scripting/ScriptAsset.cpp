#include "ScriptAsset.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace EGE
{
	namespace
	{
		bool IsInside(
			const std::filesystem::path& child,
			const std::filesystem::path& parent)
		{
			const std::filesystem::path relative =
				child.lexically_relative(parent);
			if (relative.empty())
				return child == parent;
			const std::string text = relative.generic_string();
			return text != ".." && !text.starts_with("../");
		}

		std::string Lowercase(std::string text)
		{
			for (char& character : text)
			{
				character = static_cast<char>(
					std::tolower(static_cast<unsigned char>(character)));
			}
			return text;
		}
	}

	ScriptAssetCreationResult ScriptAsset::Create(
		const std::filesystem::path& projectRoot,
		const std::string& sourcePath,
		const std::string& assetName)
	{
		ScriptAssetCreationResult result;
		const std::string className = MakeClassName(assetName);
		if (className.empty())
		{
			result.error =
				"The asset name cannot produce a valid script class name.";
			return result;
		}

		const std::filesystem::path relative(sourcePath);
		if (relative.is_absolute() ||
			Lowercase(relative.extension().string()) != ".as")
		{
			result.error = "AngelScript assets must use a relative .as path.";
			return result;
		}

		std::error_code fileError;
		const std::filesystem::path root =
			std::filesystem::absolute(projectRoot, fileError)
				.lexically_normal();
		const std::filesystem::path assetsRoot =
			(root / "Assets").lexically_normal();
		const std::filesystem::path destination =
			(root / relative).lexically_normal();
		if (fileError || !IsInside(destination, assetsRoot))
		{
			result.error =
				"The script destination must be inside the project Assets folder.";
			return result;
		}
		if (std::filesystem::exists(destination, fileError))
		{
			result.error = "An asset with this name already exists.";
			return result;
		}

		std::filesystem::create_directories(
			destination.parent_path(), fileError);
		if (fileError)
		{
			result.error =
				"The script directory could not be created: " +
				fileError.message();
			return result;
		}

		std::ofstream stream(
			destination,
			std::ios::binary | std::ios::trunc);
		if (!stream)
		{
			result.error = "The script file could not be created.";
			return result;
		}
		stream << BuildTemplate(className);
		if (!stream.good())
		{
			result.error = "The script file could not be written.";
			return result;
		}

		result.sourcePath = relative.generic_string();
		result.className = className;
		return result;
	}

	std::string ScriptAsset::MakeClassName(
		const std::string& assetName)
	{
		std::string result;
		bool capitalize = true;
		for (unsigned char character : assetName)
		{
			if (!std::isalnum(character))
			{
				capitalize = true;
				continue;
			}

			if (result.empty() && std::isdigit(character))
				result += "Script";
			result.push_back(
				capitalize
					? static_cast<char>(std::toupper(character))
					: static_cast<char>(character));
			capitalize = false;
		}
		return result;
	}

	std::string ScriptAsset::BuildTemplate(
		const std::string& className)
	{
		std::ostringstream source;
		source
			<< "[ScriptComponent]\n"
			<< "class " << className << "\n"
			<< "{\n"
			<< "    [Header(\"Properties\")]\n"
			<< "    [SerializeField]\n"
			<< "    private float speed = 1.0f;\n\n"
			<< "    void OnStart()\n"
			<< "    {\n"
			<< "    }\n\n"
			<< "    void OnUpdate(float deltaTime)\n"
			<< "    {\n"
			<< "    }\n\n"
			<< "    void OnStop()\n"
			<< "    {\n"
			<< "    }\n"
			<< "}\n";
		return source.str();
	}
}
