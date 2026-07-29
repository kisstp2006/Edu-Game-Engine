#ifndef __COMPONENT_RIGID_BODY_H__
#define __COMPONENT_RIGID_BODY_H__

// Component to allow another mesh to deform based on a skeleton 

#include "Component.h"
#include "Math.h"
#include "PhysicsCollisionSettings.h"
#include "PhysicsRigidBodyDefaults.h"
#include <LinearMath/btMotionState.h>
#include <LinearMath/btTransform.h>

class btRigidBody;
class ComponentCollider;

class ComponentRigidBody : public Component, public btMotionState
{
	friend class ModulePhysics3D;
public:
	enum BodyType
	{
		body_sphere,
		body_box,
		body_capsule,
		body_unknown
	};

	enum BodyBehaviour
	{
		fixed,
		dynamic,
		kinematic
	};

public:
	ComponentRigidBody (GameObject* container);
	~ComponentRigidBody () override;

	void GetBoundingBox(AABB& box) const override;

	void OnSave(Config& config) const override;
	void OnLoad(Config* config) override;

	void OnActivate() override;
	void OnDeActivate() override;
	void OnPlay() override;
	void OnFixedUpdate(float deltaTime) override;
	void OnStop() override;
	void OnDebugDraw(bool selected) const override;

	// from btMotionState
	void getWorldTransform(btTransform& worldTrans ) const override;
	void setWorldTransform(const btTransform& worldTrans) override;

	void SetBodyType(BodyType new_type);
	BodyType GetBodyType() const;

	void SetBehaviour(BodyBehaviour new_behaviour);
	BodyBehaviour GetBehaviour() const;

	float GetMass() const;
	void SetMass(float value);
	float GetRestitution() const;
	void SetRestitution(float value);
	float GetFriction() const;
	void SetFriction(float value);
	float GetRollingFriction() const;
	void SetRollingFriction(float value);
	float GetLinearDamping() const;
	void SetLinearDamping(float value);
	float GetAngularDamping() const;
	void SetAngularDamping(float value);

	const float3& GetLinearFactor() const;
	void SetLinearFactor(const float3& value);
	const float3& GetAngularFactor() const;
	void SetAngularFactor(const float3& value);

	const float3& GetSphereCenter() const;
	void SetSphereCenter(const float3& value);
	float GetSphereRadius() const;
	void SetSphereRadius(float value);
	const float3& GetBoxCenter() const;
	void SetBoxCenter(const float3& value);
	const float3& GetBoxHalfExtents() const;
	void SetBoxHalfExtents(const float3& value);
	const float3& GetBoxRotation() const;
	void SetBoxRotation(const float3& eulerRadians);
	const float3& GetCapsuleStart() const;
	void SetCapsuleStart(const float3& value);
	const float3& GetCapsuleEnd() const;
	void SetCapsuleEnd(const float3& value);
	float GetCapsuleRadius() const;
	void SetCapsuleRadius(float value);
	bool IsTrigger() const;
	void SetTrigger(bool value);
	std::uint32_t GetCollisionLayer() const;
	void SetCollisionLayer(std::uint32_t value);
	std::uint32_t GetCollisionMask() const;
	void SetCollisionMask(std::uint32_t value);

	bool HasRuntimeBody() const;
	float3 GetLinearVelocity() const;
	void SetLinearVelocity(const float3& value);
	float3 GetAngularVelocity() const;
	void SetAngularVelocity(const float3& value);
	float3 GetCenterOfMass() const;
	float3 GetTotalForce() const;
	float3 GetTotalTorque() const;
	bool GetUseWorldGravity() const;
	void SetUseWorldGravity(bool value);
	float3 GetGravity() const;
	void SetGravity(const float3& value);
	bool IsAwake() const;
	void WakeUp();
	void Sleep();
	void ClearForces();
	void ApplyCentralForce(const float3& force);
	void ApplyForce(
		const float3& force,
		const float3& relativePosition);
	void ApplyCentralImpulse(const float3& impulse);
	void ApplyImpulse(
		const float3& impulse,
		const float3& relativePosition);
	void ApplyTorque(const float3& torque);
	void ApplyTorqueImpulse(const float3& torque);
	void RebuildBody();

	void DrawEditor();

private:
	[[nodiscard]] ComponentCollider* GetPrimaryCollider() const;
	[[nodiscard]] ComponentCollider* EnsurePrimaryCollider();
	void CreateBody();
	void ApplyBodyConfiguration();
	void SynchronizeTriggerBody();
	[[nodiscard]] float3 GetGlobalScale() const;
	[[nodiscard]] short GetCollisionGroupBits() const;

private:

	BodyBehaviour behaviour = BodyBehaviour::dynamic;

	EGE::Physics::CollisionSettings collision_settings;

	btRigidBody* body = nullptr;
	btRigidBody* trigger_body = nullptr;
	float mass = EGE::Physics::RigidBodyDefaults::Mass;
	float restitution =
		EGE::Physics::RigidBodyDefaults::Restitution;
	float friction = EGE::Physics::RigidBodyDefaults::Friction;
	float rolling_friction =
		EGE::Physics::RigidBodyDefaults::RollingFriction;
	float linear_damping =
		EGE::Physics::RigidBodyDefaults::LinearDamping;
	float angular_damping =
		EGE::Physics::RigidBodyDefaults::AngularDamping;
	float3 gravity = float3(0.0f, -10.0f, 0.0f);
	bool use_world_gravity = true;
	float3 linear_factor = float3::one;
	float3 angular_factor = float3::one;
	float3 collision_scale = float3::one;
};

#endif // __COMPONENT_RIGID_BODY_H__
