#pragma once

#include "Math.h"

#include <cstdint>

namespace EGE::Physics
{
	inline constexpr std::uint32_t CollisionLayerCount = 16;
	inline constexpr std::uint32_t AllCollisionLayers = 0xFFFFu;

	enum class ContactKind : std::uint8_t
	{
		Collision,
		Trigger
	};

	enum class ContactPhase : std::uint8_t
	{
		Enter,
		Stay,
		Exit
	};

	struct CollisionInfo
	{
		std::uint32_t selfObjectId = 0;
		std::uint32_t otherObjectId = 0;
		std::uint32_t selfColliderId = 0;
		std::uint32_t otherColliderId = 0;
		std::uint32_t otherLayer = 0;
		float3 point = float3::zero;
		float3 normal = float3::zero;
		float separation = 0.0f;
		float impulse = 0.0f;
		bool isTrigger = false;
	};
}
