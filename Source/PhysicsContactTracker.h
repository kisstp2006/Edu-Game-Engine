#pragma once

#include "PhysicsCollision.h"

#include <cstdint>
#include <map>
#include <vector>

namespace EGE::Physics
{
	struct ContactKey
	{
		std::uint32_t firstColliderId = 0;
		std::uint32_t secondColliderId = 0;
		ContactKind kind = ContactKind::Collision;

		[[nodiscard]] static ContactKey Make(
			std::uint32_t firstColliderId,
			std::uint32_t secondColliderId,
			ContactKind kind);
		[[nodiscard]] bool operator<(
			const ContactKey& other) const;
	};

	struct ContactObservation
	{
		ContactKey key;
		CollisionInfo first;
		CollisionInfo second;
	};

	struct ContactEvent
	{
		ContactPhase phase = ContactPhase::Enter;
		CollisionInfo info;
	};

	class ContactTracker final
	{
	public:
		[[nodiscard]] std::vector<ContactEvent> Update(
			const std::vector<ContactObservation>& observations);
		void Clear();
		[[nodiscard]] std::size_t GetActiveContactCount() const;

	private:
		using ContactMap =
			std::map<ContactKey, ContactObservation>;

		ContactMap activeContacts_;
	};
}
