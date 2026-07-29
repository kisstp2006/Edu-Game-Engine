#include "Globals.h"
#include "PhysicsQuery.h"

#include "PhysicsCollisionShape.h"

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cmath>

namespace EGE::Physics
{
	namespace
	{
		bool IsFinite(const float3& value)
		{
			return std::isfinite(value.x) &&
				std::isfinite(value.y) &&
				std::isfinite(value.z);
		}

		bool IsValidCast(
			const float3& origin,
			const float3& direction,
			float maxDistance)
		{
			return IsFinite(origin) &&
				IsFinite(direction) &&
				direction.LengthSq() > 0.0000001f &&
				std::isfinite(maxDistance) &&
				maxDistance > 0.0f;
		}

		bool IsTrigger(const btCollisionObject& object)
		{
			return (object.getCollisionFlags() &
				btCollisionObject::CF_NO_CONTACT_RESPONSE) != 0;
		}

		int GetChildIndex(
			const btCollisionWorld::LocalShapeInfo* shapeInfo)
		{
			return shapeInfo ? shapeInfo->m_triangleIndex : -1;
		}

		bool AcceptObject(
			const btBroadphaseProxy& proxy,
			const QueryFilter& filter)
		{
			if ((static_cast<std::uint32_t>(
					proxy.m_collisionFilterGroup) &
					filter.layerMask &
					AllCollisionLayers) == 0)
			{
				return false;
			}

			const auto* object =
				static_cast<const btCollisionObject*>(
					proxy.m_clientObject);
			return object &&
				(filter.includeTriggers || !IsTrigger(*object));
		}

		QueryHit MakeHit(
			const btCollisionObject& object,
			int childIndex,
			const btVector3& point,
			const btVector3& normal,
			float distance,
			float fraction)
		{
			QueryHit hit;
			hit.rigidBody = GetComponentOwner(object);
			hit.collider = GetColliderOwner(object, childIndex);
			hit.point = float3(point);
			hit.normal = float3(normal).Normalized();
			hit.distance = distance;
			hit.fraction = fraction;
			hit.isTrigger = IsTrigger(object);
			return hit;
		}

		class RayQueryCallback final
			: public btCollisionWorld::RayResultCallback
		{
		public:
			RayQueryCallback(
				const btVector3& origin,
				const btVector3& target,
				float distance,
				QueryFilter filter,
				bool closestOnly)
				: origin_(origin),
				  target_(target),
				  distance_(distance),
				  filter_(filter),
				  closestOnly_(closestOnly)
			{
			}

			bool needsCollision(btBroadphaseProxy* proxy) const override
			{
				return proxy && AcceptObject(*proxy, filter_);
			}

			btScalar addSingleResult(
				btCollisionWorld::LocalRayResult& result,
				bool normalInWorldSpace) override
			{
				if (!result.m_collisionObject ||
					!GetComponentOwner(*result.m_collisionObject))
				{
					return m_closestHitFraction;
				}

				btVector3 normal = result.m_hitNormalLocal;
				if (!normalInWorldSpace)
				{
					normal =
						result.m_collisionObject
							->getWorldTransform()
							.getBasis() * normal;
				}
				const btVector3 point =
					origin_.lerp(target_, result.m_hitFraction);
				QueryHit hit = MakeHit(
					*result.m_collisionObject,
					GetChildIndex(result.m_localShapeInfo),
					point,
					normal,
					distance_ * result.m_hitFraction,
					result.m_hitFraction);
				if (!hit.collider)
					return m_closestHitFraction;

				if (closestOnly_)
				hits_.assign(1, hit);
				else
					hits_.push_back(hit);

				m_collisionObject = result.m_collisionObject;
				if (closestOnly_)
					m_closestHitFraction = result.m_hitFraction;
				return closestOnly_
					? result.m_hitFraction
					: btScalar(1.0f);
			}

			std::vector<QueryHit> TakeHits()
			{
				std::sort(
					hits_.begin(),
					hits_.end(),
					[](const QueryHit& left, const QueryHit& right)
					{
						return left.distance < right.distance;
					});
				return std::move(hits_);
			}

		private:
			btVector3 origin_;
			btVector3 target_;
			float distance_ = 0.0f;
			QueryFilter filter_;
			bool closestOnly_ = false;
			std::vector<QueryHit> hits_;
		};

		class SphereCastCallback final
			: public btCollisionWorld::ConvexResultCallback
		{
		public:
			SphereCastCallback(
				float distance,
				QueryFilter filter)
				: distance_(distance),
				  filter_(filter)
			{
			}

			bool needsCollision(btBroadphaseProxy* proxy) const override
			{
				return proxy && AcceptObject(*proxy, filter_);
			}

			btScalar addSingleResult(
				btCollisionWorld::LocalConvexResult& result,
				bool normalInWorldSpace) override
			{
				if (!result.m_hitCollisionObject ||
					!GetComponentOwner(
						*result.m_hitCollisionObject))
				{
					return m_closestHitFraction;
				}

				btVector3 normal = result.m_hitNormalLocal;
				if (!normalInWorldSpace)
				{
					normal =
						result.m_hitCollisionObject
							->getWorldTransform()
							.getBasis() * normal;
				}
				QueryHit candidate = MakeHit(
					*result.m_hitCollisionObject,
					GetChildIndex(result.m_localShapeInfo),
					result.m_hitPointLocal,
					normal,
					distance_ * result.m_hitFraction,
					result.m_hitFraction);
				if (!candidate.collider)
					return m_closestHitFraction;

				hit_ = candidate;
				hasHit_ = true;
				m_closestHitFraction = result.m_hitFraction;
				return result.m_hitFraction;
			}

			bool GetHit(QueryHit& hit) const
			{
				if (!hasHit_)
					return false;
				hit = hit_;
				return true;
			}

		private:
			float distance_ = 0.0f;
			QueryFilter filter_;
			QueryHit hit_;
			bool hasHit_ = false;
		};

