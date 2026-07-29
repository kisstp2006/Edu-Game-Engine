#include "Globals.h"
#include "Application.h"
#include "ModulePhysics3D.h"
#include "Primitive.h"
#include "PhysBody3D.h"
#include "PhysVehicle3D.h"
#include "ComponentRigidBody.h"
#include "ComponentCollider.h"
#include "GameObject.h"
#include "ModuleLevelManager.h"
#include "ModuleDebugDraw.h"
#include "PhysicsCollisionShape.h"
#include "Config.h"
#include "DebugDraw.h"
#include "Event.h"


#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif 

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace std;

namespace
{
	float3 GetGlobalScale(const ComponentRigidBody& component)
	{
		const GameObject* gameObject = component.GetGameObject();
		if (!gameObject)
			return float3::one;

		float3 translation;
		float3 scale;
		Quat rotation;
		gameObject->GetGlobalTransformation().Decompose(
			translation, rotation, scale);
		const auto preserveSign = [](float value)
		{
			if (std::abs(value) >= 0.0001f)
				return value;
			return value < 0.0f ? -0.0001f : 0.0001f;
		};
		return {
			preserveSign(scale.x),
			preserveSign(scale.y),
			preserveSign(scale.z)};
	}

}
 
ModulePhysics3D::ModulePhysics3D(bool start_enabled) : Module("Physics", start_enabled)
{
}

// Destructor
ModulePhysics3D::~ModulePhysics3D()
{
	RELEASE(debug_draw);
	RELEASE(solver);
	RELEASE(broad_phase);
	RELEASE(dispatcher);
	RELEASE(collision_conf);
}

// Render not available yet----------------------------------
bool ModulePhysics3D::Init(Config* config)
{
	LOG("Creating 3D Physics simulation");
	bool ret = true;

	collision_conf = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(collision_conf);
	broad_phase = new btDbvtBroadphase();
	solver = new btSequentialImpulseConstraintSolver;
	debug_draw = new DebugDrawer();

	return ret;
}

// ---------------------------------------------------------
bool ModulePhysics3D::Start(Config* config)
{
	LOG("Creating Physics environment");

	world = new btDiscreteDynamicsWorld(dispatcher, broad_phase, solver, collision_conf);
	world->setDebugDrawer(debug_draw);
	world->setGravity(GRAVITY);
	vehicle_raycaster = new btDefaultVehicleRaycaster(world);

	Load(config);

	return true;
}

