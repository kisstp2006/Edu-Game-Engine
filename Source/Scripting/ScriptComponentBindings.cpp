#include "ScriptComponentBindings.h"

#include "ScriptMath.h"
#include "ScriptObjectReference.h"

#include "../Application.h"
#include "../Component.h"
#include "../ComponentRigidBody.h"
#include "../GameObject.h"

#include <angelscript.h>
#include <scriptarray/scriptarray.h>

#include <array>
#include <string>

#ifdef GetObject
#undef GetObject
#endif

namespace EGE
{
	namespace
	{
		struct ComponentBinding
		{
			const char* scriptType = nullptr;
			Component::Types engineType = Component::Unknown;
			bool creatable = false;
			bool removable = false;
		};

		constexpr std::array<ComponentBinding, 1> ComponentBindings{{
			{"RigidBody", Component::RigidBody, true, true}
		}};

		void SetScriptException(const char* message)
		{
			if (asIScriptContext* context = asGetActiveContext())
				context->SetException(message);
		}

		const ComponentBinding* FindBinding(
			asIScriptEngine& engine,
			int typeId)
		{
			asITypeInfo* type = engine.GetTypeInfoById(typeId);
			if (!type)
				return nullptr;

			for (const ComponentBinding& binding : ComponentBindings)
			{
				if (binding.scriptType == std::string(type->GetName()))
					return &binding;
			}
			return nullptr;
		}

		const ComponentBinding* GetTemplateBinding(
			asIScriptGeneric& generic)
		{
			asIScriptFunction* function = generic.GetFunction();
			if (!function || function->GetSubTypeCount() != 1)
				return nullptr;
			return FindBinding(
				*generic.GetEngine(),
				function->GetSubTypeId());
		}

		GameObject* ResolveOwner(
			const ScriptGameObjectReference* reference)
		{
			GameObject* owner = reference ? reference->Resolve() : nullptr;
			if (!owner)
			{
				SetScriptException(
					"The GameObject reference is no longer valid.");
			}
			return owner;
		}

		Component* FindFirstComponent(
			GameObject& owner,
			const ComponentBinding& binding)
		{
			for (Component* component : owner.components)
			{
				if (component &&
					!component->flag_for_removal &&
					component->GetType() == binding.engineType)
				{
					return component;
				}
			}
			return nullptr;
		}

		ScriptComponentReference* MakeReference(
			GameObject& owner,
			Component* component)
		{
			return component
				? MakeComponentReference(
					owner.GetUID(), component->GetUID())
				: nullptr;
		}

		void GetComponentTemplate(asIScriptGeneric* generic)
		{
			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			if (!binding)
			{
				SetScriptException(
					"The requested type is not a registered engine component.");
				return;
			}

			GameObject* owner = ResolveOwner(ownerReference);
			ScriptComponentReference* result = owner
				? MakeReference(
					*owner,
					FindFirstComponent(*owner, *binding))
				: nullptr;
			*static_cast<ScriptComponentReference**>(
				generic->GetAddressOfReturnLocation()) = result;
		}

		void HasComponentTemplate(asIScriptGeneric* generic)
		{
			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			GameObject* owner =
				binding ? ResolveOwner(ownerReference) : nullptr;
			const bool result =
				owner && FindFirstComponent(*owner, *binding);
			*static_cast<bool*>(
				generic->GetAddressOfReturnLocation()) = result;
			if (!binding)
			{
				SetScriptException(
					"The requested type is not a registered engine component.");
			}
		}

		void TryGetComponentTemplate(asIScriptGeneric* generic)
		{
			auto** output =
				static_cast<ScriptComponentReference**>(
					generic->GetArgAddress(0));
			if (output)
				*output = nullptr;

			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			GameObject* owner =
				binding ? ResolveOwner(ownerReference) : nullptr;
			Component* component =
				owner
					? FindFirstComponent(*owner, *binding)
					: nullptr;
			if (output && component)
				*output = MakeReference(*owner, component);

			*static_cast<bool*>(
				generic->GetAddressOfReturnLocation()) =
					component != nullptr;
			if (!binding)
			{
				SetScriptException(
					"The requested type is not a registered engine component.");
			}
		}

