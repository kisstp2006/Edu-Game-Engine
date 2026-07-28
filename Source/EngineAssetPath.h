#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace EGE
{
	inline std::string ResolveEngineShaderPath(
		std::string_view requestedPath)
	{
		std::string normalized(requestedPath);
		std::replace(
			normalized.begin(), normalized.end(), '\\', '/');

		std::string lowercase = normalized;
		std::transform(
			lowercase.begin(), lowercase.end(), lowercase.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});

		constexpr std::string_view legacyPrefix = "assets/shaders/";
		const std::size_t start =
			!lowercase.empty() && lowercase.front() == '/' ? 1 : 0;
		if (lowercase.compare(
				start, legacyPrefix.size(), legacyPrefix) == 0)
		{
			return "Engine/Shaders/" +
				normalized.substr(start + legacyPrefix.size());
		}
		return normalized;
	}
}
