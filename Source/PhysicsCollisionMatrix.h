#pragma once

#include "PhysicsCollision.h"

#include <array>
#include <cstdint>

namespace EGE::Physics
{
	class CollisionMatrix final
	{
	public:
		CollisionMatrix();

		[[nodiscard]] bool CanCollide(
			std::uint32_t firstLayer,
			std::uint32_t secondLayer) const;
		void SetCanCollide(
			std::uint32_t firstLayer,
			std::uint32_t secondLayer,
			bool enabled);

		[[nodiscard]] std::uint32_t GetMask(
			std::uint32_t layer) const;
		void SetRows(
			const std::array<std::uint32_t, CollisionLayerCount>& rows);
		[[nodiscard]] const std::array<
			std::uint32_t,
			CollisionLayerCount>& GetRows() const;

	private:
		void Normalize();

		std::array<std::uint32_t, CollisionLayerCount> rows_{};
	};
}
