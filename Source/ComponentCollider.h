#pragma once

#include "Component.h"
#include "Math.h"
#include "PhysicsColliderData.h"

class ComponentRigidBody;

class ComponentCollider final : public Component
{
public:
	using ShapeType = EGE::Physics::ColliderShape;

	explicit ComponentCollider(GameObject* container);

	void GetBoundingBox(AABB& bounds) const override;
	void OnSave(Config& config) const override;
	void OnLoad(Config* config) override;
	void OnActivate() override;
	void OnDeActivate() override;
	void OnPlay() override;
	void OnDebugDraw(bool selected) const override;

	[[nodiscard]] ShapeType GetShapeType() const;
	void SetShapeType(ShapeType value);

	[[nodiscard]] bool IsTrigger() const;
	void SetTrigger(bool value);

	[[nodiscard]] const float3& GetSphereCenter() const;
	void SetSphereCenter(const float3& value);
	[[nodiscard]] float GetSphereRadius() const;
	void SetSphereRadius(float value);

	[[nodiscard]] const float3& GetBoxCenter() const;
	void SetBoxCenter(const float3& value);
	[[nodiscard]] const float3& GetBoxHalfExtents() const;
	void SetBoxHalfExtents(const float3& value);
	[[nodiscard]] const float3& GetBoxRotation() const;
	void SetBoxRotation(const float3& eulerRadians);

	[[nodiscard]] const float3& GetCapsuleStart() const;
	void SetCapsuleStart(const float3& value);
	[[nodiscard]] const float3& GetCapsuleEnd() const;
	void SetCapsuleEnd(const float3& value);
	[[nodiscard]] float GetCapsuleRadius() const;
	void SetCapsuleRadius(float value);

	[[nodiscard]] const Sphere& GetSphere() const;
	[[nodiscard]] const OBB& GetBox() const;
	[[nodiscard]] const Capsule& GetCapsule() const;

	void LoadLegacyRigidBody(const Config& config);
	void DrawEditor();

private:
	[[nodiscard]] ComponentRigidBody* FindRigidBody() const;
	void NotifyShapeChanged();

	EGE::Physics::ColliderData data_;
};