		void GetComponentsTemplate(asIScriptGeneric* generic)
		{
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			if (!binding)
			{
				SetScriptException(
					"The requested type is not a registered engine component.");
				return;
			}

			const std::string declaration =
				"array<" + std::string(binding->scriptType) + "@>";
			asITypeInfo* arrayType =
				generic->GetEngine()->GetTypeInfoByDecl(
					declaration.c_str());
			if (!arrayType)
			{
				SetScriptException(
					"The component array type could not be created.");
				return;
			}

			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			GameObject* owner = ResolveOwner(ownerReference);
			CScriptArray* result = CScriptArray::Create(arrayType);
			if (owner)
			{
				for (Component* component : owner->components)
				{
					if (!component ||
						component->flag_for_removal ||
						component->GetType() != binding->engineType)
					{
						continue;
					}

					ScriptComponentReference* reference =
						MakeReference(*owner, component);
					result->InsertLast(&reference);
					reference->Release();
				}
			}

			*static_cast<CScriptArray**>(
				generic->GetAddressOfReturnLocation()) = result;
		}

		void AddComponentTemplate(asIScriptGeneric* generic)
		{
			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			if (!binding || !binding->creatable)
			{
				SetScriptException(
					"The requested component type cannot be created.");
				return;
			}

			GameObject* owner = ResolveOwner(ownerReference);
			Component* component =
				owner
					? owner->CreateComponent(binding->engineType)
					: nullptr;
			if (component && App && App->IsPlay())
			{
				component->OnStart();
				component->OnPlay();
			}

			*static_cast<ScriptComponentReference**>(
				generic->GetAddressOfReturnLocation()) =
					owner ? MakeReference(*owner, component) : nullptr;
		}

		void RemoveComponent(
			ScriptComponentReference* componentReference,
			const ScriptGameObjectReference* ownerReference)
		{
			GameObject* owner = ResolveOwner(ownerReference);
			Component* component =
				componentReference
					? componentReference->Resolve()
					: nullptr;
			if (!owner || !component)
				return;
			if (component->GetGameObject() != owner)
			{
				SetScriptException(
					"The component belongs to another GameObject.");
				return;
			}

			const ComponentBinding* binding = nullptr;
			for (const ComponentBinding& candidate : ComponentBindings)
			{
				if (candidate.engineType == component->GetType())
				{
					binding = &candidate;
					break;
				}
			}
			if (!binding || !binding->removable)
			{
				SetScriptException(
					"The component type cannot be removed from script.");
				return;
			}
			owner->RemoveComponent(component);
		}

		bool GetComponentEnabled(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			return component && component->IsActive();
		}

		void SetComponentEnabled(
			bool enabled,
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (component)
				component->SetActive(enabled);
		}

		std::string GetComponentTypeName(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			return component
				? component->GetTypeStr()
				: std::string();
		}

		ScriptGameObjectReference* GetComponentGameObject(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			GameObject* owner =
				component ? component->GetGameObject() : nullptr;
			return owner
				? MakeGameObjectReference(owner->GetUID())
				: nullptr;
		}

		bool ComponentsEqual(
			const ScriptComponentReference* other,
			const ScriptComponentReference* reference)
		{
			return other == reference ||
				(other && reference &&
					other->GetObjectId() ==
						reference->GetObjectId() &&
					other->GetComponentId() ==
						reference->GetComponentId());
		}

		ScriptComponentReference* CastRigidBodyToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToRigidBody(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::RigidBody)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ComponentRigidBody* ResolveRigidBody(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::RigidBody)
			{
				SetScriptException(
					"The RigidBody reference is no longer valid.");
				return nullptr;
			}
			return static_cast<ComponentRigidBody*>(component);
		}

		ScriptVector3 ToScriptVector(const float3& value)
		{
			return {value.x, value.y, value.z};
		}

		float3 ToEngineVector(const ScriptVector3& value)
		{
			return {value.x, value.y, value.z};
		}

		int GetRigidBodyType(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body ? static_cast<int>(body->GetBodyType()) : 0;
		}

		void SetRigidBodyType(
			int value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
			{
				body->SetBodyType(
					static_cast<ComponentRigidBody::BodyType>(value));
			}
		}

		int GetRigidBodyBehaviour(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body
				? static_cast<int>(body->GetBehaviour())
				: 0;
		}

