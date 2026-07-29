#pragma once

#include "ScriptApiRegistry.h"

#include "../PhysicsCollision.h"
#include "ScriptMath.h"

#include <atomic>

class asIScriptEngine;

namespace EGE
{
	class ScriptGameObjectReference;
	class ScriptComponentReference;

	class ScriptCollisionInfo final
	{
	public:
		explicit ScriptCollisionInfo(
			const Physics::CollisionInfo& info);

		void AddRef();
		void Release();

		[[nodiscard]] ScriptGameObjectReference*
			GetGameObject() const;
		[[nodiscard]] ScriptComponentReference*
			GetSelfCollider() const;
		[[nodiscard]] ScriptComponentReference*
			GetCollider() const;
		[[nodiscard]] std::uint32_t GetOtherObjectId() const;
		[[nodiscard]] std::uint32_t GetSelfColliderId() const;
		[[nodiscard]] std::uint32_t GetOtherColliderId() const;
		[[nodiscard]] std::uint32_t GetOtherLayer() const;
		[[nodiscard]] ScriptVector3 GetPoint() const;
		[[nodiscard]] ScriptVector3 GetNormal() const;
		[[nodiscard]] float GetSeparation() const;
		[[nodiscard]] float GetImpulse() const;
		[[nodiscard]] bool GetIsTrigger() const;

	private:
		std::atomic_uint referenceCount_{1};
		Physics::CollisionInfo info_;
	};

	[[nodiscard]] bool RegisterPhysicsApi(
		asIScriptEngine& engine,
		std::string& error);
}
