#include "../Globals.h"
#include "ScriptPhysics.h"

#include "ScriptObjectReference.h"

#include <angelscript.h>

namespace EGE
{
	ScriptCollisionInfo::ScriptCollisionInfo(
		const Physics::CollisionInfo& info)
		: info_(info)
	{
	}

	void ScriptCollisionInfo::AddRef()
	{
		referenceCount_.fetch_add(1, std::memory_order_relaxed);
	}

	void ScriptCollisionInfo::Release()
	{
		if (referenceCount_.fetch_sub(
				1, std::memory_order_acq_rel) == 1)
		{
			delete this;
		}
	}

	ScriptGameObjectReference*
	ScriptCollisionInfo::GetGameObject() const
	{
		return MakeGameObjectReference(info_.otherObjectId);
	}

	ScriptComponentReference*
	ScriptCollisionInfo::GetSelfCollider() const
	{
		return MakeComponentReference(
			info_.selfObjectId,
			info_.selfColliderId);
	}

	ScriptComponentReference*
	ScriptCollisionInfo::GetCollider() const
	{
		return MakeComponentReference(
			info_.otherObjectId,
			info_.otherColliderId);
	}

	std::uint32_t ScriptCollisionInfo::GetOtherObjectId() const
	{
		return info_.otherObjectId;
	}

	std::uint32_t ScriptCollisionInfo::GetSelfColliderId() const
	{
		return info_.selfColliderId;
	}

	std::uint32_t ScriptCollisionInfo::GetOtherColliderId() const
	{
		return info_.otherColliderId;
	}

	std::uint32_t ScriptCollisionInfo::GetOtherLayer() const
	{
		return info_.otherLayer;
	}

	ScriptVector3 ScriptCollisionInfo::GetPoint() const
	{
		return {
			info_.point.x,
			info_.point.y,
			info_.point.z};
	}

	ScriptVector3 ScriptCollisionInfo::GetNormal() const
	{
		return {
			info_.normal.x,
			info_.normal.y,
			info_.normal.z};
	}

	float ScriptCollisionInfo::GetSeparation() const
	{
		return info_.separation;
	}

	float ScriptCollisionInfo::GetImpulse() const
	{
		return info_.impulse;
	}

	bool ScriptCollisionInfo::GetIsTrigger() const
	{
		return info_.isTrigger;
	}

	bool RegisterPhysicsApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		const bool registered =
			engine.RegisterObjectType(
				"CollisionInfo", 0, asOBJ_REF) >= 0 &&
			engine.RegisterObjectBehaviour(
				"CollisionInfo",
				asBEHAVE_ADDREF,
				"void f()",
				asMETHOD(ScriptCollisionInfo, AddRef),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectBehaviour(
				"CollisionInfo",
				asBEHAVE_RELEASE,
				"void f()",
				asMETHOD(ScriptCollisionInfo, Release),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"GameObject@ get_gameObject() const property",
				asMETHOD(ScriptCollisionInfo, GetGameObject),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"Component@ get_selfCollider() const property",
				asMETHOD(ScriptCollisionInfo, GetSelfCollider),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"Component@ get_collider() const property",
				asMETHOD(ScriptCollisionInfo, GetCollider),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"uint get_otherObjectId() const property",
				asMETHOD(ScriptCollisionInfo, GetOtherObjectId),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"uint get_selfColliderId() const property",
				asMETHOD(ScriptCollisionInfo, GetSelfColliderId),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"uint get_otherColliderId() const property",
				asMETHOD(ScriptCollisionInfo, GetOtherColliderId),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"uint get_otherLayer() const property",
				asMETHOD(ScriptCollisionInfo, GetOtherLayer),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"Vector3 get_point() const property",
				asMETHOD(ScriptCollisionInfo, GetPoint),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"Vector3 get_normal() const property",
				asMETHOD(ScriptCollisionInfo, GetNormal),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"float get_separation() const property",
				asMETHOD(ScriptCollisionInfo, GetSeparation),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"float get_impulse() const property",
				asMETHOD(ScriptCollisionInfo, GetImpulse),
				asCALL_THISCALL) >= 0 &&
			engine.RegisterObjectMethod(
				"CollisionInfo",
				"bool get_isTrigger() const property",
				asMETHOD(ScriptCollisionInfo, GetIsTrigger),
				asCALL_THISCALL) >= 0;

		if (registered)
			return true;
		error = "Could not register the CollisionInfo API.";
		return false;
	}
}
