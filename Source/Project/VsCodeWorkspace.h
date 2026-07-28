#pragma once

#include <filesystem>
#include <string>

namespace EGE
{
	[[nodiscard]] bool EnsureVsCodeWorkspace(
		const std::filesystem::path& projectDirectory,
		std::string& error);

	[[nodiscard]] bool OpenVsCode(
		const std::filesystem::path& projectDirectory,
		std::string& error);

	[[nodiscard]] bool OpenVsCode(
		const std::filesystem::path& projectDirectory,
		const std::filesystem::path& filePath,
		std::string& error);

	[[nodiscard]] bool OpenVsCode(
		const std::filesystem::path& projectDirectory,
		const std::filesystem::path& filePath,
		int line,
		int column,
		std::string& error);
}
