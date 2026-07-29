#pragma once

namespace EGE::Physics
{
	struct RigidBodyDefaults final
	{
		static constexpr int SerializationVersion = 1;
		static constexpr float Mass = 1.0f;
		static constexpr float Restitution = 0.0f;
		static constexpr float Friction = 0.5f;
		static constexpr float RollingFriction = 0.0f;
		static constexpr float LinearDamping = 0.0f;
		static constexpr float AngularDamping = 0.05f;
		static constexpr float LinearSleepingThreshold = 0.05f;
		static constexpr float AngularSleepingThreshold = 0.05f;
		static constexpr float DeactivationTime = 0.8f;
	};
}
