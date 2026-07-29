#include "../PhysicsCollisionShape.h"
#include "../PhysicsQuery.h"

#include <btBulletDynamicsCommon.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
	bool NearlyEqual(float left, float right)
	{
		return std::abs(left - right) < 0.0001f;
	}

	bool NearlyEqual(
		const btVector3& left,
		const btVector3& right)
	{
		return (left - right).length2() < 0.0000001f;
	}

	bool Expect(bool condition, const char* message)
	{
		if (condition)
			return true;
		std::cerr << message << '\n';
		return false;
	}

	void Destroy(EGE::Physics::CollisionShape shape)
	{
		delete shape.root;
		delete shape.child;
	}
}

int main()
{
	OBB box;
	box.pos = float3(1.0f, -2.0f, 0.5f);
	box.r = float3(1.0f, 2.0f, 3.0f);
	const Quat boxRotation = Quat::RotateZ(pi * 0.5f);
	box.axis[0] = boxRotation.Transform(float3::unitX);
	box.axis[1] = boxRotation.Transform(float3::unitY);
	box.axis[2] = boxRotation.Transform(float3::unitZ);

	const EGE::Physics::CollisionShape boxShape =
		EGE::Physics::CreateBoxShape(
			box,
			float3(-2.0f, 3.0f, 4.0f));
	auto* bulletBox =
		static_cast<btBoxShape*>(boxShape.child);
	if (!Expect(
			NearlyEqual(
				boxShape.root->getChildTransform(0).getOrigin(),
				btVector3(-2.0f, -6.0f, 2.0f)),
			"Box center did not include signed scale.") ||
		!Expect(
			NearlyEqual(
				bulletBox->getHalfExtentsWithMargin(),
				btVector3(2.0f, 6.0f, 12.0f)),
			"Box half extents did not include absolute scale.") ||
		!Expect(
			NearlyEqual(
				quatRotate(
					boxShape.root->getChildTransform(0).getRotation(),
					btVector3(1.0f, 0.0f, 0.0f)),
				btVector3(0.0f, 1.0f, 0.0f)),
			"Box orientation was not passed to Bullet."))
	{
		Destroy(boxShape);
		return EXIT_FAILURE;
	}
	Destroy(boxShape);

	Sphere sphere;
	sphere.pos = float3(1.0f, 2.0f, 3.0f);
	sphere.r = 0.5f;
	const EGE::Physics::CollisionShape sphereShape =
		EGE::Physics::CreateSphereShape(
			sphere,
			float3(-2.0f, 3.0f, 4.0f));
	auto* bulletSphere =
		static_cast<btSphereShape*>(sphereShape.child);
	if (!Expect(
			NearlyEqual(
				sphereShape.root->getChildTransform(0).getOrigin(),
				btVector3(-2.0f, 6.0f, 12.0f)),
			"Sphere center did not include signed scale.") ||
		!Expect(
			NearlyEqual(bulletSphere->getRadius(), 2.0f),
			"Sphere radius did not include scale."))
	{
		Destroy(sphereShape);
		return EXIT_FAILURE;
	}
	Destroy(sphereShape);

	Capsule capsule;
	capsule.l = LineSegment(
		float3(1.0f, 0.0f, 0.0f),
		float3(1.0f, 0.0f, 2.0f));
	capsule.r = 0.25f;
	const EGE::Physics::CollisionShape capsuleShape =
		EGE::Physics::CreateCapsuleShape(
			capsule,
			float3(2.0f, 3.0f, 4.0f));
	auto* bulletCapsule =
		static_cast<btCapsuleShape*>(capsuleShape.child);
	const btTransform& capsuleTransform =
		capsuleShape.root->getChildTransform(0);
	if (!Expect(
			NearlyEqual(
				capsuleTransform.getOrigin(),
				btVector3(2.0f, 0.0f, 4.0f)),
			"Capsule center was not passed to Bullet.") ||
		!Expect(
			NearlyEqual(
				quatRotate(
					capsuleTransform.getRotation(),
					btVector3(0.0f, 1.0f, 0.0f)),
				btVector3(0.0f, 0.0f, 1.0f)),
			"Capsule direction was not passed to Bullet.") ||
		!Expect(
			NearlyEqual(bulletCapsule->getHalfHeight(), 4.0f),
			"Capsule length did not include scale.") ||
		!Expect(
			NearlyEqual(bulletCapsule->getRadius(), 1.0f),
			"Capsule radius did not include scale."))
	{
		Destroy(capsuleShape);
		return EXIT_FAILURE;
	}
	Destroy(capsuleShape);

	btCollisionObject object;
	auto* owner = reinterpret_cast<ComponentRigidBody*>(0x1234);
	EGE::Physics::SetComponentOwner(object, owner);
	if (!Expect(
		EGE::Physics::GetComponentOwner(object) == owner,
		"Component rigid body owner tagging was not preserved."))
	{
		return EXIT_FAILURE;
	}

	btCompoundShape compound;
	btSphereShape firstChild(1.0f);
	btSphereShape secondChild(1.0f);
	auto* firstCollider =
		reinterpret_cast<ComponentCollider*>(0x2345);
	auto* secondCollider =
		reinterpret_cast<ComponentCollider*>(0x3456);
	firstChild.setUserPointer(firstCollider);
	secondChild.setUserPointer(secondCollider);
	btTransform firstTransform = btTransform::getIdentity();
	btTransform secondTransform = btTransform::getIdentity();
	secondTransform.setOrigin(btVector3(3.0f, 0.0f, 0.0f));
	compound.addChildShape(firstTransform, &firstChild);
	compound.addChildShape(secondTransform, &secondChild);
	object.setCollisionShape(&compound);
	if (!Expect(
			EGE::Physics::GetColliderOwner(object, 0) ==
				firstCollider,
			"First compound child lost its Collider owner.") ||
		!Expect(
			EGE::Physics::GetColliderOwner(object, 1) ==
				secondCollider,
			"Second compound child lost its Collider owner.") ||
		!Expect(
			EGE::Physics::GetColliderOwner(object, 2) == nullptr,
			"Invalid compound child resolved to a Collider."))
	{
		return EXIT_FAILURE;
	}

	btDefaultCollisionConfiguration configuration;
	btCollisionDispatcher dispatcher(&configuration);
	btDbvtBroadphase broadphase;
	btSequentialImpulseConstraintSolver solver;
	btDiscreteDynamicsWorld world(
		&dispatcher, &broadphase, &solver, &configuration);
	world.setGravity(btVector3(0.0f, 0.0f, 0.0f));

	btCompoundShape targetCompound;
	btSphereShape targetChild(1.0f);
	auto* targetCollider =
		reinterpret_cast<ComponentCollider*>(0x4567);
	targetChild.setUserPointer(targetCollider);
	btTransform targetTransform = btTransform::getIdentity();
	targetTransform.setOrigin(btVector3(3.5f, 0.0f, 0.0f));
	targetCompound.addChildShape(targetTransform, &targetChild);

	btDefaultMotionState compoundMotion(btTransform::getIdentity());
	btDefaultMotionState targetMotion(btTransform::getIdentity());
	btRigidBody compoundBody(
		btRigidBody::btRigidBodyConstructionInfo(
			1.0f, &compoundMotion, &compound));
	btRigidBody targetBody(
		btRigidBody::btRigidBodyConstructionInfo(
			0.0f, &targetMotion, &targetCompound));
	auto* targetBodyOwner =
		reinterpret_cast<ComponentRigidBody*>(0x5678);
	EGE::Physics::SetComponentOwner(compoundBody, owner);
	EGE::Physics::SetComponentOwner(targetBody, targetBodyOwner);
	world.addRigidBody(
		&compoundBody,
		1 << 2,
		EGE::Physics::AllCollisionLayers);
	world.addRigidBody(
		&targetBody,
		1 << 5,
		EGE::Physics::AllCollisionLayers);
	world.stepSimulation(1.0f / 60.0f);

	bool resolvedBulletChildPair = false;
	for (int manifoldIndex = 0;
		manifoldIndex < dispatcher.getNumManifolds();
		++manifoldIndex)
	{
		const btPersistentManifold* manifold =
			dispatcher.getManifoldByIndexInternal(manifoldIndex);
		const auto* objectA =
			static_cast<const btCollisionObject*>(
				manifold->getBody0());
		const auto* objectB =
			static_cast<const btCollisionObject*>(
				manifold->getBody1());
		for (int contactIndex = 0;
			contactIndex < manifold->getNumContacts();
			++contactIndex)
		{
			const btManifoldPoint& point =
				manifold->getContactPoint(contactIndex);
			if (point.getDistance() > 0.0f)
				continue;
			ComponentCollider* colliderA =
				EGE::Physics::GetColliderOwner(
					*objectA, point.m_index0);
			ComponentCollider* colliderB =
				EGE::Physics::GetColliderOwner(
					*objectB, point.m_index1);
			resolvedBulletChildPair |=
				(colliderA == secondCollider &&
					colliderB == targetCollider) ||
				(colliderA == targetCollider &&
					colliderB == secondCollider);
		}
	}
	if (!Expect(
			resolvedBulletChildPair,
			"Bullet compound child indices did not resolve to "
			"the touching Collider IDs."))
	{
		return EXIT_FAILURE;
	}

	EGE::Physics::QueryHit rayHit;
	const bool raycastLayerHit =
		EGE::Physics::Raycast(
			world,
			float3(-5.0f, 0.0f, 0.0f),
			float3::unitX,
			20.0f,
			{1u << 2, true},
			rayHit);
	if (!Expect(
			raycastLayerHit,
			"Raycast did not hit the selected collision layer.") ||
		!Expect(
			rayHit.rigidBody == owner &&
				rayHit.collider == firstCollider &&
				rayHit.distance > 0.0f &&
				rayHit.distance < 20.0f,
			"Raycast did not return the closest compound collider.") ||
		!Expect(
			!EGE::Physics::Raycast(
				world,
				float3(-5.0f, 0.0f, 0.0f),
				float3::unitX,
				20.0f,
				{1u << 7, true},
				rayHit),
			"Raycast ignored its layer mask."))
	{
		world.removeRigidBody(&compoundBody);
		world.removeRigidBody(&targetBody);
		return EXIT_FAILURE;
	}

	const std::vector<EGE::Physics::QueryHit> allHits =
		EGE::Physics::RaycastAll(
			world,
			float3(-5.0f, 0.0f, 0.0f),
			float3::unitX,
			20.0f,
			{EGE::Physics::AllCollisionLayers, true});
	if (!Expect(
			allHits.size() == 3 &&
				allHits[0].distance <= allHits[1].distance &&
				allHits[1].distance <= allHits[2].distance,
			"RaycastAll did not return every collider in distance order."))
	{
		world.removeRigidBody(&compoundBody);
		world.removeRigidBody(&targetBody);
		return EXIT_FAILURE;
	}

	EGE::Physics::QueryHit sphereHit;
	if (!Expect(
			EGE::Physics::SphereCast(
				world,
				float3(-5.0f, 0.0f, 0.0f),
				0.5f,
				float3::unitX,
				20.0f,
				{1u << 2, true},
				sphereHit),
			"SphereCast did not hit the selected collision layer.") ||
		!Expect(
			sphereHit.collider == firstCollider &&
				sphereHit.distance >= 0.0f &&
				sphereHit.distance < rayHit.distance,
			"SphereCast returned an incorrect closest hit."))
	{
		world.removeRigidBody(&compoundBody);
		world.removeRigidBody(&targetBody);
		return EXIT_FAILURE;
	}

	targetBody.setCollisionFlags(
		targetBody.getCollisionFlags() |
		btCollisionObject::CF_NO_CONTACT_RESPONSE);
	if (!Expect(
			!EGE::Physics::Raycast(
				world,
				float3(2.2f, 0.0f, 0.0f),
				float3::unitX,
				5.0f,
				{1u << 5, false},
				rayHit),
			"Raycast included a trigger when triggers were disabled.") ||
		!Expect(
			EGE::Physics::Raycast(
				world,
				float3(2.2f, 0.0f, 0.0f),
				float3::unitX,
				5.0f,
				{1u << 5, true},
				rayHit) &&
				rayHit.collider == targetCollider &&
				rayHit.isTrigger,
			"Raycast did not report an enabled trigger query."))
	{
		world.removeRigidBody(&compoundBody);
		world.removeRigidBody(&targetBody);
		return EXIT_FAILURE;
	}

	const std::vector<EGE::Physics::QueryHit> overlaps =
		EGE::Physics::OverlapSphere(
			world,
			float3(3.25f, 0.0f, 0.0f),
			1.0f,
			{EGE::Physics::AllCollisionLayers, true});
	bool foundSecondCollider = false;
	bool foundTriggerCollider = false;
	for (const EGE::Physics::QueryHit& overlap : overlaps)
	{
		foundSecondCollider |=
			overlap.collider == secondCollider;
		foundTriggerCollider |=
			overlap.collider == targetCollider &&
			overlap.isTrigger;
	}
	if (!Expect(
			foundSecondCollider && foundTriggerCollider,
			"OverlapSphere did not resolve solid and trigger colliders."))
	{
		world.removeRigidBody(&compoundBody);
		world.removeRigidBody(&targetBody);
		return EXIT_FAILURE;
	}

	world.removeRigidBody(&compoundBody);
	world.removeRigidBody(&targetBody);
	return EXIT_SUCCESS;
}
