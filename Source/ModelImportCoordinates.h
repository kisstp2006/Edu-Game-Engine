#pragma once

#include "Math.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace EGE::ModelImportCoordinates
{
	inline bool IsGlTf(const std::filesystem::path& source)
	{
		std::string extension = source.extension().string();
		std::transform(
			extension.begin(),
			extension.end(),
			extension.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		return extension == ".gltf" || extension == ".glb";
	}

	inline float3 Position(
		const float3& value,
		const float3& scale,
		bool convertGlTfCoordinates)
	{
		return float3(
			value.x * scale.x *
				(convertGlTfCoordinates ? -1.0f : 1.0f),
			value.y * scale.y,
			value.z * scale.z);
	}

	inline float3 Normal(
		const float3& value,
		const float3& scale,
		bool convertGlTfCoordinates)
	{
		return float3(
			value.x / scale.x *
				(convertGlTfCoordinates ? -1.0f : 1.0f),
			value.y / scale.y,
			value.z / scale.z).Normalized();
	}

	inline float4 Tangent(
		const float4& value,
		bool convertGlTfCoordinates)
	{
		if (!convertGlTfCoordinates)
			return value;
		return float4(-value.x, value.y, value.z, -value.w);
	}

	inline float3 MorphDirection(
		const float3& value,
		const float3& scale,
		bool convertGlTfCoordinates)
	{
		return float3(
			value.x / scale.x *
				(convertGlTfCoordinates ? -1.0f : 1.0f),
			value.y / scale.y,
			value.z / scale.z);
	}

	inline Quat Rotation(
		const Quat& value,
		bool convertGlTfCoordinates)
	{
		if (!convertGlTfCoordinates)
			return value;
		return Quat(value.x, -value.y, -value.z, value.w);
	}

	inline float4x4 Transform(
		float4x4 value,
		const float3& scale,
		bool convertGlTfCoordinates)
	{
		if (convertGlTfCoordinates)
		{
			const float signs[4] = {-1.0f, 1.0f, 1.0f, 1.0f};
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
					value.At(row, column) *= signs[row] * signs[column];
			}
		}

		value.SetTranslatePart(Position(
			value.TranslatePart(),
			scale,
			false));
		return value;
	}

	inline void ReverseTriangleWinding(
		unsigned* indices,
		unsigned count,
		bool convertGlTfCoordinates)
	{
		if (!convertGlTfCoordinates || !indices)
			return;
		for (unsigned index = 0; index + 2 < count; index += 3)
			std::swap(indices[index + 1], indices[index + 2]);
	}
}
