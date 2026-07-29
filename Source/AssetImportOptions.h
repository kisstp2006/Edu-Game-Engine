#pragma once

#include "Globals.h"
#include "Math.h"

#ifdef CreateDirectory
#undef CreateDirectory
#endif

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace EGE
{
	struct ModelImportOptions
	{
		std::string assetName;
		float3 scale = float3::one;
		bool importMaterials = true;
		bool generateNormals = true;
		bool generateTangents = true;
		bool weldVertices = true;
		bool optimizeMeshes = true;
		bool flipUVs = false;
		bool convertGlTfCoordinates = true;
	};

	struct TextureImportOptions
	{
		bool generateMipmaps = true;
		bool sRgb = true;
		bool convertToCubemap = false;
		std::uint32_t cubemapFaceSize = 512;
		float2 maximumSize = float2::zero;
		float4 colorMultiplier = float4::one;
	};

	enum class AudioImportMode : std::int64_t
	{
		Automatic,
		Sample,
		Stream
	};

	struct AudioImportOptions
	{
		std::string assetName;
		AudioImportMode mode = AudioImportMode::Automatic;
		bool loop = false;
		float volume = 1.0f;
		float pitch = 1.0f;
		bool spatial = false;
		float2 distanceRange = float2(1.0f, 100.0f);
	};

	struct AnimationClipImportRange
	{
		std::string name;
		std::uint32_t firstFrame = 0;
		std::uint32_t lastFrame =
			(std::numeric_limits<std::uint32_t>::max)();
	};

	struct AnimationImportOptions
	{
		float3 scale = float3::one;
		bool importMorphTargets = true;
		bool convertGlTfCoordinates = true;
		std::vector<AnimationClipImportRange> clips;
	};
}