btRigidBody* ModulePhysics3D::AddBody(
	ComponentRigidBody* component,
	btRigidBody** triggerBody)
{
	if (!component || !triggerBody || !world)
		return nullptr;

	*triggerBody = nullptr;
	const float3 scale = GetGlobalScale(*component);
	auto* solidCompound = new btCompoundShape();
	auto* triggerCompound = new btCompoundShape();

	const auto addCollider = [&](ComponentCollider& collider)
	{
		EGE::Physics::CollisionShape source;
		switch (collider.GetShapeType())
		{
		case ComponentCollider::ShapeType::Sphere:
			source = EGE::Physics::CreateSphereShape(
				collider.GetSphere(), scale);
			break;
		case ComponentCollider::ShapeType::Box:
			source = EGE::Physics::CreateBoxShape(
				collider.GetBox(), scale);
			break;
		case ComponentCollider::ShapeType::Capsule:
			source = EGE::Physics::CreateCapsuleShape(
				collider.GetCapsule(), scale);
			break;
		}
		if (!source.root || !source.child)
			return;

		const btTransform localTransform =
			source.root->getChildTransform(0);
		source.root->removeChildShape(source.child);
		delete source.root;
		source.child->setUserPointer(&collider);
		(collider.IsTrigger()
			? triggerCompound
			: solidCompound)->addChildShape(
				localTransform, source.child);
		shapes.push_back(source.child);
	};

	for (Component* candidate : component->GetGameObject()->components)
	{
		if (!candidate ||
			candidate->flag_for_removal ||
			!candidate->IsActive() ||
			candidate->GetType() != Component::Collider)
		{
			continue;
		}
		addCollider(*static_cast<ComponentCollider*>(candidate));
	}

	btCollisionShape* simulationShape = solidCompound;
	const bool hasSolidColliders =
		solidCompound->getNumChildShapes() > 0;
	if (!hasSolidColliders)
	{
		delete solidCompound;
		simulationShape = new btEmptyShape();
	}
	shapes.push_back(simulationShape);

	const float mass =
		component->behaviour == ComponentRigidBody::dynamic
			? component->mass
			: 0.0f;
	btVector3 inertia(0.0f, 0.0f, 0.0f);
	if (mass > 0.0f)
		simulationShape->calculateLocalInertia(mass, inertia);

	btRigidBody::btRigidBodyConstructionInfo bodyInfo(
		mass, component, simulationShape, inertia);
	auto* simulationBody = new btRigidBody(bodyInfo);
	EGE::Physics::SetComponentOwner(*simulationBody, component);
	world->addRigidBody(
		simulationBody,
		hasSolidColliders ? component->GetCollisionGroupBits() : 0,
		hasSolidColliders ? GetCollisionMaskBits(*component) : 0);

	if (triggerCompound->getNumChildShapes() > 0)
	{
		shapes.push_back(triggerCompound);
		btRigidBody::btRigidBodyConstructionInfo triggerInfo(
			0.0f, component, triggerCompound);
		*triggerBody = new btRigidBody(triggerInfo);
		EGE::Physics::SetComponentOwner(**triggerBody, component);
		(*triggerBody)->setCollisionFlags(
			((*triggerBody)->getCollisionFlags() &
				~(btCollisionObject::CF_STATIC_OBJECT |
					btCollisionObject::CF_KINEMATIC_OBJECT)) |
			btCollisionObject::CF_NO_CONTACT_RESPONSE);
		(*triggerBody)->setActivationState(DISABLE_DEACTIVATION);
		world->addRigidBody(
			*triggerBody,
			component->GetCollisionGroupBits(),
			GetCollisionMaskBits(*component));
		simulationBody->setIgnoreCollisionCheck(
			*triggerBody, true);
		(*triggerBody)->setIgnoreCollisionCheck(
			simulationBody, true);
	}
	else
	{
		delete triggerCompound;
	}

	return simulationBody;
}

// ---------------------------------------------------------
PhysBody3D* ModulePhysics3D::AddBody(const PCylinder& cylinder, float mass)
{
	btCollisionShape* colShape = new btCylinderShapeX(btVector3(cylinder.height*0.5f, cylinder.radius*2, 0.0f));
	shapes.push_back(colShape);

	btTransform startTransform;
	startTransform.setIdentity();

	btVector3 localInertia(0, 0, 0);
	if(mass != 0.f)
		colShape->calculateLocalInertia(mass, localInertia);

	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, colShape, localInertia);

	btRigidBody* body = new btRigidBody(rbInfo);
	PhysBody3D* pbody = new PhysBody3D(body);

	body->setUserPointer(pbody);
	world->addRigidBody(body);
	bodies.push_back(pbody);

	return pbody;
}

// ---------------------------------------------------------
PhysBody3D* ModulePhysics3D::AddBody(const PPlane& plane)
{
	btCollisionShape* colShape = new btStaticPlaneShape(btVector3(plane.normal.x, plane.normal.y, plane.normal.z), plane.constant);
	shapes.push_back(colShape);

	btTransform startTransform;
	startTransform.setIdentity();

	btVector3 localInertia(0, 0, 0);

	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, myMotionState, colShape, localInertia);

	btRigidBody* body = new btRigidBody(rbInfo);
	PhysBody3D* pbody = new PhysBody3D(body);

	body->setUserPointer(pbody);
	world->addRigidBody(body);
	bodies.push_back(pbody);

	return pbody;
}

