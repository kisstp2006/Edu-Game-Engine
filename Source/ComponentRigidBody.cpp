#include "Globals.h"
#include "Application.h"
#include "ComponentRigidBody.h"
#include "GameObject.h"
#include "Component.h"
#include "ModulePhysics3D.h"
#include "DebugDraw.h"
#include <btBulletDynamicsCommon.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>

using namespace std;

// ---------------------------------------------------------
ComponentRigidBody::ComponentRigidBody(GameObject* container) : Component(container, Types::RigidBody)
{
	sphere.r = 1.0f;
	sphere.pos = float3::zero;

	box.r = float3::one;
	box.pos = float3::zero;
	box.axis[0] = float3::unitX;
	box.axis[1] = float3::unitY;
	box.axis[2] = float3::unitZ;

	capsule.r = 1.0f;
	capsule.l = LineSegment(float3::zero, float3::one);
}

// ---------------------------------------------------------
ComponentRigidBody::~ComponentRigidBody()
{
	if (body != nullptr && App->physics3D != nullptr)
		App->physics3D->DeleteBody(body);
}

// ---------------------------------------------------------
void ComponentRigidBody::GetBoundingBox(AABB & box) const
{
	switch (body_type)
	{
		case body_sphere:
			box.Enclose(sphere);
		break;
		case body_box:
			box.Enclose(this->box);
		break;
		case body_capsule:
			box.Enclose(capsule);
		break;
	}
}

// ---------------------------------------------------------
void ComponentRigidBody::OnSave(Config& config) const
{
	config.AddInt("Behaviour", behaviour);
	config.AddInt("Body Type", body_type);
	config.AddFloat("Mass", mass);
	config.AddArrayFloat("Sphere", &sphere.pos.x, 4);
	config.AddArrayFloat("Box", &box.pos.x, 6);
	config.AddArrayFloat("Capsule", &capsule.l.a.x, 7);
	config.AddArrayFloat("Linear Factor", &linear_factor.x, 3);
	config.AddArrayFloat("Angular Factor", &angular_factor.x, 3);
	config.AddFloat("Restitution", restitution);
	config.AddFloat("Friction", friction);
	config.AddFloat("Rolling Friction", rolling_friction);
	config.AddFloat("Linear Damping", linear_damping);
	config.AddFloat("Angular Damping", angular_damping);
	config.AddBool("Use World Gravity", use_world_gravity);
	config.AddFloat3("Gravity", gravity);
}

// ---------------------------------------------------------
void ComponentRigidBody::OnLoad(Config * config)
{
	behaviour = (BodyBehaviour) config->GetInt("Behaviour", BodyBehaviour::fixed);
	body_type = (BodyType) config->GetInt("Body Type", BodyType::body_sphere);
	mass = config->GetFloat("Mass", 1.0f);

	sphere.pos.x = config->GetFloat("Sphere", 0.f, 0);
	sphere.pos.y = config->GetFloat("Sphere", 0.f, 1);
	sphere.pos.z = config->GetFloat("Sphere", 0.f, 2);
	sphere.r = config->GetFloat("Sphere", 1.f, 3);

	capsule.l.a.x = config->GetFloat("Capsule", 0.f, 0);
	capsule.l.a.y = config->GetFloat("Capsule", 0.f, 1);
	capsule.l.a.z = config->GetFloat("Capsule", 0.f, 2);
	capsule.l.b.x = config->GetFloat("Capsule", 0.f, 3);
	capsule.l.b.y = config->GetFloat("Capsule", 1.f, 4);
	capsule.l.b.z = config->GetFloat("Capsule", 0.f, 5);
	capsule.r = config->GetFloat("Capsule", 1.f, 6);

	box.pos.x = config->GetFloat("Box", 0.f, 0);
	box.pos.y = config->GetFloat("Box", 0.f, 1);
	box.pos.z = config->GetFloat("Box", 0.f, 2);
	box.r.x = config->GetFloat("Box", 1.f, 3);
	box.r.y = config->GetFloat("Box", 1.f, 4);
	box.r.z = config->GetFloat("Box", 1.f, 5);

	restitution = config->GetFloat("Restitution", 1.0f);
	friction = config->GetFloat("Friction", 0.5f);
	rolling_friction =
		config->GetFloat("Rolling Friction", 0.0f);
	linear_damping =
		config->GetFloat("Linear Damping", 0.0f);
	angular_damping =
		config->GetFloat("Angular Damping", 0.0f);
	gravity = config->GetFloat3(
		"Gravity", float3(0.0f, -10.0f, 0.0f));
	const bool hasLegacyCustomGravity =
		!gravity.Equals(
			float3(0.0f, -10.0f, 0.0f),
			0.0001f);
	use_world_gravity = config->GetBool(
		"Use World Gravity",
		!hasLegacyCustomGravity);

	linear_factor.x = config->GetFloat("Linear Factor", 1.f, 0);
	linear_factor.y = config->GetFloat("Linear Factor", 1.f, 1);
	linear_factor.z = config->GetFloat("Linear Factor", 1.f, 2);

	angular_factor.x = config->GetFloat("Angular Factor", 1.f, 0);
	angular_factor.y = config->GetFloat("Angular Factor", 1.f, 1);
	angular_factor.z = config->GetFloat("Angular Factor", 1.f, 2);
}

