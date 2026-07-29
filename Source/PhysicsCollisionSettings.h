#pragma once

#include "PhysicsCollision.h"

class Config;

namespace EGE::Physics
{
	struct CollisionSettings
	{
		std::uint32_t layer = 0;
		std::uint32_t mask = AllCollisionLayers;

		void Save(Config& config) const;
		void Load(Config& config);
		void Normalize();
	};
}