// ---------------------------------------------------------
PhysBody3D*	ModulePhysics3D::AddHeighField(const char* filename, int width, int length)
{
	unsigned char* heightfieldData = new unsigned char[width*length];
	{
		for(int i = 0; i<width*length; i++)
			heightfieldData[i] = 0;
	}

	FILE* heightfieldFile;
	fopen_s(&heightfieldFile, filename, "r");
	if(heightfieldFile)
	{
		int numBytes = int(fread(heightfieldData, 1, width*length, heightfieldFile));
		//btAssert(numBytes);
		if(!numBytes)
		{
			printf("couldn't read heightfield at %s\n", filename);
		}
		fclose(heightfieldFile);
	}

	//btScalar maxHeight = 20000.f;//exposes a bug
	btScalar maxHeight = 100;

	bool useFloatDatam = false;
	bool flipQuadEdges = false;

	int upIndex = 1;

	btHeightfieldTerrainShape* heightFieldShape = new btHeightfieldTerrainShape(width, length, heightfieldData, maxHeight, upIndex, useFloatDatam, flipQuadEdges);
	btVector3 mmin, mmax;
	heightFieldShape->getAabb(btTransform::getIdentity(), mmin, mmax);

	btCollisionShape* groundShape = heightFieldShape;

	heightFieldShape->setUseDiamondSubdivision(true);

	btVector3 localScaling(10, 1, 10);
	localScaling[upIndex] = 1.f;
	groundShape->setLocalScaling(localScaling);
	shapes.push_back(groundShape);

	//create ground object

	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(0.f, 49.4f, 0.f));

	btVector3 localInertia(0, 0, 0);

	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, myMotionState, groundShape, localInertia);

	btRigidBody* body = new btRigidBody(rbInfo);
	PhysBody3D* pbody = new PhysBody3D(body);

	body->setUserPointer(pbody);
	world->addRigidBody(body);
	bodies.push_back(pbody);

	return pbody;
}

// ---------------------------------------------------------
PhysVehicle3D* ModulePhysics3D::AddVehicle(const VehicleInfo& info)
{
	btCompoundShape* comShape = new btCompoundShape();
	shapes.push_back(comShape);

	btCollisionShape* colShape = new btBoxShape(btVector3(info.chassis_size.x*0.5f, info.chassis_size.y*0.5f, info.chassis_size.z*0.5f));
	shapes.push_back(colShape);

	btTransform trans;
	trans.setIdentity();
	trans.setOrigin(btVector3(info.chassis_offset.x, info.chassis_offset.y, info.chassis_offset.z));

	comShape->addChildShape(trans, colShape);

	btTransform startTransform;
	startTransform.setIdentity();

	btVector3 localInertia(0, 0, 0);
	comShape->calculateLocalInertia(info.mass, localInertia);

	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(info.mass, myMotionState, comShape, localInertia);

	btRigidBody* body = new btRigidBody(rbInfo);
	body->setContactProcessingThreshold(BT_LARGE_FLOAT);
	body->setActivationState(DISABLE_DEACTIVATION);

	world->addRigidBody(body);
	
	btRaycastVehicle::btVehicleTuning tuning;
	tuning.m_frictionSlip = info.frictionSlip;
	tuning.m_maxSuspensionForce = info.maxSuspensionForce;
	tuning.m_maxSuspensionTravelCm = info.maxSuspensionTravelCm;
	tuning.m_suspensionCompression = info.suspensionCompression;
	tuning.m_suspensionDamping = info.suspensionDamping;
	tuning.m_suspensionStiffness = info.suspensionStiffness;

	btRaycastVehicle* vehicle = new btRaycastVehicle(tuning, body, vehicle_raycaster);

	vehicle->setCoordinateSystem(0, 1, 2);

	for(int i = 0; i < info.num_wheels; ++i)
	{
		btVector3 conn(info.wheels[i].connection.x, info.wheels[i].connection.y, info.wheels[i].connection.z);
		btVector3 dir(info.wheels[i].direction.x, info.wheels[i].direction.y, info.wheels[i].direction.z);
		btVector3 axis(info.wheels[i].axis.x, info.wheels[i].axis.y, info.wheels[i].axis.z);

		vehicle->addWheel(conn, dir, axis, info.wheels[i].suspensionRestLength, info.wheels[i].radius, tuning, info.wheels[i].front);
	}
	// ---------------------

	PhysVehicle3D* pvehicle = new PhysVehicle3D(body, vehicle, info);
	world->addVehicle(vehicle);
	vehicles.push_back(pvehicle);

	return pvehicle;
}