// ---------------------------------------------------------
void ComponentRigidBody::OnActivate()
{
	if (App && App->IsPlay() && !body)
		CreateBody();
}

// ---------------------------------------------------------
void ComponentRigidBody::OnDeActivate()
{
	if (body && App && App->physics3D)
	{
		App->physics3D->DeleteBody(body);
		body = nullptr;
	}
}

// ---------------------------------------------------------
void ComponentRigidBody::OnPlay()
{
	if (IsActive())
		CreateBody();
}

void ComponentRigidBody::OnFixedUpdate(float deltaTime)
{
	if (!body)
		return;

	const float3 currentScale = GetAbsoluteGlobalScale();
	if (!currentScale.Equals(collision_scale, 0.0001f))
		RebuildBody();
}

// ---------------------------------------------------------
void ComponentRigidBody::OnStop()
{
	if (body != nullptr)
	{
		App->physics3D->DeleteBody(body);
		body = nullptr;
	}
}

// ---------------------------------------------------------
void ComponentRigidBody::OnDebugDraw(bool selected) const
{
	if (selected == false)
		return;

	const float4x4& globalTransform =
		game_object->GetGlobalTransformation();
	const float3 globalScale = GetAbsoluteGlobalScale();
	const float radiusScale = std::max({
		globalScale.x,
		globalScale.y,
		globalScale.z});
	const Quat globalRotation =
		globalTransform.RotatePart().ToQuat().Normalized();
	switch (body_type)
	{
		case body_sphere:
		{
			dd::sphere(
				globalTransform.TransformPos(sphere.pos),
				dd::colors::Green,
				sphere.r * radiusScale);
		}
		break;
		case body_box:
		{
			OBB worldBox;
			worldBox.pos = globalTransform.TransformPos(box.pos);
			worldBox.r = float3(
				box.r.x * globalScale.x,
				box.r.y * globalScale.y,
				box.r.z * globalScale.z);
			for (int axis = 0; axis < 3; ++axis)
			{
				worldBox.axis[axis] =
					globalRotation.Transform(box.axis[axis]).Normalized();
			}
			float3 corners[8];
			worldBox.GetCornerPoints(corners);
			dd::box(corners, dd::colors::Green);
		}
		break;
		case body_capsule:
        {
			const float3 worldStart =
				globalTransform.TransformPos(capsule.l.a);
			const float3 worldEnd =
				globalTransform.TransformPos(capsule.l.b);
            float3 pos = (worldStart + worldEnd) * 0.5f;
            float3 dir = worldEnd - worldStart;
            float len = dir.Length();

			if (len > 0.0001f)
			{
				dd::capsule(
					pos,
					dd::colors::Green,
					capsule.r * radiusScale,
					dir / len,
					len);
			}
        }
		break;
	}
}

// ---------------------------------------------------------
void ComponentRigidBody::getWorldTransform(btTransform & worldTrans) const
{
	worldTrans.setOrigin(game_object->GetGlobalTransformation().TranslatePart());
	worldTrans.setRotation(game_object->GetGlobalTransformation().RotatePart().ToQuat());
}

