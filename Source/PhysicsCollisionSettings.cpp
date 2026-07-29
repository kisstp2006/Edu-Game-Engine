#include "Globals.h"
#include "PhysicsCollisionSettings.h"

#include "Config.h"

#include <algorithm>

namespace EGE::Physics
{
	void CollisionSettings::Save(Config& config) const
	{
		config.AddUInt("Collision Layer", layer);
		config.AddUInt("Collision Mask", mask);
	}

	void CollisionSettings::Load(Config& config)
	{
		layer = config.GetUInt("Collision Layer", 0);
		mask = config.GetUInt(
			"Collision Mask", AllCollisionLayers);
		Normalize();
	}

	void CollisionSettings::Normalize()
	{
		layer = std::min(
			layer,
			CollisionLayerCount - 1);
		mask &= AllCollisionLayers;
	}

}
