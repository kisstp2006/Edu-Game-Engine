#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Math.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace EGE::ReziAudio
{
	using AudioAssetId = unsigned long long;
	using AudioParameterId = std::uint32_t;

	enum class Bus : std::uint8_t
	{
		Master,
		Music,
		SoundEffects,
		Ambience,
		UserInterface
	};

	enum class AttenuationModel : std::uint8_t
	{
		None,
		Inverse,
		Linear,
		Exponential
	};

	enum class PlaybackState : std::uint8_t
	{
		Invalid,
		Stopped,
		Playing,
		Paused,
		Finished
	};

	struct PlaybackHandle
	{
		static constexpr std::uint32_t InvalidIndex =
			std::numeric_limits<std::uint32_t>::max();

		std::uint32_t index = InvalidIndex;
		std::uint32_t generation = 0;

		[[nodiscard]] bool IsValid() const
		{
			return index != InvalidIndex && generation != 0;
		}

		friend bool operator==(
			const PlaybackHandle& left,
			const PlaybackHandle& right) = default;
	};

	struct AudioTransform
	{
		float3 position = float3::zero;
		float3 forward = float3::unitZ;
		float3 up = float3::unitY;
		float3 velocity = float3::zero;
	};

	struct ConeSettings
	{
		float innerAngleDegrees = 360.0f;
		float outerAngleDegrees = 360.0f;
		float outerGain = 0.0f;
	};

	struct SpatialSettings
	{
		bool enabled = true;
		AttenuationModel attenuation = AttenuationModel::Inverse;
		float minDistance = 1.0f;
		float maxDistance = 100.0f;
		float minGain = 0.0f;
		float maxGain = 1.0f;
		float rolloff = 1.0f;
		float dopplerFactor = 1.0f;
		ConeSettings cone;
	};

	struct VoiceSettings
	{
		float volume = 1.0f;
		float pitch = 1.0f;
		float pan = 0.0f;
		bool looping = false;
		bool streaming = false;
		Bus bus = Bus::SoundEffects;
		SpatialSettings spatial;
	};

	struct VoiceCreateInfo
	{
		std::string filePath;
		VoiceSettings settings;
		AudioTransform transform;
	};

	using ParameterValue =
		std::variant<bool, int, float, float2, float3, float4, std::string>;

	struct NamedParameter
	{
		std::string name;
		AudioParameterId id = 0;
		ParameterValue defaultValue = 0.0f;
	};

	struct ClipAsset
	{
		AudioAssetId id = 0;
		std::string sourcePath;
		bool streaming = false;
		bool preload = false;
	};

	enum class GraphPinType : std::uint8_t
	{
		Flow,
		Bool,
		Integer,
		Float,
		Vector2,
		Vector3,
		Color,
		String,
		AudioBuffer,
		AudioClip
	};

	struct GraphPin
	{
		std::uint64_t id = 0;
		std::string name;
		GraphPinType type = GraphPinType::Float;
		ParameterValue defaultValue = 0.0f;
	};

	struct GraphNode
	{
		std::uint64_t id = 0;
		std::string type;
		std::string displayName;
		std::vector<GraphPin> inputs;
		std::vector<GraphPin> outputs;
		std::map<std::string, ParameterValue> properties;
		float2 editorPosition = float2::zero;
	};

	struct GraphLink
	{
		std::uint64_t id = 0;
		std::uint64_t outputPin = 0;
		std::uint64_t inputPin = 0;
	};

	struct SoundGraphAsset
	{
		AudioAssetId id = 0;
		std::string name;
		std::vector<NamedParameter> parameters;
		std::vector<GraphNode> nodes;
		std::vector<GraphLink> links;
	};
}