// ---------------------------------------------------------
void ComponentRigidBody::setWorldTransform(const btTransform & worldTrans)
{
	const Quat globalRotation(worldTrans.getRotation());
	const float3 globalPosition(worldTrans.getOrigin());

	GameObject* parent = game_object->GetParent();
	float3 localPosition = globalPosition;
	Quat localRotation = globalRotation;
	if (parent)
	{
		const float4x4& parentTransform =
			parent->GetGlobalTransformation();
		localPosition =
			parentTransform.Inverted().TransformPos(globalPosition);

		float3 parentPosition;
		float3 parentScale;
		Quat parentRotation;
		parentTransform.Decompose(
			parentPosition,
			parentRotation,
			parentScale);
		localRotation =
			(parentRotation.Inverted() * globalRotation).Normalized();
	}

	game_object->SetLocalPosition(localPosition);
	game_object->SetLocalRotation(localRotation);
}

// ---------------------------------------------------------
void ComponentRigidBody::CreateBody()
{
	if (!App || !App->physics3D)
		return;

	if (body != nullptr)
		App->physics3D->DeleteBody(body);

	switch (body_type)
	{
		case body_sphere:
			body = App->physics3D->AddBody(sphere, this);
		break;
		case body_box:
			body = App->physics3D->AddBody(box, this);
		break;
		case body_capsule:
			body = App->physics3D->AddBody(capsule, this);
		break;
	}

	if (body != nullptr)
	{
		collision_scale = GetAbsoluteGlobalScale();
		ApplyBodyConfiguration();
	}
}

// ---------------------------------------------------------
void ComponentRigidBody::SetBodyType(BodyType new_type)
{
	if (new_type >= body_sphere &&
		new_type <= body_capsule &&
		new_type != body_type)
	{
		body_type = new_type;
		if (body)
			RebuildBody();
	}
}

// ---------------------------------------------------------
ComponentRigidBody::BodyType ComponentRigidBody::GetBodyType() const
{
	return body_type;
}

// ---------------------------------------------------------
void ComponentRigidBody::SetBehaviour(BodyBehaviour new_behaviour)
{
	if (new_behaviour >= fixed &&
		new_behaviour <= kinematic &&
		new_behaviour != behaviour)
	{
		behaviour = new_behaviour;
		if (body)
			ApplyBodyConfiguration();
	}
}

// ---------------------------------------------------------
ComponentRigidBody::BodyBehaviour ComponentRigidBody::GetBehaviour() const
{
	return behaviour;
}

float ComponentRigidBody::GetMass() const
{
	return mass;
}

void ComponentRigidBody::SetMass(float value)
{
	mass = std::max(value, 0.0001f);
	if (body)
		ApplyBodyConfiguration();
}

float ComponentRigidBody::GetRestitution() const
{
	return restitution;
}

void ComponentRigidBody::SetRestitution(float value)
{
	restitution = std::clamp(value, 0.0f, 1.0f);
	if (body)
		body->setRestitution(restitution);
}

float ComponentRigidBody::GetFriction() const
{
	return friction;
}

void ComponentRigidBody::SetFriction(float value)
{
	friction = std::max(value, 0.0f);
	if (body)
		body->setFriction(friction);
}

float ComponentRigidBody::GetRollingFriction() const
{
	return rolling_friction;
}

void ComponentRigidBody::SetRollingFriction(float value)
{
	rolling_friction = std::max(value, 0.0f);
	if (body)
		body->setRollingFriction(rolling_friction);
}

float ComponentRigidBody::GetLinearDamping() const
{
	return linear_damping;
}

void ComponentRigidBody::SetLinearDamping(float value)
{
	linear_damping = std::clamp(value, 0.0f, 1.0f);
	if (body)
		body->setDamping(linear_damping, angular_damping);
}

float ComponentRigidBody::GetAngularDamping() const
{
	return angular_damping;
}

void ComponentRigidBody::SetAngularDamping(float value)
{
	angular_damping = std::clamp(value, 0.0f, 1.0f);
	if (body)
		body->setDamping(linear_damping, angular_damping);
}

const float3& ComponentRigidBody::GetLinearFactor() const
{
	return linear_factor;
}

void ComponentRigidBody::SetLinearFactor(const float3& value)
{
	linear_factor = value;
	if (body)
		body->setLinearFactor(linear_factor);
}

