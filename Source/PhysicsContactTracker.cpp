#include "Globals.h"
#include "PhysicsContactTracker.h"

#include <algorithm>
#include <tuple>

namespace EGE::Physics
{
	ContactKey ContactKey::Make(
		std::uint32_t firstColliderId,
		std::uint32_t secondColliderId,
		ContactKind kind)
	{
		if (firstColliderId <= secondColliderId)
			return {firstColliderId, secondColliderId, kind};
		return {secondColliderId, firstColliderId, kind};
	}

	bool ContactKey::operator<(
		const ContactKey& other) const
	{
		return std::tie(
				firstColliderId,
				secondColliderId,
				kind) <
			std::tie(
				other.firstColliderId,
				other.secondColliderId,
				other.kind);
	}

	std::vector<ContactEvent> ContactTracker::Update(
		const std::vector<ContactObservation>& observations)
	{
		ContactMap currentContacts;
		for (const ContactObservation& observation : observations)
		{
			auto [iterator, inserted] =
				currentContacts.emplace(
					observation.key,
					observation);
			if (inserted)
				continue;

			ContactObservation& existing = iterator->second;
			if (observation.first.separation <
				existing.first.separation)
			{
				existing.first.point = observation.first.point;
				existing.first.normal = observation.first.normal;
				existing.first.separation =
					observation.first.separation;
				existing.second.point = observation.second.point;
				existing.second.normal = observation.second.normal;
				existing.second.separation =
					observation.second.separation;
			}
			existing.first.impulse += observation.first.impulse;
			existing.second.impulse += observation.second.impulse;
		}

		std::vector<ContactEvent> events;
		events.reserve(
			currentContacts.size() * 2 +
			activeContacts_.size() * 2);

		for (const auto& [key, observation] : currentContacts)
		{
			const ContactPhase phase =
				activeContacts_.contains(key)
					? ContactPhase::Stay
					: ContactPhase::Enter;
			events.push_back({phase, observation.first});
			events.push_back({phase, observation.second});
		}

		for (const auto& [key, observation] : activeContacts_)
		{
			if (currentContacts.contains(key))
				continue;
			events.push_back(
				{ContactPhase::Exit, observation.first});
			events.push_back(
				{ContactPhase::Exit, observation.second});
		}

		activeContacts_ = std::move(currentContacts);
		return events;
	}

	void ContactTracker::Clear()
	{
		activeContacts_.clear();
	}

	std::size_t ContactTracker::GetActiveContactCount() const
	{
		return activeContacts_.size();
	}
}
