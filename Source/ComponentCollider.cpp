#include "Globals.h"
#include "ComponentCollider.h"

#include "Application.h"
#include "ComponentRigidBody.h"
#include "GameObject.h"
#include "ModuleDebugDraw.h"
#include "ModulePhysics3D.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

ComponentCollider::ComponentCollider(GameObject* container)
	: Component(container, Types::Collider)
{
}

void ComponentCollider::GetBoundingBox(AABB& bounds) const
{
	switch (data_.shape)
	{
	case ShapeType::Sphere:
		bounds.Enclose(data_.sphere);
		break;
	case ShapeType::Box:
		bounds.Enclose(data_.box);
		break;
	case ShapeType::Capsule:
		bounds.Enclose(data_.capsule);
		break;
	}
}

void ComponentCollider::OnSave(Config& config) const
{
	data_.Save(config);
}

void ComponentCollider::OnLoad(Config* config)
{
	if (config)
		data_.Load(*config);
}

void ComponentCollider::LoadLegacyRigidBody(const Config& config)
{
	Config legacy = config;
	data_.LoadLegacyRigidBody(legacy);
}

void ComponentCollider::OnActivate()
{
	NotifyShapeChanged();
}

void ComponentCollider::OnDeActivate()
{
	NotifyShapeChanged();
}

void ComponentCollider::OnPlay()
{
	NotifyShapeChanged();
}

void ComponentCollider::OnDebugDraw(bool selected) const
{
	const bool physicsDebug =
		App && App->physics3D &&
		App->physics3D->IsDebugEnabled();
	if ((!selected && !physicsDebug) ||
		!game_object ||
		!App ||
		!App->debug_draw)
		return;

	ModuleDebugDraw& debugDraw = *App->debug_draw;
	const DebugDrawChannel channel = physicsDebug
		? DebugDrawChannel::Physics
		: DebugDrawChannel::Engine;
	const float4x4& transform = game_object->GetGlobalTransformation();
	float3 position;
	float3 scale;
	Quat rotation;
	transform.Decompose(position, rotation, scale);
	rotation.Normalize();
	const float3 absoluteScale(
		std::abs(scale.x), std::abs(scale.y), std::abs(scale.z));
	const float radiusScale = std::max({
		absoluteScale.x, absoluteScale.y, absoluteScale.z});
	const float3 color =
		data_.isTrigger
			? float3(1.0f, 0.85f, 0.1f)
			: float3(0.15f, 1.0f, 0.35f);

	switch (data_.shape)
	{
	case ShapeType::Sphere:
		debugDraw.DrawSphere(
			transform.TransformPos(data_.sphere.pos),
			color,
			data_.sphere.r * radiusScale,
			0.0f,
			true,
			channel);
		break;
	case ShapeType::Box:
	{
		OBB worldBox;
		worldBox.pos = transform.TransformPos(data_.box.pos);
		worldBox.r = float3(
			data_.box.r.x * absoluteScale.x,
			data_.box.r.y * absoluteScale.y,
			data_.box.r.z * absoluteScale.z);
		for (int axis = 0; axis < 3; ++axis)
			worldBox.axis[axis] =
				rotation.Transform(data_.box.axis[axis]).Normalized();
		std::array<float3, 8> corners;
		worldBox.GetCornerPoints(corners.data());
		debugDraw.DrawBox(
			corners,
			color,
			0.0f,
			true,
			channel);
		break;
	}
	case ShapeType::Capsule:
	{
		const float3 start =
			transform.TransformPos(data_.capsule.l.a);
		const float3 end =
			transform.TransformPos(data_.capsule.l.b);
		const float3 axis = end - start;
		debugDraw.DrawCapsule(
			(start + end) * 0.5f,
			axis.LengthSq() > 0.000001f
				? axis.Normalized()
				: float3::unitY,
			color,
			data_.capsule.r * radiusScale,
			axis.Length(),
			0.0f,
			true,
			channel);
		break;
	}
	}
}

ComponentCollider::ShapeType ComponentCollider::GetShapeType() const
{
	return data_.shape;
}

void ComponentCollider::SetShapeType(ShapeType value)
{
	if (data_.shape == value)
		return;
	data_.shape = value;
	NotifyShapeChanged();
}

bool ComponentCollider::IsTrigger() const
{
	return data_.isTrigger;
}

void ComponentCollider::SetTrigger(bool value)
{
	if (data_.isTrigger == value)
		return;
	data_.isTrigger = value;
	NotifyShapeChanged();
}

const float3& ComponentCollider::GetSphereCenter() const
{
	return data_.sphere.pos;
}

void ComponentCollider::SetSphereCenter(const float3& value)
{
	data_.sphere.pos = value;
	NotifyShapeChanged();
}