const float3& ComponentRigidBody::GetAngularFactor() const
{
	return angular_factor;
}

void ComponentRigidBody::SetAngularFactor(const float3& value)
{
	angular_factor = value;
	if (body)
		body->setAngularFactor(angular_factor);
}

const float3& ComponentRigidBody::GetSphereCenter() const
{
	return sphere.pos;
}

void ComponentRigidBody::SetSphereCenter(const float3& value)
{
	sphere.pos = value;
	if (body)
		RebuildBody();
}

float ComponentRigidBody::GetSphereRadius() const
{
	return sphere.r;
}

void ComponentRigidBody::SetSphereRadius(float value)
{
	sphere.r = std::max(value, 0.001f);
	if (body)
		RebuildBody();
}

const float3& ComponentRigidBody::GetBoxCenter() const
{
	return box.pos;
}

void ComponentRigidBody::SetBoxCenter(const float3& value)
{
	box.pos = value;
	if (body)
		RebuildBody();
}

const float3& ComponentRigidBody::GetBoxHalfExtents() const
{
	return box.r;
}

void ComponentRigidBody::SetBoxHalfExtents(const float3& value)
{
	box.r = float3(
		std::max(value.x, 0.001f),
		std::max(value.y, 0.001f),
		std::max(value.z, 0.001f));
	if (body)
		RebuildBody();
}

const float3& ComponentRigidBody::GetCapsuleStart() const
{
	return capsule.l.a;
}

void ComponentRigidBody::SetCapsuleStart(const float3& value)
{
	capsule.l.a = value;
	if (body)
		RebuildBody();
}

const float3& ComponentRigidBody::GetCapsuleEnd() const
{
	return capsule.l.b;
}

void ComponentRigidBody::SetCapsuleEnd(const float3& value)
{
	capsule.l.b = value;
	if (body)
		RebuildBody();
}

float ComponentRigidBody::GetCapsuleRadius() const
{
	return capsule.r;
}

void ComponentRigidBody::SetCapsuleRadius(float value)
{
	capsule.r = std::max(value, 0.001f);
	if (body)
		RebuildBody();
}

bool ComponentRigidBody::HasRuntimeBody() const
{
	return body != nullptr;
}

float3 ComponentRigidBody::GetLinearVelocity() const
{
	return body ? float3(body->getLinearVelocity()) : float3::zero;
}

void ComponentRigidBody::SetLinearVelocity(const float3& value)
{
	if (body)
	{
		body->setLinearVelocity(value);
		body->activate(true);
	}
}

float3 ComponentRigidBody::GetAngularVelocity() const
{
	return body ? float3(body->getAngularVelocity()) : float3::zero;
}

void ComponentRigidBody::SetAngularVelocity(const float3& value)
{
	if (body)
	{
		body->setAngularVelocity(value);
		body->activate(true);
	}
}

float3 ComponentRigidBody::GetCenterOfMass() const
{
	return body
		? float3(body->getCenterOfMassPosition())
		: game_object->GetGlobalPosition();
}

float3 ComponentRigidBody::GetTotalForce() const
{
	return body ? float3(body->getTotalForce()) : float3::zero;
}

float3 ComponentRigidBody::GetTotalTorque() const
{
	return body ? float3(body->getTotalTorque()) : float3::zero;
}

bool ComponentRigidBody::GetUseWorldGravity() const
{
	return use_world_gravity;
}

void ComponentRigidBody::SetUseWorldGravity(bool value)
{
	use_world_gravity = value;
	if (body)
		ApplyBodyConfiguration();
}

float3 ComponentRigidBody::GetGravity() const
{
	if (body)
		return float3(body->getGravity());
	if (use_world_gravity &&
		App &&
		App->physics3D)
	{
		return App->physics3D->GetGravity();
	}
	return gravity;
}

void ComponentRigidBody::SetGravity(const float3& value)
{
	gravity = value;
	use_world_gravity = false;
	if (body)
		ApplyBodyConfiguration();
}

bool ComponentRigidBody::IsAwake() const
{
	return body && body->isActive();
}

void ComponentRigidBody::WakeUp()
{
	if (body)
		body->activate(true);
}

void ComponentRigidBody::Sleep()
{
	if (body)
		body->forceActivationState(ISLAND_SLEEPING);
}