		void SetRigidBodyBehaviour(
			int value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
			{
				body->SetBehaviour(
					static_cast<ComponentRigidBody::BodyBehaviour>(
						value));
			}
		}

#define EGE_RIGID_FLOAT_PROPERTY(Name) \
		float Get##Name(const ScriptComponentReference* reference) \
		{ \
			ComponentRigidBody* body = ResolveRigidBody(reference); \
			return body ? body->Get##Name() : 0.0f; \
		} \
		void Set##Name(float value, ScriptComponentReference* reference) \
		{ \
			if (ComponentRigidBody* body = ResolveRigidBody(reference)) \
				body->Set##Name(value); \
		}

		EGE_RIGID_FLOAT_PROPERTY(Mass)
		EGE_RIGID_FLOAT_PROPERTY(Restitution)
		EGE_RIGID_FLOAT_PROPERTY(Friction)
		EGE_RIGID_FLOAT_PROPERTY(RollingFriction)
		EGE_RIGID_FLOAT_PROPERTY(LinearDamping)
		EGE_RIGID_FLOAT_PROPERTY(AngularDamping)
		EGE_RIGID_FLOAT_PROPERTY(SphereRadius)
		EGE_RIGID_FLOAT_PROPERTY(CapsuleRadius)

#undef EGE_RIGID_FLOAT_PROPERTY

#define EGE_RIGID_VECTOR_PROPERTY(Name) \
		ScriptVector3 Get##Name( \
			const ScriptComponentReference* reference) \
		{ \
			ComponentRigidBody* body = ResolveRigidBody(reference); \
			return body \
				? ToScriptVector(body->Get##Name()) \
				: ScriptVector3{}; \
		} \
		void Set##Name( \
			const ScriptVector3& value, \
			ScriptComponentReference* reference) \
		{ \
			if (ComponentRigidBody* body = ResolveRigidBody(reference)) \
				body->Set##Name(ToEngineVector(value)); \
		}

		EGE_RIGID_VECTOR_PROPERTY(LinearFactor)
		EGE_RIGID_VECTOR_PROPERTY(AngularFactor)
		EGE_RIGID_VECTOR_PROPERTY(SphereCenter)
		EGE_RIGID_VECTOR_PROPERTY(BoxCenter)
		EGE_RIGID_VECTOR_PROPERTY(BoxHalfExtents)
		EGE_RIGID_VECTOR_PROPERTY(CapsuleStart)
		EGE_RIGID_VECTOR_PROPERTY(CapsuleEnd)
		EGE_RIGID_VECTOR_PROPERTY(LinearVelocity)
		EGE_RIGID_VECTOR_PROPERTY(AngularVelocity)
		EGE_RIGID_VECTOR_PROPERTY(Gravity)

#undef EGE_RIGID_VECTOR_PROPERTY

