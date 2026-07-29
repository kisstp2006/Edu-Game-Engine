#pragma once

#include "Math.h"

class Config;

namespace EGE::Physics
{
	enum class ColliderShape
	{
		Sphere,
		Box,
		Capsule
	};

	struct ColliderData
	{
		ColliderShape shape = ColliderShape::Box;
		bool isTrigger = false;
		Sphere sphere;
		OBB box;
		Capsule capsule;
		float3 boxRotation = float3::zero;

		ColliderData();
		void Save(Config& config) const;
		void Load(Config& config);
		void LoadLegacyRigidBody(Config& config);
		void Normalize();
		void UpdateBoxAxes();
	};
}