void ComponentRigidBody::ClearForces()
{
	if (body)
		body->clearForces();
}

void ComponentRigidBody::ApplyCentralForce(const float3& force)
{
	if (body)
	{
		body->applyCentralForce(force);
		body->activate(true);
	}
}

void ComponentRigidBody::ApplyForce(
	const float3& force,
	const float3& relativePosition)
{
	if (body)
	{
		body->applyForce(force, relativePosition);
		body->activate(true);
	}
}

void ComponentRigidBody::ApplyCentralImpulse(
	const float3& impulse)
{
	if (body)
	{
		body->applyCentralImpulse(impulse);
		body->activate(true);
	}
}

void ComponentRigidBody::ApplyImpulse(
	const float3& impulse,
	const float3& relativePosition)
{
	if (body)
	{
		body->applyImpulse(impulse, relativePosition);
		body->activate(true);
	}
}

void ComponentRigidBody::ApplyTorque(const float3& torque)
{
	if (body)
	{
		body->applyTorque(torque);
		body->activate(true);
	}
}

void ComponentRigidBody::ApplyTorqueImpulse(
	const float3& torque)
{
	if (body)
	{
		body->applyTorqueImpulse(torque);
		body->activate(true);
	}
}

void ComponentRigidBody::RebuildBody()
{
	if (!body)
		return;

	const float3 linearVelocity(body->getLinearVelocity());
	const float3 angularVelocity(body->getAngularVelocity());
	CreateBody();
	SetLinearVelocity(linearVelocity);
	SetAngularVelocity(angularVelocity);
}

float3 ComponentRigidBody::GetAbsoluteGlobalScale() const
{
	if (!game_object)
		return float3::one;

	float3 position;
	float3 scale;
	Quat rotation;
	game_object->GetGlobalTransformation().Decompose(
		position,
		rotation,
		scale);
	return {
		std::max(std::abs(scale.x), 0.0001f),
		std::max(std::abs(scale.y), 0.0001f),
		std::max(std::abs(scale.z), 0.0001f)};
}

void ComponentRigidBody::ApplyBodyConfiguration()
{
	if (!body)
		return;

	int flags = body->getCollisionFlags();
	flags &= ~(
		btCollisionObject::CF_STATIC_OBJECT |
		btCollisionObject::CF_KINEMATIC_OBJECT);

	float runtimeMass = 0.0f;
	if (behaviour == BodyBehaviour::dynamic)
		runtimeMass = mass;
	else if (behaviour == BodyBehaviour::fixed)
		flags |= btCollisionObject::CF_STATIC_OBJECT;
	else
		flags |= btCollisionObject::CF_KINEMATIC_OBJECT;

	btVector3 inertia(0.0f, 0.0f, 0.0f);
	if (runtimeMass > 0.0f && body->getCollisionShape())
	{
		body->getCollisionShape()->calculateLocalInertia(
			runtimeMass, inertia);
	}
	body->setCollisionFlags(flags);
	body->setMassProps(runtimeMass, inertia);
	body->updateInertiaTensor();
	body->setLinearFactor(linear_factor);
	body->setAngularFactor(angular_factor);
	body->setRestitution(restitution);
	body->setFriction(friction);
	body->setRollingFriction(rolling_friction);
	body->setDamping(linear_damping, angular_damping);
	int rigidBodyFlags = body->getFlags();
	if (use_world_gravity)
	{
		rigidBodyFlags &= ~BT_DISABLE_WORLD_GRAVITY;
		body->setGravity(
			App && App->physics3D
				? App->physics3D->GetGravity()
				: float3(0.0f, -10.0f, 0.0f));
	}
	else
	{
		rigidBodyFlags |= BT_DISABLE_WORLD_GRAVITY;
		body->setGravity(gravity);
	}
	body->setFlags(rigidBodyFlags);
	if (behaviour == BodyBehaviour::kinematic)
		body->setActivationState(DISABLE_DEACTIVATION);
	else
		body->activate(true);
}

