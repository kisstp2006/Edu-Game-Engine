#pragma once

#include "Math.h"
#include "PhysicsCollision.h"

#include <vector>

class btCollisionWorld;
class ComponentCollider;
class ComponentRigidBody;

namespace EGE::Physics
{
	struct QueryFilter
	{
		std::uint32_t layerMask = AllCollisionLayers;
		bool includeTriggers = true;
	};

	struct QueryHit
	{
		ComponentRigidBody* rigidBody = nullptr;
		ComponentCollider* collider = nullptr;
		float3 point = float3::zero;
		float3 normal = float3::zero;
		float distance = 0.0f;
		float fraction = 0.0f;
		bool isTrigger = false;
	};

	[[nodiscard]] bool Raycast(
		const btCollisionWorld& world,
		const float3& origin,
		const float3& direction,
		float maxDistance,
		const QueryFilter& filter,
		QueryHit& hit);

	[[nodiscard]] std::vector<QueryHit> RaycastAll(
		const btCollisionWorld& world,
		const float3& origin,
		const float3& direction,
		float maxDistance,
		const QueryFilter& filter);

	[[nodiscard]] bool SphereCast(
		const btCollisionWorld& world,
		const float3& origin,
		float radius,
		const float3& direction,
		float maxDistance,
		const QueryFilter& filter,
		QueryHit& hit);

	[[nodiscard]] std::vector<QueryHit> OverlapSphere(
		btCollisionWorld& world,
		const float3& center,
		float radius,
		const QueryFilter& filter);
}
