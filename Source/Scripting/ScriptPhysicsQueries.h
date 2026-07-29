#pragma once

#include "../PhysicsQuery.h"
#include "ScriptApiRegistry.h"
#include "ScriptMath.h"

#include <atomic>
#include <cstdint>

class asIScriptEngine;

namespace EGE
{
	class ScriptComponentReference;
	class ScriptGameObjectReference;

	class ScriptRaycastHit final
	{
	public:
		explicit ScriptRaycastHit(
			const Physics::QueryHit& hit);

		void AddRef();
		void Release();

		[[nodiscard]] ScriptGameObjectReference*
			GetGameObject() const;
		[[nodiscard]] ScriptComponentReference*
			GetRigidBody() const;
		[[nodiscard]] ScriptComponentReference*
			GetCollider() const;
		[[nodiscard]] ScriptVector3 GetPoint() const;
		[[nodiscard]] ScriptVector3 GetNormal() const;
		[[nodiscard]] float GetDistance() const;
		[[nodiscard]] float GetFraction() const;
		[[nodiscard]] std::uint32_t GetLayer() const;
		[[nodiscard]] bool GetIsTrigger() const;

	private:
		std::atomic_uint referenceCount_{1};
		std::uint32_t objectId_ = 0;
		std::uint32_t rigidBodyId_ = 0;
		std::uint32_t colliderId_ = 0;
		std::uint32_t layer_ = 0;
		ScriptVector3 point_;
		ScriptVector3 normal_;
		float distance_ = 0.0f;
		float fraction_ = 0.0f;
		bool isTrigger_ = false;
	};

	[[nodiscard]] bool RegisterPhysicsQueryApi(
		asIScriptEngine& engine,
		std::string& error);
}
