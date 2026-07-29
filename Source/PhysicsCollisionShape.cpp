#include "Globals.h"
#include "PhysicsCollisionShape.h"

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cmath>

namespace EGE::Physics
{
	namespace
	{
		constexpr int ComponentOwnerIndex = 0x45474552;

		float3 Multiply(
			const float3& left,
			const float3& right)
		{
			return {
				left.x * right.x,
				left.y * right.y,
				left.z * right.z};
		}

		float3 Absolute(const float3& value)
		{
			return {
				std::abs(value.x),
				std::abs(value.y),
				std::abs(value.z)};
		}

		btTransform ChildTransform(
			const float3& center,
			const Quat& rotation = Quat::identity)
		{
			btTransform transform;
			transform.setIdentity();
			transform.setOrigin(center);
			transform.setRotation(rotation);
			return transform;
		}
	}

	CollisionShape CreateBoxShape(
		const OBB& box,
		const float3& scale)
	{
		auto* child = new btBoxShape(
			Multiply(box.r, Absolute(scale)));
		auto* root = new btCompoundShape();
		const Quat rotation =
			float3x3(
				box.axis[0],
				box.axis[1],
				box.axis[2]).ToQuat();
		root->addChildShape(
			ChildTransform(Multiply(box.pos, scale), rotation),
			child);
		return {root, child};
	}

	CollisionShape CreateSphereShape(
		const Sphere& sphere,
		const float3& scale)
	{
		const float3 absoluteScale = Absolute(scale);
		const float radiusScale = std::max({
			absoluteScale.x,
			absoluteScale.y,
			absoluteScale.z});
		auto* child = new btSphereShape(
			std::max(sphere.r * radiusScale, 0.001f));
		auto* root = new btCompoundShape();
		root->addChildShape(
			ChildTransform(Multiply(sphere.pos, scale)),
			child);
		return {root, child};
	}

	CollisionShape CreateCapsuleShape(
		const Capsule& capsule,
		const float3& scale)
	{
		const float3 start = Multiply(capsule.l.a, scale);
		const float3 end = Multiply(capsule.l.b, scale);
		const float3 center = (start + end) * 0.5f;
		const float3 axis = end - start;
		const float lineLength = axis.Length();
		const float3 absoluteScale = Absolute(scale);
		const float radiusScale = std::max({
			absoluteScale.x,
			absoluteScale.y,
			absoluteScale.z});
		const float radius =
			std::max(capsule.r * radiusScale, 0.001f);

		auto* child = new btCapsuleShape(radius, lineLength);
		auto* root = new btCompoundShape();
		const Quat rotation = lineLength > 0.0001f
			? Quat::RotateFromTo(float3::unitY, axis / lineLength)
			: Quat::identity;
		root->addChildShape(
			ChildTransform(center, rotation),
			child);
		return {root, child};
	}

	void SetComponentOwner(
		btCollisionObject& object,
		ComponentRigidBody* component)
	{
		object.setUserPointer(component);
		object.setUserIndex(
			component ? ComponentOwnerIndex : -1);
	}

	ComponentRigidBody* GetComponentOwner(
		const btCollisionObject& object)
	{
		if (object.getUserIndex() != ComponentOwnerIndex)
			return nullptr;
		return static_cast<ComponentRigidBody*>(
			object.getUserPointer());
	}

	ComponentCollider* GetColliderOwner(
		const btCollisionObject& object,
		int childIndex)
	{
		const btCollisionShape* shape = object.getCollisionShape();
		if (!shape)
			return nullptr;

		if (shape->isCompound())
		{
			auto* compound =
				static_cast<const btCompoundShape*>(shape);
			if (childIndex >= 0 &&
				childIndex < compound->getNumChildShapes())
			{
				shape = compound->getChildShape(childIndex);
			}
			else if (compound->getNumChildShapes() == 1)
			{
				shape = compound->getChildShape(0);
			}
			else
			{
				return nullptr;
			}
		}

		return static_cast<ComponentCollider*>(
			shape->getUserPointer());
	}
}
