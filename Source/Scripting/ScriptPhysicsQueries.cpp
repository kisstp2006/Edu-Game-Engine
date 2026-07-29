#include "../Globals.h"
#include "ScriptPhysicsQueries.h"

#include "ScriptObjectReference.h"

#include "../Application.h"
#include "../ComponentCollider.h"
#include "../ComponentRigidBody.h"
#include "../GameObject.h"
#include "../ModulePhysics3D.h"

#include <angelscript.h>
#include <scriptarray/scriptarray.h>

#include <vector>

namespace EGE
{
	namespace
	{
		float3 ToEngineVector(const ScriptVector3& value)
		{
			return {value.x, value.y, value.z};
		}

		Physics::QueryFilter MakeFilter(
			unsigned int layerMask,
			bool includeTriggers)
		{
			return {
				layerMask & Physics::AllCollisionLayers,
				includeTriggers};
		}

		ModulePhysics3D* GetPhysics()
		{
			return App ? App->physics3D : nullptr;
		}

		bool ScriptRaycast(
			const ScriptVector3& origin,
			const ScriptVector3& direction,
			ScriptRaycastHit** output,
			float maxDistance,
			unsigned int layerMask,
			bool includeTriggers)
		{
			if (output)
				*output = nullptr;
			ModulePhysics3D* physics = GetPhysics();
			if (!physics || !output)
				return false;

			Physics::QueryHit hit;
			if (!physics->Raycast(
					ToEngineVector(origin),
					ToEngineVector(direction),
					maxDistance,
					MakeFilter(layerMask, includeTriggers),
					hit))
			{
				return false;
			}

			*output = new ScriptRaycastHit(hit);
			return true;
		}

		bool ScriptSphereCast(
			const ScriptVector3& origin,
			float radius,
			const ScriptVector3& direction,
			ScriptRaycastHit** output,
			float maxDistance,
			unsigned int layerMask,
			bool includeTriggers)
		{
			if (output)
				*output = nullptr;
			ModulePhysics3D* physics = GetPhysics();
			if (!physics || !output)
				return false;

			Physics::QueryHit hit;
			if (!physics->SphereCast(
					ToEngineVector(origin),
					radius,
					ToEngineVector(direction),
					maxDistance,
					MakeFilter(layerMask, includeTriggers),
					hit))
			{
				return false;
			}

			*output = new ScriptRaycastHit(hit);
			return true;
		}

		CScriptArray* MakeHitArray(
			const std::vector<Physics::QueryHit>& hits)
		{
			asIScriptContext* context = asGetActiveContext();
			asIScriptEngine* engine =
				context ? context->GetEngine() : nullptr;
			asITypeInfo* arrayType = engine
				? engine->GetTypeInfoByDecl(
					"array<RaycastHit@>")
				: nullptr;
			if (!arrayType)
				return nullptr;

			CScriptArray* result =
				CScriptArray::Create(arrayType);
			for (const Physics::QueryHit& hit : hits)
			{
				auto* scriptHit = new ScriptRaycastHit(hit);
				result->InsertLast(&scriptHit);
				scriptHit->Release();
			}
			return result;
		}

		CScriptArray* ScriptRaycastAll(
			const ScriptVector3& origin,
			const ScriptVector3& direction,
			float maxDistance,
			unsigned int layerMask,
			bool includeTriggers)
		{
			ModulePhysics3D* physics = GetPhysics();
			return MakeHitArray(
				physics
					? physics->RaycastAll(
						ToEngineVector(origin),
						ToEngineVector(direction),
						maxDistance,
						MakeFilter(
							layerMask,
							includeTriggers))
					: std::vector<Physics::QueryHit>{});
		}

		CScriptArray* ScriptOverlapSphere(
			const ScriptVector3& center,
			float radius,
			unsigned int layerMask,
			bool includeTriggers)
		{
			ModulePhysics3D* physics = GetPhysics();
			return MakeHitArray(
				physics
					? physics->OverlapSphere(
						ToEngineVector(center),
						radius,
						MakeFilter(
							layerMask,
							includeTriggers))
					: std::vector<Physics::QueryHit>{});
		}
	}

	ScriptRaycastHit::ScriptRaycastHit(
		const Physics::QueryHit& hit)
		: point_{
			hit.point.x,
			hit.point.y,
			hit.point.z},
		  normal_{
			hit.normal.x,
			hit.normal.y,
			hit.normal.z},
		  distance_(hit.distance),
		  fraction_(hit.fraction),
		  isTrigger_(hit.isTrigger)
	{
		GameObject* owner = hit.rigidBody
			? hit.rigidBody->GetGameObject()
			: nullptr;
		objectId_ = owner ? owner->GetUID() : 0;
		rigidBodyId_ = hit.rigidBody
			? hit.rigidBody->GetUID()
			: 0;
		colliderId_ = hit.collider
			? hit.collider->GetUID()
			: 0;
		layer_ = hit.rigidBody
			? hit.rigidBody->GetCollisionLayer()
			: 0;
	}