// ---------------------------------------------------------
void ModulePhysics3D::DeleteBody(PhysBody3D* pbody)
{
	/*
	if(pbody->body && pbody->body->getMotionState())
		RELEASE( pbody->body->getMotionState());

	world->removeCollisionObject(pbody->body);

	RELEASE( pbody->body);
	pbody->body = nullptr;

	RELEASE( pbody->collision_shape);
	pbody->collision_shape = nullptr;
	*/
	// TODO: remove from the array "bodies"
}

// ---------------------------------------------------------
void ModulePhysics3D::DeleteBody(btRigidBody * body)
{
	if (!body)
		return;

	if (world)
		world->removeRigidBody(body);

	btCollisionShape* shape = body->getCollisionShape();
	delete body;
	DeleteCollisionShape(shape);
}

void ModulePhysics3D::DeleteCollisionShape(btCollisionShape* shape)
{
	if (!shape)
		return;

	std::vector<btCollisionShape*> children;
	if (shape->isCompound())
	{
		auto* compound = static_cast<btCompoundShape*>(shape);
		children.reserve(compound->getNumChildShapes());
		for (int index = 0; index < compound->getNumChildShapes(); ++index)
			children.push_back(compound->getChildShape(index));
	}

	const auto found = std::find(shapes.begin(), shapes.end(), shape);
	if (found != shapes.end())
		shapes.erase(found);
	delete shape;

	for (btCollisionShape* child : children)
		DeleteCollisionShape(child);
}

// ---------------------------------------------------------
uint ModulePhysics3D::GetDebugMode() const
{
	return debug_draw->getDebugMode();
}

// ---------------------------------------------------------
void ModulePhysics3D::SetDebugMode(uint mode)
{
	debug_draw->setDebugMode(mode);
}

bool ModulePhysics3D::IsDebugEnabled() const
{
	return debug_enabled;
}

void ModulePhysics3D::SetDebugEnabled(bool enabled)
{
	debug_enabled = enabled;
	if (enabled && GetDebugMode() == btIDebugDraw::DBG_NoDebug)
	{
		SetDebugMode(
			btIDebugDraw::DBG_DrawWireframe |
			btIDebugDraw::DBG_DrawContactPoints);
	}
	if (App && App->debug_draw)
	{
		App->debug_draw->SetChannelEnabled(
			DebugDrawChannel::Physics,
			enabled);
	}
}

// ---------------------------------------------------------
update_status ModulePhysics3D::PreUpdate(float dt)
{
	return UPDATE_CONTINUE;
}

void ModulePhysics3D::Step(float fixed_delta_time)
{
	if (paused || !world || fixed_delta_time <= 0.0f)
		return;

	world->stepSimulation(
		fixed_delta_time,
		1,
		fixed_delta_time);
	DispatchCollisions();
}

void ModulePhysics3D::ResetContactState()
{
	contact_tracker.Clear();
}

