#pragma once

#include "ReziAudioTypes.h"

#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace EGE::ReziAudio
{
	enum class GraphDiagnosticSeverity : std::uint8_t
	{
		Info,
		Warning,
		Error
	};

	struct GraphDiagnostic
	{
		GraphDiagnosticSeverity severity = GraphDiagnosticSeverity::Error;
		std::uint64_t nodeId = 0;
		std::uint64_t pinId = 0;
		std::string message;
	};

	struct NodePinDefinition
	{
		std::string name;
		GraphPinType type = GraphPinType::Float;
		ParameterValue defaultValue = 0.0f;
	};

	struct NodeDescriptor
	{
		std::string type;
		std::string displayName;
		std::string category;
		std::vector<NodePinDefinition> inputs;
		std::vector<NodePinDefinition> outputs;
		std::map<std::string, ParameterValue> defaultProperties;
	};

	class NodeRegistry final
	{
	public:
		NodeRegistry();

		[[nodiscard]] const NodeDescriptor* Find(
			std::string_view type) const;
		[[nodiscard]] const std::vector<NodeDescriptor>& Descriptors() const;
		[[nodiscard]] GraphNode CreateNode(
			std::string_view type,
			std::uint64_t nodeId,
			std::uint64_t& nextPinId) const;

	private:
		std::vector<NodeDescriptor> descriptors_;
	};

	struct CompiledSoundGraph
	{
		SoundGraphAsset asset;
		std::vector<std::uint64_t> evaluationOrder;
		std::unordered_map<std::uint64_t, std::uint64_t> inputSources;
		std::vector<GraphDiagnostic> diagnostics;

		[[nodiscard]] bool IsValid() const;
	};

	class SoundGraphCompiler final
	{
	public:
		[[nodiscard]] static CompiledSoundGraph Compile(
			const SoundGraphAsset& asset,
			const NodeRegistry& registry);
	};

	class RuntimeParameterSet final
	{
	public:
		void Reset(const std::vector<NamedParameter>& parameters);
		bool Set(std::string_view name, const ParameterValue& value);
		bool Set(AudioParameterId id, const ParameterValue& value);
		[[nodiscard]] const ParameterValue* Find(std::string_view name) const;
		[[nodiscard]] const ParameterValue* Find(AudioParameterId id) const;
		[[nodiscard]] const std::vector<NamedParameter>& Definitions() const;

	private:
		std::vector<NamedParameter> definitions_;
		std::unordered_map<AudioParameterId, ParameterValue> values_;
		std::unordered_map<std::string, AudioParameterId> names_;
	};

	struct SoundGraphEvaluation
	{
		bool succeeded = false;
		VoiceCreateInfo voice;
		std::vector<GraphDiagnostic> diagnostics;
	};

	class SoundGraphInstance final
	{
	public:
		bool Load(const SoundGraphAsset& asset, const NodeRegistry& registry);
		[[nodiscard]] bool IsValid() const;

		bool SetParameter(std::string_view name, const ParameterValue& value);
		bool SetParameter(AudioParameterId id, const ParameterValue& value);
		[[nodiscard]] const ParameterValue* GetParameter(
			std::string_view name) const;
		[[nodiscard]] RuntimeParameterSet& Parameters();
		[[nodiscard]] const RuntimeParameterSet& Parameters() const;
		[[nodiscard]] const CompiledSoundGraph& Prototype() const;

		[[nodiscard]] SoundGraphEvaluation Evaluate();

	private:
		CompiledSoundGraph prototype_;
		RuntimeParameterSet parameters_;
		std::mt19937 random_{0xE6E2026u};
	};

	[[nodiscard]] AudioParameterId HashAudioParameter(std::string_view name);
	[[nodiscard]] SoundGraphAsset CreateDefaultSoundGraph(
		const NodeRegistry& registry,
		const std::string& clipPath);
}