#define EGE_RIGID_READONLY_VECTOR(Name) \
		ScriptVector3 Get##Name( \
			const ScriptComponentReference* reference) \
		{ \
			ComponentRigidBody* body = ResolveRigidBody(reference); \
			return body \
				? ToScriptVector(body->Get##Name()) \
				: ScriptVector3{}; \
		}

		EGE_RIGID_READONLY_VECTOR(CenterOfMass)
		EGE_RIGID_READONLY_VECTOR(TotalForce)
		EGE_RIGID_READONLY_VECTOR(TotalTorque)

#undef EGE_RIGID_READONLY_VECTOR

		bool GetHasRuntimeBody(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body && body->HasRuntimeBody();
		}

		bool GetIsAwake(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body && body->IsAwake();
		}

		void WakeUp(ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->WakeUp();
		}

		void Sleep(ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->Sleep();
		}

		void ClearForces(ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ClearForces();
		}

		void ApplyCentralForce(
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ApplyCentralForce(ToEngineVector(value));
		}

		void ApplyForce(
			const ScriptVector3& force,
			const ScriptVector3& relativePosition,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
			{
				body->ApplyForce(
					ToEngineVector(force),
					ToEngineVector(relativePosition));
			}
		}

		void ApplyCentralImpulse(
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ApplyCentralImpulse(ToEngineVector(value));
		}

		void ApplyImpulse(
			const ScriptVector3& impulse,
			const ScriptVector3& relativePosition,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
			{
				body->ApplyImpulse(
					ToEngineVector(impulse),
					ToEngineVector(relativePosition));
			}
		}

		void ApplyTorque(
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ApplyTorque(ToEngineVector(value));
		}

		void ApplyTorqueImpulse(
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ApplyTorqueImpulse(ToEngineVector(value));
		}

		void RebuildBody(ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->RebuildBody();
		}

		bool RegisterComponentHandle(
			asIScriptEngine& engine,
			const char* type,
			std::string& error)
		{
			const bool registered =
				engine.RegisterObjectType(type, 0, asOBJ_REF) >= 0 &&
				engine.RegisterObjectBehaviour(
					type, asBEHAVE_ADDREF, "void f()",
					asMETHOD(ScriptComponentReference, AddRef),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectBehaviour(
					type, asBEHAVE_RELEASE, "void f()",
					asMETHOD(ScriptComponentReference, Release),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "uint get_id() const property",
					asMETHOD(
						ScriptComponentReference,
						GetComponentId),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "bool get_valid() const property",
					asMETHOD(ScriptComponentReference, IsValid),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "bool get_enabled() const property",
					asFUNCTION(GetComponentEnabled),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "void set_enabled(bool) property",
					asFUNCTION(SetComponentEnabled),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "string get_typeName() const property",
					asFUNCTION(GetComponentTypeName),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "GameObject@ get_gameObject() const property",
					asFUNCTION(GetComponentGameObject),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type,
					("bool opEquals(const " +
					 std::string(type) +
					 "@+ other) const").c_str(),
					asFUNCTION(ComponentsEqual),
					asCALL_CDECL_OBJLAST) >= 0;
			if (registered)
				return true;
			error =
				"Could not register the '" +
				std::string(type) + "' component handle.";
			return false;
		}

		bool RegisterRigidBodyApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(engine, "RigidBody", error))
				return false;

			const bool enums =
				engine.RegisterEnum("RigidBodyType") >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyType", "Sphere",
					ComponentRigidBody::body_sphere) >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyType", "Box",
					ComponentRigidBody::body_box) >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyType", "Capsule",
					ComponentRigidBody::body_capsule) >= 0 &&
				engine.RegisterEnum("RigidBodyBehaviour") >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyBehaviour", "Fixed",
					ComponentRigidBody::fixed) >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyBehaviour", "Dynamic",
					ComponentRigidBody::dynamic) >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyBehaviour", "Kinematic",
					ComponentRigidBody::kinematic) >= 0;
			if (!enums)
			{
				error = "Could not register the RigidBody enums.";
				return false;
			}

			const bool baseApi =
				engine.RegisterObjectMethod(
					"RigidBody", "Component@+ opImplCast()",
					asFUNCTION(CastRigidBodyToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component", "RigidBody@+ opCast()",
					asFUNCTION(CastComponentToRigidBody),
					asCALL_CDECL_OBJLAST) >= 0;
			if (!baseApi)
			{
				error = "Could not register RigidBody component casts.";
				return false;
			}

#define EGE_REGISTER_RIGID_FLOAT(Property, Name) \
			engine.RegisterObjectMethod( \
				"RigidBody", "float get_" Property "() const property", \
				asFUNCTION(Get##Name), asCALL_CDECL_OBJLAST) >= 0 && \
			engine.RegisterObjectMethod( \
				"RigidBody", "void set_" Property "(float) property", \
				asFUNCTION(Set##Name), asCALL_CDECL_OBJLAST) >= 0

#define EGE_REGISTER_RIGID_VECTOR(Property, Name) \
			engine.RegisterObjectMethod( \
				"RigidBody", \
				"Vector3 get_" Property "() const property", \
				asFUNCTION(Get##Name), asCALL_CDECL_OBJLAST) >= 0 && \
			engine.RegisterObjectMethod( \
				"RigidBody", \
				"void set_" Property \
					"(const Vector3 &in) property", \
				asFUNCTION(Set##Name), asCALL_CDECL_OBJLAST) >= 0

#define EGE_REGISTER_RIGID_READONLY_VECTOR(Property, Name) \
			engine.RegisterObjectMethod( \
				"RigidBody", \
				"Vector3 get_" Property "() const property", \
				asFUNCTION(Get##Name), asCALL_CDECL_OBJLAST) >= 0

			const bool properties =
				engine.RegisterObjectMethod(
					"RigidBody",
					"RigidBodyType get_bodyType() const property",
					asFUNCTION(GetRigidBodyType),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void set_bodyType(RigidBodyType) property",
					asFUNCTION(SetRigidBodyType),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"RigidBodyBehaviour get_behaviour() const property",
					asFUNCTION(GetRigidBodyBehaviour),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void set_behaviour("
						"RigidBodyBehaviour) property",
					asFUNCTION(SetRigidBodyBehaviour),
					asCALL_CDECL_OBJLAST) >= 0 &&
				EGE_REGISTER_RIGID_FLOAT("mass", Mass) &&
				EGE_REGISTER_RIGID_FLOAT(
					"restitution", Restitution) &&
				EGE_REGISTER_RIGID_FLOAT("friction", Friction) &&
				EGE_REGISTER_RIGID_FLOAT(
					"rollingFriction", RollingFriction) &&
				EGE_REGISTER_RIGID_FLOAT(
					"linearDamping", LinearDamping) &&
				EGE_REGISTER_RIGID_FLOAT(
					"angularDamping", AngularDamping) &&
				EGE_REGISTER_RIGID_VECTOR(
					"linearFactor", LinearFactor) &&
				EGE_REGISTER_RIGID_VECTOR(
					"angularFactor", AngularFactor) &&
				EGE_REGISTER_RIGID_VECTOR(
					"sphereCenter", SphereCenter) &&
				EGE_REGISTER_RIGID_FLOAT(
					"sphereRadius", SphereRadius) &&
				EGE_REGISTER_RIGID_VECTOR(
					"boxCenter", BoxCenter) &&
				EGE_REGISTER_RIGID_VECTOR(
					"boxHalfExtents", BoxHalfExtents) &&
				EGE_REGISTER_RIGID_VECTOR(
					"capsuleStart", CapsuleStart) &&
				EGE_REGISTER_RIGID_VECTOR(
					"capsuleEnd", CapsuleEnd) &&
				EGE_REGISTER_RIGID_FLOAT(
					"capsuleRadius", CapsuleRadius) &&
				EGE_REGISTER_RIGID_VECTOR(
					"linearVelocity", LinearVelocity) &&
				EGE_REGISTER_RIGID_VECTOR(
					"angularVelocity", AngularVelocity) &&
				EGE_REGISTER_RIGID_VECTOR("gravity", Gravity) &&
				EGE_REGISTER_RIGID_READONLY_VECTOR(
					"centerOfMass", CenterOfMass) &&
				EGE_REGISTER_RIGID_READONLY_VECTOR(
					"totalForce", TotalForce) &&
				EGE_REGISTER_RIGID_READONLY_VECTOR(
					"totalTorque", TotalTorque) &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"bool get_hasRuntimeBody() const property",
					asFUNCTION(GetHasRuntimeBody),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"bool get_isAwake() const property",
					asFUNCTION(GetIsAwake),
					asCALL_CDECL_OBJLAST) >= 0;

#undef EGE_REGISTER_RIGID_FLOAT
#undef EGE_REGISTER_RIGID_VECTOR
#undef EGE_REGISTER_RIGID_READONLY_VECTOR

			const bool methods =
				engine.RegisterObjectMethod(
					"RigidBody", "void WakeUp()",
					asFUNCTION(WakeUp),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody", "void Sleep()",
					asFUNCTION(Sleep),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody", "void ClearForces()",
					asFUNCTION(ClearForces),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyCentralForce("
						"const Vector3 &in force)",
					asFUNCTION(ApplyCentralForce),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyForce("
						"const Vector3 &in force, "
						"const Vector3 &in relativePosition)",
					asFUNCTION(ApplyForce),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyCentralImpulse("
						"const Vector3 &in impulse)",
					asFUNCTION(ApplyCentralImpulse),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyImpulse("
						"const Vector3 &in impulse, "
						"const Vector3 &in relativePosition)",
					asFUNCTION(ApplyImpulse),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyTorque("
						"const Vector3 &in torque)",
					asFUNCTION(ApplyTorque),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyTorqueImpulse("
						"const Vector3 &in torque)",
					asFUNCTION(ApplyTorqueImpulse),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody", "void RebuildBody()",
					asFUNCTION(RebuildBody),
					asCALL_CDECL_OBJLAST) >= 0;

			if (properties && methods)
				return true;
			error = "Could not register the complete RigidBody API.";
			return false;
		}

		bool ValidateTypedComponentApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			constexpr const char* ModuleName =
				"__EGE_TypedComponentApiValidation";
			constexpr const char* Source = R"(
void ValidateTypedComponentApi(GameObject@ object)
{
    RigidBody@ first = object.GetComponent<RigidBody>();
    bool hasBody = object.HasComponent<RigidBody>();
    RigidBody@ found;
    bool foundBody = object.TryGetComponent<RigidBody>(found);
    array<RigidBody@>@ bodies =
        object.GetComponents<RigidBody@>();
    RigidBody@ added = object.AddComponent<RigidBody>();
    Component@ component = added;
    RigidBody@ casted = cast<RigidBody>(component);
    if (first !is null)
    {
        uint componentId = first.id;
        bool componentValid = first.valid;
        first.enabled = !first.enabled;
        string componentType = first.typeName;
        GameObject@ owner = first.gameObject;

        first.bodyType = RigidBodyType::Box;
        first.behaviour = RigidBodyBehaviour::Dynamic;
        first.mass = first.mass;
        first.restitution = first.restitution;
        first.friction = first.friction;
        first.rollingFriction = first.rollingFriction;
        first.linearDamping = first.linearDamping;
        first.angularDamping = first.angularDamping;
        first.linearFactor = first.linearFactor;
        first.angularFactor = first.angularFactor;
        first.sphereCenter = first.sphereCenter;
        first.sphereRadius = first.sphereRadius;
        first.boxCenter = first.boxCenter;
        first.boxHalfExtents = first.boxHalfExtents;
        first.capsuleStart = first.capsuleStart;
        first.capsuleEnd = first.capsuleEnd;
        first.capsuleRadius = first.capsuleRadius;
        first.linearVelocity = first.linearVelocity;
        first.angularVelocity = first.angularVelocity;
        first.gravity = first.gravity;
        Vector3 center = first.centerOfMass;
        Vector3 force = first.totalForce;
        Vector3 torque = first.totalTorque;
        bool hasRuntimeBody = first.hasRuntimeBody;
        bool awake = first.isAwake;

        first.WakeUp();
        first.Sleep();
        first.ClearForces();
        first.ApplyCentralForce(force);
        first.ApplyForce(force, center);
        first.ApplyCentralImpulse(force);
        first.ApplyImpulse(force, center);
        first.ApplyTorque(torque);
        first.ApplyTorqueImpulse(torque);
        first.RebuildBody();
    }
    object.RemoveComponent(component);
}
)";

			asIScriptModule* module =
				engine.GetModule(ModuleName, asGM_ALWAYS_CREATE);
			if (!module ||
				module->AddScriptSection(
					"TypedComponentApiValidation",
					Source) < 0 ||
				module->Build() < 0)
			{
				engine.DiscardModule(ModuleName);
				error =
					"The typed Component API did not pass its "
					"AngelScript compile validation.";
				return false;
			}

			engine.DiscardModule(ModuleName);
			return true;
		}
	}

	bool RegisterTypedComponentApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		const bool componentBaseRegistered =
			engine.RegisterObjectMethod(
				"Component", "bool get_enabled() const property",
				asFUNCTION(GetComponentEnabled),
				asCALL_CDECL_OBJLAST) >= 0 &&
			engine.RegisterObjectMethod(
				"Component", "void set_enabled(bool) property",
				asFUNCTION(SetComponentEnabled),
				asCALL_CDECL_OBJLAST) >= 0;
		if (!componentBaseRegistered)
		{
			error = "Could not register the Component base API.";
			return false;
		}

		if (!RegisterRigidBodyApi(engine, error))
			return false;

		const bool registered =
			engine.RegisterObjectMethod(
				"GameObject",
				"T@ GetComponent<T>() const",
				asFUNCTION(GetComponentTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"bool HasComponent<T>() const",
				asFUNCTION(HasComponentTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"bool TryGetComponent<T>(T@&out component) const",
				asFUNCTION(TryGetComponentTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"array<T>@ GetComponents<T>() const",
				asFUNCTION(GetComponentsTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"T@ AddComponent<T>()",
				asFUNCTION(AddComponentTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"void RemoveComponent(Component@+ component)",
				asFUNCTION(RemoveComponent),
				asCALL_CDECL_OBJLAST) >= 0;
		if (registered)
			return ValidateTypedComponentApi(engine, error);

		error = "Could not register the typed GameObject component API.";
		return false;
	}
}