float ComponentCollider::GetSphereRadius() const
{
	return data_.sphere.r;
}

void ComponentCollider::SetSphereRadius(float value)
{
	data_.sphere.r = std::max(value, 0.001f);
	NotifyShapeChanged();
}

const float3& ComponentCollider::GetBoxCenter() const
{
	return data_.box.pos;
}

void ComponentCollider::SetBoxCenter(const float3& value)
{
	data_.box.pos = value;
	NotifyShapeChanged();
}

const float3& ComponentCollider::GetBoxHalfExtents() const
{
	return data_.box.r;
}

void ComponentCollider::SetBoxHalfExtents(const float3& value)
{
	data_.box.r = float3(
		std::max(value.x, 0.001f),
		std::max(value.y, 0.001f),
		std::max(value.z, 0.001f));
	NotifyShapeChanged();
}

const float3& ComponentCollider::GetBoxRotation() const
{
	return data_.boxRotation;
}

void ComponentCollider::SetBoxRotation(const float3& eulerRadians)
{
	data_.boxRotation = eulerRadians;
	data_.UpdateBoxAxes();
	NotifyShapeChanged();
}

const float3& ComponentCollider::GetCapsuleStart() const
{
	return data_.capsule.l.a;
}

void ComponentCollider::SetCapsuleStart(const float3& value)
{
	data_.capsule.l.a = value;
	NotifyShapeChanged();
}

const float3& ComponentCollider::GetCapsuleEnd() const
{
	return data_.capsule.l.b;
}

void ComponentCollider::SetCapsuleEnd(const float3& value)
{
	data_.capsule.l.b = value;
	NotifyShapeChanged();
}

float ComponentCollider::GetCapsuleRadius() const
{
	return data_.capsule.r;
}

void ComponentCollider::SetCapsuleRadius(float value)
{
	data_.capsule.r = std::max(value, 0.001f);
	NotifyShapeChanged();
}

const Sphere& ComponentCollider::GetSphere() const
{
	return data_.sphere;
}

const OBB& ComponentCollider::GetBox() const
{
	return data_.box;
}

const Capsule& ComponentCollider::GetCapsule() const
{
	return data_.capsule;
}

void ComponentCollider::DrawEditor()
{
	static const char* shapes[] = {"Sphere", "Box", "Capsule"};
	int shape = static_cast<int>(data_.shape);
	if (ImGui::Combo("Shape", &shape, shapes, IM_ARRAYSIZE(shapes)))
		SetShapeType(static_cast<ShapeType>(shape));

	bool trigger = data_.isTrigger;
	if (ImGui::Checkbox("Is Trigger", &trigger))
		SetTrigger(trigger);

	float3 vector;
	switch (data_.shape)
	{
	case ShapeType::Sphere:
		vector = data_.sphere.pos;
		if (ImGui::DragFloat3("Center", &vector.x, 0.05f))
			SetSphereCenter(vector);
	{
		float radius = data_.sphere.r;
		if (ImGui::DragFloat("Radius", &radius, 0.05f, 0.001f))
			SetSphereRadius(radius);
		break;
	}
	case ShapeType::Box:
		vector = data_.box.pos;
		if (ImGui::DragFloat3("Center", &vector.x, 0.05f))
			SetBoxCenter(vector);
		vector = data_.box.r;
		if (ImGui::DragFloat3(
				"Half Extents", &vector.x, 0.05f, 0.001f))
		{
			SetBoxHalfExtents(vector);
		}
		vector = data_.boxRotation * RADTODEG;
		if (ImGui::DragFloat3("Rotation", &vector.x, 0.1f))
			SetBoxRotation(vector * DEGTORAD);
		break;
	case ShapeType::Capsule:
		vector = data_.capsule.l.a;
		if (ImGui::DragFloat3("Start", &vector.x, 0.05f))
			SetCapsuleStart(vector);
		vector = data_.capsule.l.b;
		if (ImGui::DragFloat3("End", &vector.x, 0.05f))
			SetCapsuleEnd(vector);
	{
		float radius = data_.capsule.r;
		if (ImGui::DragFloat("Radius", &radius, 0.05f, 0.001f))
			SetCapsuleRadius(radius);
		break;
	}
	}

	if (!FindRigidBody())
		ImGui::TextDisabled(
			"Add a RigidBody to include this collider in physics.");
}

ComponentRigidBody* ComponentCollider::FindRigidBody() const
{
	if (!game_object)
		return nullptr;
	return static_cast<ComponentRigidBody*>(
		game_object->FindFirstComponent(Component::RigidBody));
}

void ComponentCollider::NotifyShapeChanged()
{
	InvalidateBoundingBox();
	if (ComponentRigidBody* rigidBody = FindRigidBody())
		rigidBody->RebuildBody();
}