void ModulePhysics3D::DispatchCollisions()
{
	struct LegacyCollision
	{
		PhysBody3D* first = nullptr;
		PhysBody3D* second = nullptr;
	};

	std::vector<EGE::Physics::ContactObservation>
		componentContacts;
	std::vector<LegacyCollision> legacyCollisions;
	const int manifoldCount =
		world->getDispatcher()->getNumManifolds();
	for (int manifoldIndex = 0;
			manifoldIndex < manifoldCount;
			++manifoldIndex)
	{
		btPersistentManifold* manifold =
			world->getDispatcher()->getManifoldByIndexInternal(
				manifoldIndex);
		if (!manifold)
			continue;

		const btManifoldPoint* deepestContact = nullptr;
		float totalImpulse = 0.0f;
		for (int contactIndex = 0;
			contactIndex < manifold->getNumContacts();
			++contactIndex)
		{
			const btManifoldPoint& contact =
				manifold->getContactPoint(contactIndex);
			if (contact.getDistance() > 0.0f)
				continue;

			totalImpulse += contact.getAppliedImpulse();
			if (!deepestContact ||
				contact.getDistance() <
					deepestContact->getDistance())
			{
				deepestContact = &contact;
			}
		}
		if (!deepestContact)
			continue;

		const auto* objectA =
			static_cast<const btCollisionObject*>(
				manifold->getBody0());
		const auto* objectB =
			static_cast<const btCollisionObject*>(
				manifold->getBody1());
		if (!objectA || !objectB)
			continue;

		ComponentRigidBody* componentA =
			EGE::Physics::GetComponentOwner(*objectA);
		ComponentRigidBody* componentB =
			EGE::Physics::GetComponentOwner(*objectB);
		const bool componentBodyA = componentA != nullptr;
		const bool componentBodyB = componentB != nullptr;
		if (componentBodyA && componentBodyB)
		{
			ComponentCollider* colliderA =
				EGE::Physics::GetColliderOwner(
					*objectA, deepestContact->m_index0);
			ComponentCollider* colliderB =
				EGE::Physics::GetColliderOwner(
					*objectB, deepestContact->m_index1);
			GameObject* ownerA =
				componentA ? componentA->GetGameObject() : nullptr;
			GameObject* ownerB =
				componentB ? componentB->GetGameObject() : nullptr;
			if (ownerA && ownerB && ownerA != ownerB &&
				colliderA && colliderB)
			{
				const bool isTrigger =
					colliderA->IsTrigger() ||
					colliderB->IsTrigger();
				const float3 normalOnB =
					deepestContact->m_normalWorldOnB;

				EGE::Physics::CollisionInfo infoA;
				infoA.selfObjectId = ownerA->GetUID();
				infoA.otherObjectId = ownerB->GetUID();
				infoA.selfColliderId = colliderA->GetUID();
				infoA.otherColliderId = colliderB->GetUID();
				infoA.otherLayer =
					componentB->GetCollisionLayer();
				infoA.point =
					float3(
						deepestContact->getPositionWorldOnA());
				infoA.normal = normalOnB;
				infoA.separation =
					deepestContact->getDistance();
				infoA.impulse = totalImpulse;
				infoA.isTrigger = isTrigger;

				EGE::Physics::CollisionInfo infoB;
				infoB.selfObjectId = ownerB->GetUID();
				infoB.otherObjectId = ownerA->GetUID();
				infoB.selfColliderId = colliderB->GetUID();
				infoB.otherColliderId = colliderA->GetUID();
				infoB.otherLayer =
					componentA->GetCollisionLayer();
				infoB.point =
					float3(
						deepestContact->getPositionWorldOnB());
				infoB.normal = -normalOnB;
				infoB.separation =
					deepestContact->getDistance();
				infoB.impulse = totalImpulse;
				infoB.isTrigger = isTrigger;

				const EGE::Physics::ContactKind kind =
					isTrigger
						? EGE::Physics::ContactKind::Trigger
						: EGE::Physics::ContactKind::Collision;
				EGE::Physics::ContactObservation observation;
				observation.key =
					EGE::Physics::ContactKey::Make(
						colliderA->GetUID(),
						colliderB->GetUID(),
						kind);
				if (componentA->GetUID() <= componentB->GetUID())
				{
					observation.first = infoA;
					observation.second = infoB;
				}
				else
				{
					observation.first = infoB;
					observation.second = infoA;
				}
				componentContacts.push_back(observation);
			}
			continue;
		}
		if (componentBodyA || componentBodyB)
			continue;

		auto* bodyA = static_cast<PhysBody3D*>(
			objectA->getUserPointer());
		auto* bodyB = static_cast<PhysBody3D*>(
			objectB->getUserPointer());
		if (!bodyA || !bodyB)
			continue;

		legacyCollisions.push_back({bodyA, bodyB});
	}

	const std::vector<EGE::Physics::ContactEvent> contactEvents =
		contact_tracker.Update(componentContacts);
	for (const EGE::Physics::ContactEvent& event : contactEvents)
	{
		GameObject* receiver =
			App && App->level
				? App->level->Find(event.info.selfObjectId)
				: nullptr;
		if (!receiver)
			continue;

		receiver->OnPhysicsEvent(event.phase, event.info);
		if (!event.info.isTrigger &&
			event.phase != EGE::Physics::ContactPhase::Exit)
		{
			GameObject* other =
				App->level->Find(event.info.otherObjectId);
			if (other)
				receiver->OnCollision(other);
		}
	}

	for (const LegacyCollision& collision : legacyCollisions)
	{
		for (Module* listener : collision.first->collision_listeners)
			listener->OnCollision(collision.first, collision.second);
		for (Module* listener : collision.second->collision_listeners)
			listener->OnCollision(collision.second, collision.first);
	}
}

