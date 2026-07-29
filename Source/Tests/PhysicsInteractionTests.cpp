#include "../Config.h"
#include "../PhysicsColliderData.h"
#include "../PhysicsCollisionMatrix.h"
#include "../PhysicsCollisionSettings.h"
#include "../PhysicsContactTracker.h"
#include "../PhysicsRigidBodyDefaults.h"
#include "../Settings/SettingsStore.h"

#include <btBulletDynamicsCommon.h>

#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

namespace
{
	bool Check(bool condition, const char* message)
	{
		if (condition)
			return true;
		std::cerr << message << '\n';
		return false;
	}

	bool NearlyEqual(float left, float right)
	{
		return std::abs(left - right) < 0.0001f;
	}

	EGE::Physics::ContactObservation Observation(
		EGE::Physics::ContactKind kind,
		float separation = -0.1f,
		float impulse = 2.0f)
	{
		EGE::Physics::ContactObservation observation;
		observation.key =
			EGE::Physics::ContactKey::Make(10, 20, kind);
		observation.first.selfObjectId = 1;
		observation.first.otherObjectId = 2;
		observation.first.selfColliderId = 10;
		observation.first.otherColliderId = 20;
		observation.first.separation = separation;
		observation.first.impulse = impulse;
		observation.first.isTrigger =
			kind == EGE::Physics::ContactKind::Trigger;
		observation.second.selfObjectId = 2;
		observation.second.otherObjectId = 1;
		observation.second.selfColliderId = 20;
		observation.second.otherColliderId = 10;
		observation.second.separation = separation;
		observation.second.impulse = impulse;
		observation.second.isTrigger =
			observation.first.isTrigger;
		return observation;
	}

	struct BulletWorld
	{
		btDefaultCollisionConfiguration configuration;
		btCollisionDispatcher dispatcher{&configuration};
		btDbvtBroadphase broadphase;
		btSequentialImpulseConstraintSolver solver;
		btDiscreteDynamicsWorld world{
			&dispatcher,
			&broadphase,
			&solver,
			&configuration};

		BulletWorld()
		{
			world.setGravity(btVector3(0.0f, 0.0f, 0.0f));
		}
	};

	struct Body
	{
		btSphereShape shape{1.0f};
		btDefaultMotionState motionState;
		btRigidBody body;

		Body(float mass, const btVector3& position)
			: motionState(btTransform(
				btQuaternion::getIdentity(),
				position)),
			  body(btRigidBody::btRigidBodyConstructionInfo(
				mass,
				&motionState,
				&shape))
		{
		}
	};
}

