#include "Globals.h"
#include "PhysicsCollisionMatrix.h"

namespace EGE::Physics
{
	CollisionMatrix::CollisionMatrix()
	{
		rows_.fill(AllCollisionLayers);
	}

	bool CollisionMatrix::CanCollide(
		std::uint32_t firstLayer,
		std::uint32_t secondLayer) const
	{
		if (firstLayer >= CollisionLayerCount ||
			secondLayer >= CollisionLayerCount)
		{
			return false;
		}
		return (rows_[firstLayer] & (1u << secondLayer)) != 0;
	}

	void CollisionMatrix::SetCanCollide(
		std::uint32_t firstLayer,
		std::uint32_t secondLayer,
		bool enabled)
	{
		if (firstLayer >= CollisionLayerCount ||
			secondLayer >= CollisionLayerCount)
		{
			return;
		}

		const std::uint32_t firstBit = 1u << secondLayer;
		const std::uint32_t secondBit = 1u << firstLayer;
		if (enabled)
		{
			rows_[firstLayer] |= firstBit;
			rows_[secondLayer] |= secondBit;
		}
		else
		{
			rows_[firstLayer] &= ~firstBit;
			rows_[secondLayer] &= ~secondBit;
		}
	}

	std::uint32_t CollisionMatrix::GetMask(
		std::uint32_t layer) const
	{
		return layer < CollisionLayerCount
			? rows_[layer]
			: 0;
	}

	void CollisionMatrix::SetRows(
		const std::array<
			std::uint32_t,
			CollisionLayerCount>& rows)
	{
		rows_ = rows;
		Normalize();
	}

	const std::array<
		std::uint32_t,
		CollisionLayerCount>& CollisionMatrix::GetRows() const
	{
		return rows_;
	}

	void CollisionMatrix::Normalize()
	{
		for (std::uint32_t first = 0;
			first < CollisionLayerCount;
			++first)
		{
			rows_[first] &= AllCollisionLayers;
			for (std::uint32_t second = first;
				second < CollisionLayerCount;
				++second)
			{
				const bool enabled =
					(rows_[first] & (1u << second)) != 0 &&
					(rows_[second] & (1u << first)) != 0;
				SetCanCollide(first, second, enabled);
			}
		}
	}
}