// ---------------------------------------------------------
update_status ModulePhysics3D::Update(float dt)
{
	/* Legacy
	// Render vehicles
	for (list<PhysVehicle3D*>::iterator it = vehicles.begin(); it != vehicles.end(); ++it)
		(*it)->Render();
	*/

	return UPDATE_CONTINUE;
}

// ---------------------------------------------------------
update_status ModulePhysics3D::PostUpdate(float dt)
{

	return UPDATE_CONTINUE;
}

// ---------------------------------------------------------
// Called before quitting
bool ModulePhysics3D::CleanUp()
{
	LOG("Destroying 3D Physics simulation");

	// Free all the bodies ---
	for(int i = world->getNumCollisionObjects() - 1; i >= 0; i--)
	{
		btCollisionObject* obj = world->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		btMotionState* state;
		ComponentRigidBody* componentOwner =
			obj ? EGE::Physics::GetComponentOwner(*obj) : nullptr;
		const bool componentOwned = componentOwner != nullptr;
		if (componentOwner && body)
		{
			if (componentOwner->body == body)
				componentOwner->body = nullptr;
			if (componentOwner->trigger_body == body)
				componentOwner->trigger_body = nullptr;
		}
		if (body &&
			!componentOwned &&
			(state = body->getMotionState()) != nullptr)
		{
			RELEASE(state);
		}
		world->removeCollisionObject(obj);
		RELEASE(obj);
	}

	// Free all collision shapes
	for (list<btCollisionShape*>::iterator it = shapes.begin(); it != shapes.end(); ++it)
		RELEASE(*it);

	shapes.clear();
	contact_tracker.Clear();
	
	for (list<PhysBody3D*>::iterator it = bodies.begin(); it != bodies.end(); ++it)
		RELEASE(*it);

	bodies.clear();

	for (list<PhysVehicle3D*>::iterator it = vehicles.begin(); it != vehicles.end(); ++it)
		RELEASE(*it);

	vehicles.clear();

	// Order matters !
	RELEASE(vehicle_raycaster);
	RELEASE(world);

	return true;
}

// ---------------------------------------------------------
void ModulePhysics3D::DrawDebug()
{
	if (debug_enabled && world)
		world->debugDrawWorld();
}

// =============================================
void ModulePhysics3D::Save(Config * config) const
{
	float3 gravity = GetGravity();
	config->AddArrayFloat("Gravity", &gravity.x, 3);
	config->AddBool("Debug Draw", debug_enabled);
	config->AddBool("Paused", paused);
	config->AddInt("Debug Mode", debug_draw->getDebugMode());
}

// =============================================
void ModulePhysics3D::Load(Config * config)
{
	float3 gravity(0.f, -10.f, 0.f);
	gravity.x = config->GetFloat("Gravity", gravity.x, 0);
	gravity.y = config->GetFloat("Gravity", gravity.y, 1);
	gravity.z = config->GetFloat("Gravity", gravity.z, 2);

	SetGravity(gravity);

	debug_draw->setDebugMode(config->GetInt(
		"Debug Mode",
		btIDebugDraw::DBG_DrawWireframe |
			btIDebugDraw::DBG_DrawContactPoints));
	SetDebugEnabled(config->GetBool("Debug Draw", false));
	paused = config->GetBool("Paused", false);
}

// =============================================
void ModulePhysics3D::ReceiveEvent(const Event & event)
{
	switch (event.type)
	{
	case Event::play:
		ResetContactState();
		paused = false;
		break;
	case Event::unpause:
		paused = false;
		break;
	case Event::stop:
		ResetContactState();
		paused = true;
		break;
	case Event::pause:
		paused = true;
		break;
	}
}

// =============================================
void ModulePhysics3D::SetGravity(const float3 & gravity)
{
	world->setGravity(gravity);
}

