#include "ScriptResource.h"

#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <random>
#include <string_view>

namespace EGE
{
	namespace
	{
		constexpr std::string_view AssetIdPrefix = "// EGE-ScriptId: ";

		std::string ReadTextFile(const std::filesystem::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return {
				std::istreambuf_iterator<char>(stream),
				std::istreambuf_iterator<char>()};
		}

		bool WriteTextFile(
			const std::filesystem::path& path,
			const std::string& source)
		{
			std::ofstream stream(path, std::ios::binary | std::ios::trunc);
			stream << source;
			return stream.good();
		}

		std::string FindAssetId(const std::string& source)
		{
			const std::size_t start = source.find(AssetIdPrefix);
			if (start == std::string::npos)
				return {};

			const std::size_t valueStart = start + AssetIdPrefix.size();
			const std::size_t valueEnd = source.find_first_of("\r\n", valueStart);
			return source.substr(valueStart, valueEnd - valueStart);
		}
	}

	std::string ScriptResource::CreateAssetId()
	{
		std::array<unsigned char, 16> bytes;
		std::random_device device;
		std::mt19937_64 random(
			(static_cast<std::uint64_t>(device()) << 32) ^
			static_cast<std::uint64_t>(
				std::chrono::high_resolution_clock::now()
					.time_since_epoch()
					.count()));
		for (unsigned char& byte : bytes)
			byte = static_cast<unsigned char>(random());

		bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
		bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

		constexpr char Hex[] = "0123456789abcdef";
		std::string result;
		result.reserve(36);
		for (std::size_t index = 0; index < bytes.size(); ++index)
		{
			if (index == 4 || index == 6 || index == 8 || index == 10)
				result.push_back('-');
			result.push_back(Hex[bytes[index] >> 4]);
			result.push_back(Hex[bytes[index] & 0x0F]);
		}
		return result;
	}

	bool ScriptResource::IsAssetId(const std::string& value)
	{
		if (value.size() != 36)
			return false;
		for (std::size_t index = 0; index < value.size(); ++index)
		{
			if (index == 8 || index == 13 || index == 18 || index == 23)
			{
				if (value[index] != '-')
					return false;
				continue;
			}
			const char character = value[index];
			if (!std::isxdigit(static_cast<unsigned char>(character)))
				return false;
		}
		return true;
	}

	ScriptResourceInfo ScriptResource::Read(const std::filesystem::path& path)
	{
		ScriptResourceInfo result;
		result.sourcePath = path;
		result.assetId = FindAssetId(ReadTextFile(path));
		if (!IsAssetId(result.assetId))
			result.assetId.clear();
		return result;
	}

	ScriptResourceInfo ScriptResource::ReadOrCreate(
		const std::filesystem::path& path,
		std::string& error)
	{
		error.clear();
		ScriptResourceInfo result = Read(path);
		if (result)
			return result;

		const std::string source = ReadTextFile(path);
		if (source.empty() && !std::filesystem::exists(path))
		{
			error = "The script source file could not be read.";
			return {};
		}

		result.assetId = CreateAssetId();
		if (!WriteTextFile(path, AddAssetId(source, result.assetId)))
		{
			error = "The script asset identifier could not be saved.";
			return {};
		}
		return result;
	}

	std::string ScriptResource::AddAssetId(
		const std::string& source,
		const std::string& assetId)
	{
		return std::string(AssetIdPrefix) + assetId + "\n" + source;
	}
}
