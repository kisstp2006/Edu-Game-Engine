#include "Globals.h"
#include "Application.h"
#include "ModulePhysics3D.h"
#include "Primitive.h"
#include "PhysBody3D.h"
#include "PhysVehicle3D.h"
#include "ComponentRigidBody.h"
#include "GameObject.h"
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
	float3 MultiplyComponents(const float3& left, const float3& right)
	{
		return {
			left.x * right.x,
			left.y * right.y,
			left.z * right.z};
	}

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

	btTransform MakeChildTransform(
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

// ---------------------------------------------------------
btRigidBody* ModulePhysics3D::AddBody(const OBB& cube, ComponentRigidBody* component)
{
	float mass = (component->behaviour == ComponentRigidBody::BodyBehaviour::dynamic) ? component->mass : 0.0f;
	const float3 scale = GetGlobalScale(*component);
	const float3 absoluteScale(
		std::abs(scale.x),
		std::abs(scale.y),
		std::abs(scale.z));
	const float3 halfExtents =
		MultiplyComponents(cube.r, absoluteScale);
	const float3 center = MultiplyComponents(cube.pos, scale);

	auto* childShape = new btBoxShape(halfExtents);
	auto* colShape = new btCompoundShape();
	const Quat localRotation =
		float3x3(cube.axis[0], cube.axis[1], cube.axis[2]).ToQuat();
	colShape->addChildShape(
		MakeChildTransform(center, localRotation),
		childShape);

	shapes.push_back(childShape);
	shapes.push_back(colShape);

	btVector3 localInertia(0.f, 0.f, 0.f);
	if(mass != 0.f)
		colShape->calculateLocalInertia(mass, localInertia);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, component, colShape, localInertia);

	btRigidBody* body = new btRigidBody(rbInfo);
	body->setUserPointer(component);
	body->setUserIndex(ComponentRigidBody::PhysicsUserIndex);
	world->addRigidBody(body);

	return body;
}

// ---------------------------------------------------------
btRigidBody* ModulePhysics3D::AddBody(const Sphere& sphere, ComponentRigidBody* component)
{
	float mass = (component->behaviour == ComponentRigidBody::BodyBehaviour::dynamic) ? component->mass : 0.0f;
	const float3 scale = GetGlobalScale(*component);
	const float radiusScale = std::max({
		std::abs(scale.x),
		std::abs(scale.y),
		std::abs(scale.z)});
	const float3 center = MultiplyComponents(sphere.pos, scale);

	auto* childShape = new btSphereShape(
		std::max(sphere.r * radiusScale, 0.001f));
	auto* colShape = new btCompoundShape();
	colShape->addChildShape(MakeChildTransform(center), childShape);
	shapes.push_back(childShape);
	shapes.push_back(colShape);

	btVector3 localInertia(0, 0, 0);
	if(mass != 0.f)
		colShape->calculateLocalInertia(mass, localInertia);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, component, colShape, localInertia);

	btRigidBody* body = new btRigidBody(rbInfo);
	body->setUserPointer(component);
	body->setUserIndex(ComponentRigidBody::PhysicsUserIndex);
	world->addRigidBody(body);

	return body;
}

// ---------------------------------------------------------
btRigidBody * ModulePhysics3D::AddBody(const Capsule & capsule, ComponentRigidBody * component)
{
	float mass = (component->behaviour == ComponentRigidBody::BodyBehaviour::dynamic) ? component->mass : 0.0f;
	const float3 scale = GetGlobalScale(*component);
	const float3 start = MultiplyComponents(capsule.l.a, scale);
	const float3 end = MultiplyComponents(capsule.l.b, scale);
	const float3 center = (start + end) * 0.5f;
	const float3 axis = end - start;
	const float lineLength = axis.Length();
	const float radiusScale = std::max({
		std::abs(scale.x),
		std::abs(scale.y),
		std::abs(scale.z)});
	const float radius = std::max(capsule.r * radiusScale, 0.001f);

	auto* childShape = new btCapsuleShape(radius, lineLength);
	auto* colShape = new btCompoundShape();
	const Quat localRotation = lineLength > 0.0001f
		? Quat::RotateFromTo(float3::unitY, axis / lineLength)
		: Quat::identity;
	colShape->addChildShape(
		MakeChildTransform(center, localRotation),
		childShape);
	shapes.push_back(childShape);
	shapes.push_back(colShape);

	btVector3 localInertia(0, 0, 0);
	if(mass != 0.f)
		colShape->calculateLocalInertia(mass, localInertia);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, component, colShape, localInertia);

	btRigidBody* body = new btRigidBody(rbInfo);
	body->setUserPointer(component);
	body->setUserIndex(ComponentRigidBody::PhysicsUserIndex);
	world->addRigidBody(body);

	return body;
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

void ModulePhysics3D::DispatchCollisions()
{
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

		bool touching = false;
		for (int contactIndex = 0;
			contactIndex < manifold->getNumContacts();
			++contactIndex)
		{
			if (manifold->getContactPoint(contactIndex).getDistance() <= 0.0f)
			{
				touching = true;
				break;
			}
		}
		if (!touching)
			continue;

		auto* objectA = const_cast<btCollisionObject*>(
			static_cast<const btCollisionObject*>(
				manifold->getBody0()));
		auto* objectB = const_cast<btCollisionObject*>(
			static_cast<const btCollisionObject*>(
				manifold->getBody1()));
		if (!objectA || !objectB)
			continue;

		const bool componentBodyA =
			objectA->getUserIndex() ==
				ComponentRigidBody::PhysicsUserIndex;
		const bool componentBodyB =
			objectB->getUserIndex() ==
				ComponentRigidBody::PhysicsUserIndex;
		if (componentBodyA && componentBodyB)
		{
			auto* componentA = static_cast<ComponentRigidBody*>(
				objectA->getUserPointer());
			auto* componentB = static_cast<ComponentRigidBody*>(
				objectB->getUserPointer());
			GameObject* ownerA =
				componentA ? componentA->GetGameObject() : nullptr;
			GameObject* ownerB =
				componentB ? componentB->GetGameObject() : nullptr;
			if (ownerA && ownerB && ownerA != ownerB)
			{
				ownerA->OnCollision(ownerB);
				ownerB->OnCollision(ownerA);
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

		for (Module* listener : bodyA->collision_listeners)
			listener->OnCollision(bodyA, bodyB);
		for (Module* listener : bodyB->collision_listeners)
			listener->OnCollision(bodyB, bodyA);
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
		if(body && (state = body->getMotionState()) != nullptr)
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
	if(debug == true)
		world->debugDrawWorld();
}

// =============================================
void ModulePhysics3D::Save(Config * config) const
{
	float3 gravity = GetGravity();
	config->AddArrayFloat("Gravity", &gravity.x, 3);
	config->AddBool("Debug Draw", debug);
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

	debug = config->GetBool("Debug Draw", false);
	paused = config->GetBool("Paused", false);

	debug_draw->setDebugMode(config->GetInt("Debug Mode", 0));
}

// =============================================
void ModulePhysics3D::ReceiveEvent(const Event & event)
{
	switch (event.type)
	{
	case Event::play:
	case Event::unpause:
		paused = false;
		break;
	case Event::stop:
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

// =============================================
void DebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	dd::line(from, to, float3(color.getX(), color.getY(), color.getZ()));
}

void DebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
{
	dd::point(PointOnB, float3(color.getX(), color.getY(), color.getZ()));
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