// ---------------------------------------------------------
void ComponentRigidBody::DrawEditor()
{
	static const char* behaviours[] = { "Fixed", "Dynamic", "Kinematic" };

	int behaviour_type = static_cast<int>(behaviour);
	if (ImGui::Combo("Behaviour", &behaviour_type, behaviours, 3))
		SetBehaviour(static_cast<BodyBehaviour>(behaviour_type));

	static const char* types[] = { "Sphere", "Box", "Capsule" };

	int type = static_cast<int>(body_type);
	if (ImGui::Combo("Type", &type, types, 3))
		SetBodyType(static_cast<BodyType>(type));

	switch (type)
	{
		case BodyType::body_sphere:
			ImGui::DragFloat3(
				"Center", &sphere.pos.x, 0.05f);
			ImGui::DragFloat("Radius", &sphere.r, 0.1f, 0.1f);
		break;
		case BodyType::body_box:
			ImGui::DragFloat3(
				"Center", &box.pos.x, 0.05f);
			ImGui::DragFloat3(
				"Half Extents", &box.r.x, 0.1f, 0.1f);
		break;
		case BodyType::body_capsule:
			ImGui::DragFloat3("Top", &capsule.l.a.x, 0.1f, 0.1f);
			ImGui::DragFloat3("Bottom", &capsule.l.b.x, 0.1f, 0.1f);
			ImGui::DragFloat("Radius", &capsule.r, 0.1f, 0.1f);
		break;
	}

	float value = GetMass();
	if (ImGui::DragFloat("Mass", &value, 0.1f, 0.0001f, 100000.0f))
		SetMass(value);

	value = GetRestitution();
	if (ImGui::DragFloat("Restitution", &value, 0.01f, 0.0f, 1.0f))
		SetRestitution(value);

	value = GetFriction();
	if (ImGui::DragFloat("Friction", &value, 0.01f, 0.0f))
		SetFriction(value);

	value = GetRollingFriction();
	if (ImGui::DragFloat("Rolling Friction", &value, 0.01f, 0.0f))
		SetRollingFriction(value);

	value = GetLinearDamping();
	if (ImGui::DragFloat("Linear Damping", &value, 0.01f, 0.0f, 1.0f))
		SetLinearDamping(value);

	value = GetAngularDamping();
	if (ImGui::DragFloat("Angular Damping", &value, 0.01f, 0.0f, 1.0f))
		SetAngularDamping(value);

	if (ImGui::Button("Commit Changes to Physics Engine"))
		RebuildBody();

	float3 vector = GetLinearFactor();
	if (ImGui::DragFloat3(
			"Linear Factor", &vector.x, 0.05f, 0.0f, 1.0f))
	{
		SetLinearFactor(vector);
	}

	vector = GetAngularFactor();
	if (ImGui::DragFloat3(
			"Angular Factor", &vector.x, 0.05f, 0.0f, 1.0f))
	{
		SetAngularFactor(vector);
	}

	bool useWorldGravity = GetUseWorldGravity();
	if (ImGui::Checkbox("Use World Gravity", &useWorldGravity))
		SetUseWorldGravity(useWorldGravity);

	if (!useWorldGravity)
	{
		vector = GetGravity();
		if (ImGui::DragFloat3("Gravity", &vector.x, 0.05f))
			SetGravity(vector);
	}

	if (body != nullptr)
	{
		float3 data = body->getLinearVelocity();
		IMGUI_PRINT("Linear Velocity: ", "%.2f %.2f %.2f", data.x, data.y, data.z);
		data = body->getAngularVelocity();
		IMGUI_PRINT("Angular Velocity: ", "%.2f %.2f %.2f", data.x, data.y, data.z);
		data = body->getCenterOfMassPosition();
		IMGUI_PRINT("Center of Mass: ", "%.2f %.2f %.2f", data.x, data.y, data.z);
//		data = body->getLocalInertia();
		IMGUI_PRINT("Local Inertia: ", "%.2f %.2f %.2f", data.x, data.y, data.z);
		data = body->getTotalForce();
		IMGUI_PRINT("Total Force: ", "%.2f %.2f %.2f", data.x, data.y, data.z);
		data = body->getTotalTorque();
		IMGUI_PRINT("Total Torque: ", "%.2f %.2f %.2f", data.x, data.y, data.z);
		data.x = body->getFriction();
		data.y = body->getHitFraction();
		data.z = body->getRollingFriction();
		IMGUI_PRINT("Friction/Hit/Rolling: ", "%.2f %.2f %.2f", data.x, data.y, data.z);
	}

}