int main()
{
	EGE::Physics::CollisionMatrix matrix;
	if (!Check(
			matrix.CanCollide(0, 15),
			"Default collision matrix did not enable all layers."))
	{
		return EXIT_FAILURE;
	}
	matrix.SetCanCollide(2, 7, false);
	if (!Check(
			!matrix.CanCollide(2, 7) &&
				!matrix.CanCollide(7, 2),
			"Collision matrix pair was not updated symmetrically.") ||
		!Check(
			(matrix.GetMask(2) & (1u << 7)) == 0 &&
				(matrix.GetMask(7) & (1u << 2)) == 0,
			"Collision matrix row masks disagree with the pair state."))
	{
		return EXIT_FAILURE;
	}

	std::array<
		std::uint32_t,
		EGE::Physics::CollisionLayerCount> asymmetricRows;
	asymmetricRows.fill(EGE::Physics::AllCollisionLayers);
	asymmetricRows[3] &= ~(1u << 9);
	matrix.SetRows(asymmetricRows);
	if (!Check(
			!matrix.CanCollide(3, 9) &&
				!matrix.CanCollide(9, 3),
			"Asymmetric persisted matrix was not normalized safely.") ||
		!Check(
			!matrix.CanCollide(
				EGE::Physics::CollisionLayerCount, 0),
			"Out-of-range collision layer was accepted."))
	{
		return EXIT_FAILURE;
	}

	const auto settingsTestRoot =
		std::filesystem::temp_directory_path() /
		("ege-physics-settings-" +
			std::to_string(
				std::chrono::steady_clock::now()
					.time_since_epoch().count()));
	const auto settingsPath =
		settingsTestRoot / "ProjectSettings.json";
	const auto schemaPath =
		std::filesystem::path(EGE_SOURCE_ROOT) /
		"Game" / "Settings" / "Schemas" /
		"ProjectSettings.schema.json";
	EGE::SettingsStore projectSettings;
	std::string settingsError;
	if (!Check(
			projectSettings.Load(
				schemaPath, settingsPath, settingsError),
			"Project collision matrix settings could not be loaded.") ||
		!Check(
			projectSettings.GetInt(
				"physics.collision_matrix_0", 0) ==
				static_cast<int>(
					EGE::Physics::AllCollisionLayers),
			"Collision matrix did not default to all layers."))
	{
		std::filesystem::remove_all(settingsTestRoot);
		return EXIT_FAILURE;
	}

	const int persistedRow = static_cast<int>(
		EGE::Physics::AllCollisionLayers & ~(1u << 6));
	if (!Check(
			projectSettings.SetValue(
				"physics.collision_matrix_0", persistedRow) &&
				projectSettings.SetValue(
					"physics.collision_matrix_6",
					static_cast<int>(
						EGE::Physics::AllCollisionLayers &
						~(1u << 0))) &&
				projectSettings.Save(settingsError),
			"Collision matrix rows could not be saved to the project."))
	{
		std::filesystem::remove_all(settingsTestRoot);
		return EXIT_FAILURE;
	}

	EGE::SettingsStore reloadedSettings;
	bool matrixRowsHidden = false;
	if (reloadedSettings.Load(
			schemaPath, settingsPath, settingsError))
	{
		for (const EGE::SettingCategory& category :
			reloadedSettings.GetCategories())
		{
			for (const EGE::SettingDefinition& definition :
				category.settings)
			{
				if (definition.id ==
					"physics.collision_matrix_0")
				{
					matrixRowsHidden = definition.editorHidden;
				}
			}
		}
	}
	if (!Check(
			reloadedSettings.GetInt(
				"physics.collision_matrix_0", -1) == persistedRow,
			"Project collision matrix row did not survive reload.") ||
		!Check(
			matrixRowsHidden,
			"Raw collision matrix rows were not hidden from the "
			"generic settings UI."))
	{
		std::filesystem::remove_all(settingsTestRoot);
		return EXIT_FAILURE;
	}
	std::filesystem::remove_all(settingsTestRoot);

	EGE::Physics::ColliderData colliderData;
	colliderData.shape = EGE::Physics::ColliderShape::Capsule;
	colliderData.isTrigger = true;
	colliderData.sphere.pos = float3(1.0f, 2.0f, 3.0f);
	colliderData.sphere.r = 4.0f;
	colliderData.box.pos = float3(-1.0f, -2.0f, -3.0f);
	colliderData.box.r = float3(2.0f, 3.0f, 4.0f);
	colliderData.boxRotation = float3(0.1f, 0.2f, 0.3f);
	colliderData.capsule.l = LineSegment(
		float3(2.0f, 3.0f, 4.0f),
		float3(5.0f, 6.0f, 7.0f));
	colliderData.capsule.r = 0.75f;
	colliderData.Normalize();

	Config savedCollider;
	colliderData.Save(savedCollider);
	char* colliderJson = nullptr;
	savedCollider.Save(&colliderJson, nullptr);
	Config loadedColliderConfig(colliderJson);
	delete[] colliderJson;
	EGE::Physics::ColliderData loadedCollider;
	loadedCollider.Load(loadedColliderConfig);
	if (!Check(
			loadedCollider.shape ==
				EGE::Physics::ColliderShape::Capsule,
			"Collider shape did not survive serialization.") ||
		!Check(
			loadedCollider.isTrigger,
			"Collider trigger state did not survive serialization.") ||
		!Check(
			loadedCollider.sphere.pos.Equals(
				colliderData.sphere.pos) &&
				NearlyEqual(
					loadedCollider.sphere.r,
					colliderData.sphere.r),
			"Sphere collider data did not survive serialization.") ||
		!Check(
			loadedCollider.box.pos.Equals(colliderData.box.pos) &&
				loadedCollider.box.r.Equals(colliderData.box.r) &&
				loadedCollider.boxRotation.Equals(
					colliderData.boxRotation),
			"Box collider data did not survive serialization.") ||
		!Check(
			loadedCollider.capsule.l.a.Equals(
				colliderData.capsule.l.a) &&
				loadedCollider.capsule.l.b.Equals(
					colliderData.capsule.l.b) &&
				NearlyEqual(
					loadedCollider.capsule.r,
					colliderData.capsule.r),
			"Capsule collider data did not survive serialization."))
	{
		return EXIT_FAILURE;
	}

	Config legacyCollider;
	legacyCollider.AddInt("Body Type", 1);
	legacyCollider.AddBool("Is Trigger", true);
	const float legacySphere[] = {3.0f, 2.0f, 1.0f, 2.5f};
	const float legacyBox[] = {
		1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
	const float legacyCapsule[] = {
		0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 0.5f};
	legacyCollider.AddArrayFloat("Sphere", legacySphere, 4);
	legacyCollider.AddArrayFloat("Box", legacyBox, 6);
	legacyCollider.AddFloat3(
		"Box Rotation", float3(0.4f, 0.5f, 0.6f));
	legacyCollider.AddArrayFloat(
		"Capsule", legacyCapsule, 7);
	EGE::Physics::ColliderData migratedCollider;
	migratedCollider.LoadLegacyRigidBody(legacyCollider);
	if (!Check(
			migratedCollider.shape ==
				EGE::Physics::ColliderShape::Box,
			"Legacy RigidBody shape was not migrated.") ||
		!Check(
			migratedCollider.isTrigger &&
				migratedCollider.box.pos.Equals(
					float3(1.0f, 2.0f, 3.0f)) &&
				migratedCollider.box.r.Equals(
					float3(4.0f, 5.0f, 6.0f)),
			"Legacy collider geometry or trigger state was lost.") ||
		!Check(
			migratedCollider.boxRotation.Equals(
				float3(0.4f, 0.5f, 0.6f)),
			"Legacy box orientation was lost during migration."))
	{
		return EXIT_FAILURE;
	}

	EGE::Physics::CollisionSettings settings;
	settings.layer = 7;
	settings.mask = (1u << 2) | (1u << 7);

	Config saved;
	settings.Save(saved);
	char* json = nullptr;
	saved.Save(&json, nullptr);
	Config loadedConfig(json);
	delete[] json;

	EGE::Physics::CollisionSettings loaded;
	loaded.Load(loadedConfig);
	if (!Check(
			loaded.layer == settings.layer,
			"Collision layer did not survive serialization.") ||
		!Check(
			loaded.mask == settings.mask,
			"Collision mask did not survive serialization."))
	{
		return EXIT_FAILURE;
	}

	loaded.layer = 100;
	loaded.mask = 0xFFFFFFFFu;
	loaded.Normalize();
	if (!Check(
			loaded.layer == 15,
			"Collision layer was not clamped to Bullet's range.") ||
		!Check(
			loaded.mask == EGE::Physics::AllCollisionLayers,
			"Collision mask was not limited to 16 layers."))
	{
		return EXIT_FAILURE;
	}

	EGE::Physics::ColliderData rotatedBox;
	rotatedBox.boxRotation =
		float3(0.0f, 0.0f, pi * 0.5f);
	rotatedBox.UpdateBoxAxes();
	if (!Check(
			rotatedBox.box.axis[0].Equals(
				float3::unitY, 0.0001f),
			"Serialized box rotation did not update the OBB axes."))
	{
		return EXIT_FAILURE;
	}

	EGE::Physics::ContactTracker tracker;
	auto events = tracker.Update(
		{Observation(EGE::Physics::ContactKind::Collision)});
	if (!Check(events.size() == 2, "Collision Enter was not paired.") ||
		!Check(
			events[0].phase == EGE::Physics::ContactPhase::Enter &&
				events[1].phase == EGE::Physics::ContactPhase::Enter,
			"First collision was not Enter."))
	{
		return EXIT_FAILURE;
	}

	events = tracker.Update(
		{Observation(EGE::Physics::ContactKind::Collision)});
	if (!Check(
			events.size() == 2 &&
				events[0].phase == EGE::Physics::ContactPhase::Stay &&
				events[1].phase == EGE::Physics::ContactPhase::Stay,
			"Persistent collision was not Stay."))
	{
		return EXIT_FAILURE;
	}

	events = tracker.Update({});
	if (!Check(
			events.size() == 2 &&
				events[0].phase == EGE::Physics::ContactPhase::Exit &&
				events[1].phase == EGE::Physics::ContactPhase::Exit,
			"Removed collision was not Exit."))
	{
		return EXIT_FAILURE;
	}

	(void)tracker.Update(
		{Observation(EGE::Physics::ContactKind::Collision)});
	events = tracker.Update(
		{Observation(EGE::Physics::ContactKind::Trigger)});
	int triggerEnterCount = 0;
	int collisionExitCount = 0;
	for (const EGE::Physics::ContactEvent& event : events)
	{
		if (event.info.isTrigger &&
			event.phase == EGE::Physics::ContactPhase::Enter)
		{
			++triggerEnterCount;
		}
		if (!event.info.isTrigger &&
			event.phase == EGE::Physics::ContactPhase::Exit)
		{
			++collisionExitCount;
		}
	}
	if (!Check(
		triggerEnterCount == 2 && collisionExitCount == 2,
		"Switching to trigger did not emit Exit then Enter."))
	{
		return EXIT_FAILURE;
	}

	{
		BulletWorld bullet;
		Body first(0.0f, btVector3(0.0f, 0.0f, 0.0f));
		Body second(0.0f, btVector3(0.5f, 0.0f, 0.0f));
		EGE::Physics::CollisionMatrix filterMatrix;
		filterMatrix.SetCanCollide(0, 1, false);
		bullet.world.addRigidBody(
			&first.body,
			1 << 0,
			static_cast<short>(filterMatrix.GetMask(0)));
		bullet.world.addRigidBody(
			&second.body,
			1 << 1,
			static_cast<short>(filterMatrix.GetMask(1)));
		bullet.world.stepSimulation(1.0f / 60.0f);
		if (!Check(
			bullet.dispatcher.getNumManifolds() == 0,
			"Layer masks did not filter the Bullet pair."))
		{
			return EXIT_FAILURE;
		}
		bullet.world.removeRigidBody(&first.body);
		bullet.world.removeRigidBody(&second.body);
	}

	{
		BulletWorld bullet;
		Body first(1.0f, btVector3(0.0f, 0.0f, 0.0f));
		Body trigger(0.0f, btVector3(0.5f, 0.0f, 0.0f));
		trigger.body.setCollisionFlags(
			trigger.body.getCollisionFlags() |
			btCollisionObject::CF_NO_CONTACT_RESPONSE);
		bullet.world.addRigidBody(&first.body, 1 << 0, 1 << 1);
		bullet.world.addRigidBody(&trigger.body, 1 << 1, 1 << 0);
		bullet.world.stepSimulation(1.0f / 60.0f);

		bool touching = false;
		for (int index = 0;
			index < bullet.dispatcher.getNumManifolds();
			++index)
		{
			btPersistentManifold* manifold =
				bullet.dispatcher.getManifoldByIndexInternal(index);
			for (int contact = 0;
				contact < manifold->getNumContacts();
				++contact)
			{
				touching |=
					manifold->getContactPoint(contact).getDistance() <=
					0.0f;
			}
		}
		if (!Check(
			touching,
			"Trigger did not produce a Bullet contact manifold.") ||
			!Check(
				NearlyEqual(
					first.body.getWorldTransform()
						.getOrigin().x(),
					0.0f),
				"Trigger produced a physical response."))
		{
			return EXIT_FAILURE;
		}
		bullet.world.removeRigidBody(&first.body);
		bullet.world.removeRigidBody(&trigger.body);
	}

	{
		BulletWorld bullet;
		btSphereShape solidShape(1.0f);
		btSphereShape triggerShape(1.0f);
		btSphereShape externalShape(1.0f);
		btCompoundShape solidCompound;
		btCompoundShape triggerCompound;
		const btTransform identity = btTransform::getIdentity();
		solidCompound.addChildShape(identity, &solidShape);
		triggerCompound.addChildShape(identity, &triggerShape);

		btDefaultMotionState solidMotion(identity);
		btDefaultMotionState triggerMotion(identity);
		btDefaultMotionState externalMotion(identity);
		btRigidBody solidBody(
			btRigidBody::btRigidBodyConstructionInfo(
				1.0f, &solidMotion, &solidCompound));
		btRigidBody triggerBody(
			btRigidBody::btRigidBodyConstructionInfo(
				0.0f, &triggerMotion, &triggerCompound));
		btRigidBody externalBody(
			btRigidBody::btRigidBodyConstructionInfo(
				0.0f, &externalMotion, &externalShape));
		triggerBody.setCollisionFlags(
			(triggerBody.getCollisionFlags() &
				~(btCollisionObject::CF_STATIC_OBJECT |
					btCollisionObject::CF_KINEMATIC_OBJECT)) |
			btCollisionObject::CF_NO_CONTACT_RESPONSE);
		triggerBody.setActivationState(DISABLE_DEACTIVATION);
		solidBody.setIgnoreCollisionCheck(&triggerBody, true);
		triggerBody.setIgnoreCollisionCheck(&solidBody, true);

		bullet.world.addRigidBody(&solidBody);
		bullet.world.addRigidBody(&triggerBody);
		bullet.world.addRigidBody(&externalBody);
		bullet.world.stepSimulation(1.0f / 60.0f);

		int externalSolidContacts = 0;
		int externalTriggerContacts = 0;
		int selfContacts = 0;
		for (int index = 0;
			index < bullet.dispatcher.getNumManifolds();
			++index)
		{
			const btPersistentManifold* manifold =
				bullet.dispatcher.getManifoldByIndexInternal(index);
			const auto* first = static_cast<const btCollisionObject*>(
				manifold->getBody0());
			const auto* second = static_cast<const btCollisionObject*>(
				manifold->getBody1());
			const bool solidTrigger =
				(first == &solidBody && second == &triggerBody) ||
				(first == &triggerBody && second == &solidBody);
			const bool solidExternal =
				(first == &solidBody && second == &externalBody) ||
				(first == &externalBody && second == &solidBody);
			const bool triggerExternal =
				(first == &triggerBody && second == &externalBody) ||
				(first == &externalBody && second == &triggerBody);
			selfContacts += solidTrigger ? 1 : 0;
			externalSolidContacts += solidExternal ? 1 : 0;
			externalTriggerContacts += triggerExternal ? 1 : 0;
		}
		if (!Check(
			selfContacts == 0,
			"Solid and trigger bodies of one RigidBody collided.") ||
		!Check(
			externalSolidContacts > 0 &&
				externalTriggerContacts > 0,
			"Mixed solid/trigger compound did not observe an "
			"external static body."))
		{
			return EXIT_FAILURE;
		}

		bullet.world.removeRigidBody(&solidBody);
		bullet.world.removeRigidBody(&triggerBody);
		bullet.world.removeRigidBody(&externalBody);
	}

	{
		BulletWorld bullet;
		bullet.world.setGravity(btVector3(0.0f, -10.0f, 0.0f));

		btBoxShape floorShape(btVector3(5.0f, 0.5f, 5.0f));
		btSphereShape fallingShape(0.5f);
		const btTransform floorTransform(
			btQuaternion::getIdentity(),
			btVector3(0.0f, -0.5f, 0.0f));
		const btTransform fallingTransform(
			btQuaternion::getIdentity(),
			btVector3(0.0f, 5.0f, 0.0f));
		btDefaultMotionState floorMotion(floorTransform);
		btDefaultMotionState fallingMotion(fallingTransform);
		btRigidBody floorBody(
			btRigidBody::btRigidBodyConstructionInfo(
				0.0f, &floorMotion, &floorShape));
		btVector3 inertia(0.0f, 0.0f, 0.0f);
		fallingShape.calculateLocalInertia(
			EGE::Physics::RigidBodyDefaults::Mass,
			inertia);
		btRigidBody fallingBody(
			btRigidBody::btRigidBodyConstructionInfo(
				EGE::Physics::RigidBodyDefaults::Mass,
				&fallingMotion,
				&fallingShape,
				inertia));

		for (btRigidBody* body : {&floorBody, &fallingBody})
		{
			body->setRestitution(
				EGE::Physics::RigidBodyDefaults::Restitution);
			body->setFriction(
				EGE::Physics::RigidBodyDefaults::Friction);
			body->setRollingFriction(
				EGE::Physics::RigidBodyDefaults::RollingFriction);
		}
		fallingBody.setDamping(
			EGE::Physics::RigidBodyDefaults::LinearDamping,
			EGE::Physics::RigidBodyDefaults::AngularDamping);
		fallingBody.setSleepingThresholds(
			EGE::Physics::RigidBodyDefaults::
				LinearSleepingThreshold,
			EGE::Physics::RigidBodyDefaults::
				AngularSleepingThreshold);
		fallingBody.setDeactivationTime(
			EGE::Physics::RigidBodyDefaults::DeactivationTime);

		bullet.world.addRigidBody(&floorBody);
		bullet.world.addRigidBody(&fallingBody);
		for (int frame = 0; frame < 600; ++frame)
		{
			bullet.world.stepSimulation(
				1.0f / 60.0f,
				1,
				1.0f / 60.0f);
		}

		if (!Check(
				std::abs(
					fallingBody.getLinearVelocity().y()) < 0.01f,
				"Default dynamic body kept bouncing vertically.") ||
			!Check(
				fallingBody.getActivationState() ==
					ISLAND_SLEEPING,
				"Default dynamic body did not enter sleeping state."))
		{
			return EXIT_FAILURE;
		}

		bullet.world.removeRigidBody(&fallingBody);
		bullet.world.removeRigidBody(&floorBody);
	}

	return EXIT_SUCCESS;
}