// =============================================
float3 ModulePhysics3D::GetGravity() const
{
	return world->getGravity();
}

void ModulePhysics3D::SetCollisionMatrix(
	const EGE::Physics::CollisionMatrix& matrix)
{
	collision_matrix = matrix;
	if (!world)
		return;

	std::vector<ComponentRigidBody*> components;
	for (int index = 0;
		index < world->getNumCollisionObjects();
		++index)
	{
		btCollisionObject* object =
			world->getCollisionObjectArray()[index];
		ComponentRigidBody* component =
			object
				? EGE::Physics::GetComponentOwner(*object)
				: nullptr;
		if (component &&
			std::find(
				components.begin(),
				components.end(),
				component) == components.end())
		{
			components.push_back(component);
		}
	}

	for (ComponentRigidBody* component : components)
		component->RebuildBody();
}

const EGE::Physics::CollisionMatrix&
ModulePhysics3D::GetCollisionMatrix() const
{
	return collision_matrix;
}

bool ModulePhysics3D::Raycast(
	const float3& origin,
	const float3& direction,
	float maxDistance,
	const EGE::Physics::QueryFilter& filter,
	EGE::Physics::QueryHit& hit) const
{
	return world &&
		EGE::Physics::Raycast(
			*world,
			origin,
			direction,
			maxDistance,
			filter,
			hit);
}

std::vector<EGE::Physics::QueryHit>
ModulePhysics3D::RaycastAll(
	const float3& origin,
	const float3& direction,
	float maxDistance,
	const EGE::Physics::QueryFilter& filter) const
{
	return world
		? EGE::Physics::RaycastAll(
			*world,
			origin,
			direction,
			maxDistance,
			filter)
		: std::vector<EGE::Physics::QueryHit>{};
}

bool ModulePhysics3D::SphereCast(
	const float3& origin,
	float radius,
	const float3& direction,
	float maxDistance,
	const EGE::Physics::QueryFilter& filter,
	EGE::Physics::QueryHit& hit) const
{
	return world &&
		EGE::Physics::SphereCast(
			*world,
			origin,
			radius,
			direction,
			maxDistance,
			filter,
			hit);
}

std::vector<EGE::Physics::QueryHit>
ModulePhysics3D::OverlapSphere(
	const float3& center,
	float radius,
	const EGE::Physics::QueryFilter& filter)
{
	return world
		? EGE::Physics::OverlapSphere(
			*world,
			center,
			radius,
			filter)
		: std::vector<EGE::Physics::QueryHit>{};
}

short ModulePhysics3D::GetCollisionMaskBits(
	const ComponentRigidBody& component) const
{
	return static_cast<short>(
		component.GetCollisionMask() &
		collision_matrix.GetMask(component.GetCollisionLayer()));
}

// =============================================
void DebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	if (!App || !App->debug_draw)
		return;
	App->debug_draw->DrawLine(
		float3(from),
		float3(to),
		float3(color.getX(), color.getY(), color.getZ()),
		0.0f,
		true,
		DebugDrawChannel::Physics);
}

void DebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
{
	if (!App || !App->debug_draw)
		return;

	const float3 point(PointOnB);
	const float3 normal(normalOnB);
	const float3 drawColor(
		color.getX(), color.getY(), color.getZ());
	const float duration =
		std::max(lifeTime, 0) * 0.001f;
	App->debug_draw->DrawPoint(
		point,
		drawColor,
		5.0f,
		duration,
		false,
		DebugDrawChannel::Physics);
	App->debug_draw->DrawLine(
		point,
		point + normal * static_cast<float>(distance),
		drawColor,
		duration,
		false,
		DebugDrawChannel::Physics);
}

void DebugDrawer::reportErrorWarning(const char* warningString)
{
	LOG("Bullet warning: %s", warningString);
}

void DebugDrawer::draw3dText(const btVector3& location, const char* textString)
{
	LOG("Bullet draw text: %s", textString);
}

void DebugDrawer::setDebugMode(int debugMode)
{
	mode = (DebugDrawModes) debugMode;
}

int	 DebugDrawer::getDebugMode() const
{
	return mode;
}
