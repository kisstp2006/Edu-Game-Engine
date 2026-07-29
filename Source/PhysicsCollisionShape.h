#pragma once

#include "Math.h"

class btCollisionObject;
class btCollisionShape;
class btCompoundShape;
class ComponentRigidBody;
class ComponentCollider;

namespace EGE::Physics
{
	struct CollisionShape
	{
		btCompoundShape* root = nullptr;
		btCollisionShape* child = nullptr;
	};

	[[nodiscard]] CollisionShape CreateBoxShape(
		const OBB& box,
		const float3& scale);
	[[nodiscard]] CollisionShape CreateSphereShape(
		const Sphere& sphere,
		const float3& scale);
	[[nodiscard]] CollisionShape CreateCapsuleShape(
		const Capsule& capsule,
		const float3& scale);

	void SetComponentOwner(
		btCollisionObject& object,
		ComponentRigidBody* component);
	[[nodiscard]] ComponentRigidBody* GetComponentOwner(
		const btCollisionObject& object);
	[[nodiscard]] ComponentCollider* GetColliderOwner(
		const btCollisionObject& object,
		int childIndex);
}