	void ScriptRaycastHit::AddRef()
	{
		referenceCount_.fetch_add(1, std::memory_order_relaxed);
	}

	void ScriptRaycastHit::Release()
	{
		if (referenceCount_.fetch_sub(
				1, std::memory_order_acq_rel) == 1)
		{
			delete this;
		}
	}

	ScriptGameObjectReference*
	ScriptRaycastHit::GetGameObject() const
	{
		return MakeGameObjectReference(objectId_);
	}

	ScriptComponentReference*
	ScriptRaycastHit::GetRigidBody() const
	{
		return MakeComponentReference(
			objectId_, rigidBodyId_);
	}

	ScriptComponentReference*
	ScriptRaycastHit::GetCollider() const
	{
		return MakeComponentReference(
			objectId_, colliderId_);
	}

	ScriptVector3 ScriptRaycastHit::GetPoint() const
	{
		return point_;
	}

	ScriptVector3 ScriptRaycastHit::GetNormal() const
	{
		return normal_;
	}

	float ScriptRaycastHit::GetDistance() const
	{
		return distance_;
	}

	float ScriptRaycastHit::GetFraction() const
	{
		return fraction_;
	}

	std::uint32_t ScriptRaycastHit::GetLayer() const
	{
		return layer_;
	}

	bool ScriptRaycastHit::GetIsTrigger() const
	{
		return isTrigger_;
	}

	bool RegisterPhysicsQueryApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		const bool hitRegistered =
			engine.RegisterObjectType(
				"RaycastHit", 0, asOBJ_REF) >= 0 &&
			engine.RegisterObjectBehaviour(
				"RaycastHit",
				asBEHAVE_ADDREF,
				"void f()",
				asMETHOD(ScriptRaycastHit, AddRef),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectBehaviour(
				"RaycastHit",
				asBEHAVE_RELEASE,
				"void f()",
				asMETHOD(ScriptRaycastHit, Release),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"GameObject@ get_gameObject() const property",
				asMETHOD(ScriptRaycastHit, GetGameObject),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"RigidBody@ get_rigidBody() const property",
				asMETHOD(ScriptRaycastHit, GetRigidBody),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"Collider@ get_collider() const property",
				asMETHOD(ScriptRaycastHit, GetCollider),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"Vector3 get_point() const property",
				asMETHOD(ScriptRaycastHit, GetPoint),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"Vector3 get_normal() const property",
				asMETHOD(ScriptRaycastHit, GetNormal),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"float get_distance() const property",
				asMETHOD(ScriptRaycastHit, GetDistance),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"float get_fraction() const property",
				asMETHOD(ScriptRaycastHit, GetFraction),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"uint get_layer() const property",
				asMETHOD(ScriptRaycastHit, GetLayer),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"RaycastHit",
				"bool get_isTrigger() const property",
				asMETHOD(ScriptRaycastHit, GetIsTrigger),
				asCALL_THISCALL) >= 0;
		if (!hitRegistered)
		{
			error = "Could not register the RaycastHit API.";
			return false;
		}

		engine.SetDefaultNamespace("Physics");
		const bool queriesRegistered =
			engine.RegisterGlobalFunction(
				"bool Raycast("
				"const Vector3 &in origin, "
				"const Vector3 &in direction, "
				"RaycastHit@&out hit, "
				"float maxDistance = 1000.0f, "
				"uint layerMask = 65535, "
				"bool includeTriggers = true)",
				asFUNCTION(ScriptRaycast),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"array<RaycastHit@>@ RaycastAll("
				"const Vector3 &in origin, "
				"const Vector3 &in direction, "
				"float maxDistance = 1000.0f, "
				"uint layerMask = 65535, "
				"bool includeTriggers = true)",
				asFUNCTION(ScriptRaycastAll),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"bool SphereCast("
				"const Vector3 &in origin, "
				"float radius, "
				"const Vector3 &in direction, "
				"RaycastHit@&out hit, "
				"float maxDistance = 1000.0f, "
				"uint layerMask = 65535, "
				"bool includeTriggers = true)",
				asFUNCTION(ScriptSphereCast),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"array<RaycastHit@>@ OverlapSphere("
				"const Vector3 &in center, "
				"float radius, "
				"uint layerMask = 65535, "
				"bool includeTriggers = true)",
				asFUNCTION(ScriptOverlapSphere),
				asCALL_CDECL) >= 0;
		engine.SetDefaultNamespace("");
		if (queriesRegistered)
			return true;

		error = "Could not register the Physics query API.";
		return false;
	}
}
