#pragma once

#include <atomic>
#include <cstdint>

class Component;
class GameObject;

namespace EGE
{
	class ScriptGameObjectReference final
	{
	public:
		explicit ScriptGameObjectReference(std::uint32_t objectId)
			: objectId_(objectId)
		{
		}

		void AddRef()
		{
			referenceCount_.fetch_add(1, std::memory_order_relaxed);
		}

		void Release()
		{
			if (referenceCount_.fetch_sub(
					1, std::memory_order_acq_rel) == 1)
			{
				delete this;
			}
		}

		[[nodiscard]] std::uint32_t GetObjectId() const
		{
			return objectId_;
		}
		[[nodiscard]] GameObject* Resolve() const;
		[[nodiscard]] bool IsValid() const;

	private:
		std::atomic_uint referenceCount_{1};
		std::uint32_t objectId_ = 0;
	};

	class ScriptComponentReference final
	{
	public:
		ScriptComponentReference(
			std::uint32_t objectId,
			std::uint32_t componentId)
			: objectId_(objectId),
			  componentId_(componentId)
		{
		}

		void AddRef()
		{
			referenceCount_.fetch_add(1, std::memory_order_relaxed);
		}

		void Release()
		{
			if (referenceCount_.fetch_sub(
					1, std::memory_order_acq_rel) == 1)
			{
				delete this;
			}
		}

		[[nodiscard]] std::uint32_t GetObjectId() const
		{
			return objectId_;
		}

		[[nodiscard]] std::uint32_t GetComponentId() const
		{
			return componentId_;
		}
		[[nodiscard]] Component* Resolve() const;
		[[nodiscard]] bool IsValid() const;

	private:
		std::atomic_uint referenceCount_{1};
		std::uint32_t objectId_ = 0;
		std::uint32_t componentId_ = 0;
	};

	[[nodiscard]] inline ScriptGameObjectReference*
	MakeGameObjectReference(std::uint32_t objectId)
	{
		return objectId != 0
			? new ScriptGameObjectReference(objectId)
			: nullptr;
	}

	[[nodiscard]] inline ScriptComponentReference*
	MakeComponentReference(
		std::uint32_t objectId,
		std::uint32_t componentId)
	{
		return objectId != 0 && componentId != 0
			? new ScriptComponentReference(objectId, componentId)
			: nullptr;
	}
}