		class OverlapSphereCallback final
			: public btCollisionWorld::ContactResultCallback
		{
		public:
			OverlapSphereCallback(
				const btCollisionObject& query,
				QueryFilter filter)
				: query_(query),
				  filter_(filter)
			{
			}

			bool needsCollision(btBroadphaseProxy* proxy) const override
			{
				return proxy && AcceptObject(*proxy, filter_);
			}

			btScalar addSingleResult(
				btManifoldPoint& point,
				const btCollisionObjectWrapper* objectA,
				int,
				int indexA,
				const btCollisionObjectWrapper* objectB,
				int,
				int indexB) override
			{
				const bool queryIsA =
					objectA &&
					objectA->getCollisionObject() == &query_;
				const btCollisionObjectWrapper* target =
					queryIsA ? objectB : objectA;
				const int childIndex = queryIsA ? indexB : indexA;
				if (!target)
					return 0.0f;

				const btCollisionObject* object =
					target->getCollisionObject();
				if (!object || !GetComponentOwner(*object))
					return 0.0f;

				const btVector3 hitPoint = queryIsA
					? point.getPositionWorldOnB()
					: point.getPositionWorldOnA();
				const btVector3 normal = queryIsA
					? -point.m_normalWorldOnB
					: point.m_normalWorldOnB;
				QueryHit hit = MakeHit(
					*object,
					childIndex,
					hitPoint,
					normal,
					0.0f,
					0.0f);
				if (!hit.collider)
					return 0.0f;

				const auto existing = std::find_if(
					hits_.begin(),
					hits_.end(),
					[&hit](const QueryHit& candidate)
					{
						return candidate.collider == hit.collider;
					});
				if (existing == hits_.end())
					hits_.push_back(hit);
				return 0.0f;
			}

			std::vector<QueryHit> TakeHits()
			{
				return std::move(hits_);
			}

		private:
			const btCollisionObject& query_;
			QueryFilter filter_;
			std::vector<QueryHit> hits_;
		};

		std::vector<QueryHit> CastRay(
			const btCollisionWorld& world,
			const float3& origin,
			const float3& direction,
			float maxDistance,
			const QueryFilter& filter,
			bool closestOnly)
		{
			if (!IsValidCast(origin, direction, maxDistance))
				return {};

			const float3 normalizedDirection =
				direction.Normalized();
			const btVector3 from(origin);
			const btVector3 to(
				origin + normalizedDirection * maxDistance);
			RayQueryCallback callback(
				from, to, maxDistance, filter, closestOnly);
			world.rayTest(from, to, callback);
			return callback.TakeHits();
		}
	}

	bool Raycast(
		const btCollisionWorld& world,
		const float3& origin,
		const float3& direction,
		float maxDistance,
		const QueryFilter& filter,
		QueryHit& hit)
	{
		std::vector<QueryHit> hits = CastRay(
			world,
			origin,
			direction,
			maxDistance,
			filter,
			true);
		if (hits.empty())
			return false;
		hit = hits.front();
		return true;
	}

	std::vector<QueryHit> RaycastAll(
		const btCollisionWorld& world,
		const float3& origin,
		const float3& direction,
		float maxDistance,
		const QueryFilter& filter)
	{
		return CastRay(
			world,
			origin,
			direction,
			maxDistance,
			filter,
			false);
	}

	bool SphereCast(
		const btCollisionWorld& world,
		const float3& origin,
		float radius,
		const float3& direction,
		float maxDistance,
		const QueryFilter& filter,
		QueryHit& hit)
	{
		if (!IsValidCast(origin, direction, maxDistance) ||
			!std::isfinite(radius) ||
			radius <= 0.0f)
		{
			return false;
		}

		btSphereShape sphere(radius);
		btTransform from = btTransform::getIdentity();
		btTransform to = btTransform::getIdentity();
		from.setOrigin(origin);
		to.setOrigin(
			origin + direction.Normalized() * maxDistance);
		SphereCastCallback callback(maxDistance, filter);
		world.convexSweepTest(
			&sphere, from, to, callback);
		return callback.GetHit(hit);
	}

	std::vector<QueryHit> OverlapSphere(
		btCollisionWorld& world,
		const float3& center,
		float radius,
		const QueryFilter& filter)
	{
		if (!IsFinite(center) ||
			!std::isfinite(radius) ||
			radius <= 0.0f)
		{
			return {};
		}

		btSphereShape shape(radius);
		btCollisionObject query;
		query.setCollisionShape(&shape);
		btTransform transform = btTransform::getIdentity();
		transform.setOrigin(center);
		query.setWorldTransform(transform);
		OverlapSphereCallback callback(query, filter);
		world.contactTest(&query, callback);
		return callback.TakeHits();
	}
}
