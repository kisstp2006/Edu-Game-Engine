#include "Globals.h"
#include "PhysicsColliderData.h"

#include "Config.h"

#include <algorithm>

namespace
{
	constexpr int ColliderDataVersion = 1;
}

namespace EGE::Physics
{
	ColliderData::ColliderData()
	{
		sphere.pos = float3::zero;
		sphere.r = 1.0f;
		box.pos = float3::zero;
		box.r = float3::one;
		capsule.l = LineSegment(float3::zero, float3::unitY);
		capsule.r = 1.0f;
		UpdateBoxAxes();
	}

	void ColliderData::Save(Config& config) const
	{
		config.AddInt("Collider Data Version", ColliderDataVersion);
		config.AddInt("Shape", static_cast<int>(shape));
		config.AddBool("Is Trigger", isTrigger);
		config.AddArrayFloat("Sphere", &sphere.pos.x, 4);
		config.AddArrayFloat("Box", &box.pos.x, 6);
		config.AddFloat3("Box Rotation", boxRotation);
		config.AddArrayFloat("Capsule", &capsule.l.a.x, 7);
	}

	void ColliderData::Load(Config& config)
	{
		shape = static_cast<ColliderShape>(std::clamp(
			config.GetInt("Shape", static_cast<int>(ColliderShape::Box)),
			static_cast<int>(ColliderShape::Sphere),
			static_cast<int>(ColliderShape::Capsule)));
		isTrigger = config.GetBool("Is Trigger", false);

		sphere.pos.x = config.GetFloat("Sphere", 0.0f, 0);
		sphere.pos.y = config.GetFloat("Sphere", 0.0f, 1);
		sphere.pos.z = config.GetFloat("Sphere", 0.0f, 2);
		sphere.r = config.GetFloat("Sphere", 1.0f, 3);

		box.pos.x = config.GetFloat("Box", 0.0f, 0);
		box.pos.y = config.GetFloat("Box", 0.0f, 1);
		box.pos.z = config.GetFloat("Box", 0.0f, 2);
		box.r.x = config.GetFloat("Box", 1.0f, 3);
		box.r.y = config.GetFloat("Box", 1.0f, 4);
		box.r.z = config.GetFloat("Box", 1.0f, 5);
		boxRotation =
			config.GetFloat3("Box Rotation", float3::zero);

		capsule.l.a.x = config.GetFloat("Capsule", 0.0f, 0);
		capsule.l.a.y = config.GetFloat("Capsule", 0.0f, 1);
		capsule.l.a.z = config.GetFloat("Capsule", 0.0f, 2);
		capsule.l.b.x = config.GetFloat("Capsule", 0.0f, 3);
		capsule.l.b.y = config.GetFloat("Capsule", 1.0f, 4);
		capsule.l.b.z = config.GetFloat("Capsule", 0.0f, 5);
		capsule.r = config.GetFloat("Capsule", 1.0f, 6);
		Normalize();
	}

	void ColliderData::LoadLegacyRigidBody(Config& config)
	{
		shape = static_cast<ColliderShape>(std::clamp(
			config.GetInt("Body Type", 0), 0, 2));
		isTrigger = config.GetBool("Is Trigger", false);

		sphere.pos.x = config.GetFloat("Sphere", 0.0f, 0);
		sphere.pos.y = config.GetFloat("Sphere", 0.0f, 1);
		sphere.pos.z = config.GetFloat("Sphere", 0.0f, 2);
		sphere.r = config.GetFloat("Sphere", 1.0f, 3);

		box.pos.x = config.GetFloat("Box", 0.0f, 0);
		box.pos.y = config.GetFloat("Box", 0.0f, 1);
		box.pos.z = config.GetFloat("Box", 0.0f, 2);
		box.r.x = config.GetFloat("Box", 1.0f, 3);
		box.r.y = config.GetFloat("Box", 1.0f, 4);
		box.r.z = config.GetFloat("Box", 1.0f, 5);
		boxRotation =
			config.GetFloat3("Box Rotation", float3::zero);

		capsule.l.a.x = config.GetFloat("Capsule", 0.0f, 0);
		capsule.l.a.y = config.GetFloat("Capsule", 0.0f, 1);
		capsule.l.a.z = config.GetFloat("Capsule", 0.0f, 2);
		capsule.l.b.x = config.GetFloat("Capsule", 0.0f, 3);
		capsule.l.b.y = config.GetFloat("Capsule", 1.0f, 4);
		capsule.l.b.z = config.GetFloat("Capsule", 0.0f, 5);
		capsule.r = config.GetFloat("Capsule", 1.0f, 6);
		Normalize();
	}

	void ColliderData::Normalize()
	{
		sphere.r = std::max(sphere.r, 0.001f);
		box.r.x = std::max(box.r.x, 0.001f);
		box.r.y = std::max(box.r.y, 0.001f);
		box.r.z = std::max(box.r.z, 0.001f);
		capsule.r = std::max(capsule.r, 0.001f);
		UpdateBoxAxes();
	}

	void ColliderData::UpdateBoxAxes()
	{
		const Quat rotation =
			Quat::FromEulerXYZ(
				boxRotation.x,
				boxRotation.y,
				boxRotation.z).Normalized();
		box.axis[0] =
			rotation.Transform(float3::unitX).Normalized();
		box.axis[1] =
			rotation.Transform(float3::unitY).Normalized();
		box.axis[2] =
			rotation.Transform(float3::unitZ).Normalized();
	}
}
