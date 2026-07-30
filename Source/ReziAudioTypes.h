#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Math.h"

#include <cstdint>
#include <array>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace EGE::ReziAudio
{
	using AudioAssetId = unsigned long long;
	using AudioParameterId = std::uint32_t;

	struct AudioClipReference
	{
		AudioAssetId assetId = 0;
		std::string resolvedSource;

		[[nodiscard]] bool IsValid() const
		{
			return assetId != 0 || !resolvedSource.empty();
		}

		friend bool operator==(
			const AudioClipReference& left,
			const AudioClipReference& right) = default;
	};

	using FloatArray = std::vector<float>;
	using IntegerArray = std::vector<int>;
	using AudioClipArray = std::vector<AudioClipReference>;

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
		std::variant<
			bool,
			int,
			float,
			float2,
			float3,
			float4,
			std::string,
			AudioClipReference,
			FloatArray,
			IntegerArray,
			AudioClipArray>;

	enum class ParameterValueType : std::uint8_t
	{
		Bool,
		Integer,
		Float,
		Vector2,
		Vector3,
		Color,
		String,
		AudioClip,
		FloatArray,
		IntegerArray,
		AudioClipArray
	};

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
		AudioClip,
		FloatArray,
		IntegerArray,
		AudioClipArray
	};

	struct ParameterTypeDescriptor
	{
		ParameterValueType type = ParameterValueType::Float;
		std::string_view displayName;
		std::string_view defaultName;
		GraphPinType pinType = GraphPinType::Float;
		ParameterValue defaultValue = 0.0f;
		bool availableInAudioGraph = true;
	};

	[[nodiscard]] inline std::span<const ParameterTypeDescriptor>
	GetParameterTypeDescriptors()
	{
		static const std::array<ParameterTypeDescriptor, 11> descriptors = {{
			{ParameterValueType::Float, "Float", "Float",
			 GraphPinType::Float, 0.0f},
			{ParameterValueType::Integer, "Integer", "Int",
			 GraphPinType::Integer, 0},
			{ParameterValueType::Bool, "Bool", "Bool",
			 GraphPinType::Bool, false},
			{ParameterValueType::Vector2, "Vector 2", "Vector2",
			 GraphPinType::Vector2, float2::zero},
			{ParameterValueType::Vector3, "Vector 3", "Vector3",
			 GraphPinType::Vector3, float3::zero},
			{ParameterValueType::Color, "Color", "Color",
			 GraphPinType::Color, float4(1.0f, 1.0f, 1.0f, 1.0f), false},
			{ParameterValueType::String, "String", "String",
			 GraphPinType::String, std::string(), false},
			{ParameterValueType::AudioClip, "Audio Clip", "Clip",
			 GraphPinType::AudioClip, AudioClipReference{}},
			{ParameterValueType::FloatArray, "Float Array", "FloatArray",
			 GraphPinType::FloatArray, FloatArray{}},
			{ParameterValueType::IntegerArray, "Integer Array", "IntArray",
			 GraphPinType::IntegerArray, IntegerArray{}},
			{ParameterValueType::AudioClipArray, "Audio Clip Array", "ClipArray",
			 GraphPinType::AudioClipArray, AudioClipArray{}}
		}};
		return descriptors;
	}

	[[nodiscard]] inline ParameterValueType GetParameterValueType(
		const ParameterValue& value)
	{
		return static_cast<ParameterValueType>(value.index());
	}

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
