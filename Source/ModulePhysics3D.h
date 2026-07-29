#ifndef __MODULE_PHYSICS_3D_H__
#define __MODULE_PHYSICS_3D_H__

#include "Module.h"
#include "Globals.h"
#include <list>
#include "Primitive.h"
#include "PhysicsContactTracker.h"
#include "PhysicsCollisionMatrix.h"
#include "PhysicsQuery.h"
#include <LinearMath/btIDebugDraw.h>
#include "Math.h"

// Recommended scale is 1.0f == 1 meter, no less than 0.2 objects
#define GRAVITY btVector3(0.0f, -10.0f, 0.0f) 

class btRigidBody;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btDefaultVehicleRaycaster;
class btCollisionShape;
class DebugDrawer;

class DebugDrawer;
class ComponentRigidBody;
class ComponentCollider;

struct PhysBody3D;
struct PhysVehicle3D;
struct VehicleInfo;

class ModulePhysics3D : public Module
{
public:
	ModulePhysics3D(bool start_enabled = true);
	~ModulePhysics3D();

	bool Init(Config* config = nullptr) override;
	bool Start(Config* config = nullptr) override;
	update_status PreUpdate(float dt) override;
	update_status Update(float dt) override;
	update_status PostUpdate(float dt) override;
	bool CleanUp();
	void DrawDebug() override;

	void Save(Config* config) const override;
	void Load(Config* config) override;

	void ReceiveEvent(const Event& event) override;

	// Utils ----

	void SetGravity(const float3& gravity);
	float3 GetGravity() const;
	void SetCollisionMatrix(
		const EGE::Physics::CollisionMatrix& matrix);
	[[nodiscard]] const EGE::Physics::CollisionMatrix&
		GetCollisionMatrix() const;
	[[nodiscard]] bool Raycast(
		const float3& origin,
		const float3& direction,
		float maxDistance,
		const EGE::Physics::QueryFilter& filter,
		EGE::Physics::QueryHit& hit) const;
	[[nodiscard]] std::vector<EGE::Physics::QueryHit> RaycastAll(
		const float3& origin,
		const float3& direction,
		float maxDistance,
		const EGE::Physics::QueryFilter& filter) const;
	[[nodiscard]] bool SphereCast(
		const float3& origin,
		float radius,
		const float3& direction,
		float maxDistance,
		const EGE::Physics::QueryFilter& filter,
		EGE::Physics::QueryHit& hit) const;
	[[nodiscard]] std::vector<EGE::Physics::QueryHit> OverlapSphere(
		const float3& center,
		float radius,
		const EGE::Physics::QueryFilter& filter);

	// Bodies ---

	btRigidBody* AddBody(
		ComponentRigidBody* component,
		btRigidBody** triggerBody);
	PhysBody3D*		AddBody(const PCylinder& cylinder, float mass = 1.0f);
	PhysBody3D*		AddBody(const PPlane& plane);
	PhysBody3D*		AddHeighField(const char* filename, int width, int height);
	PhysVehicle3D*	AddVehicle(const VehicleInfo& info);

	void DeleteBody(PhysBody3D* body);
	void DeleteBody(btRigidBody* body);
	void Step(float fixed_delta_time);
	void ResetContactState();

	uint GetDebugMode() const;
	void SetDebugMode(uint mode);
	bool IsDebugEnabled() const;
	void SetDebugEnabled(bool enabled);

public:
	bool paused = true;

private:
	void DeleteCollisionShape(btCollisionShape* shape);
	void DispatchCollisions();
	[[nodiscard]] short GetCollisionMaskBits(
		const ComponentRigidBody& component) const;

	btDefaultCollisionConfiguration*	collision_conf = nullptr;
	btCollisionDispatcher*				dispatcher = nullptr;
	btBroadphaseInterface*				broad_phase = nullptr;
	btSequentialImpulseConstraintSolver* solver = nullptr;
	btDiscreteDynamicsWorld*			world = nullptr;
	btDefaultVehicleRaycaster*			vehicle_raycaster = nullptr;
	DebugDrawer*						debug_draw = nullptr;
	EGE::Physics::ContactTracker		contact_tracker;
	EGE::Physics::CollisionMatrix collision_matrix;
	bool debug_enabled = false;

	std::list<btCollisionShape*> shapes;
	std::list<PhysBody3D*> bodies;
	std::list<PhysVehicle3D*> vehicles;
};

class DebugDrawer : public btIDebugDraw
{
public:
	DebugDrawer()
		: mode(static_cast<DebugDrawModes>(
			DBG_DrawWireframe | DBG_DrawContactPoints))
	{}

	void drawLine(const btVector3& from, const btVector3& to, const btVector3& color);
	void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color);
	void reportErrorWarning(const char* warningString);
	void draw3dText(const btVector3& location, const char* textString);
	void setDebugMode(int debugMode);
	int	 getDebugMode() const;

	DebugDrawModes mode;
};

#endif __MODULE_PHYSICS_3D_H__
