#pragma once

#include "ReziAudioBackend.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace EGE::ReziAudio
{
	enum class DspNodeType : std::uint8_t
	{
		WavePlayer,
		Gain,
		Pan,
		LowPass,
		HighPass,
		Delay,
		Reverb,
		Mixer,
		Output,
		SineOscillator,
		NoiseGenerator,
		ADEnvelope,
		AudioAdd,
		AudioSubtract,
		AudioMultiply,
		AudioMinimum,
		AudioMaximum,
		AudioClamp,
		AudioMapRange,
		RepeatTrigger,
		DelayedTrigger,
		TriggerCounter,
		AudioOffset,
		EventInput
	};

	struct DspNodeDescriptor
	{
		DspNodeType type = DspNodeType::Gain;
		std::string_view displayName;
		std::string_view category;
		std::uint8_t minimumInputs = 1;
		std::uint8_t maximumInputs = 1;
	};

	[[nodiscard]] inline std::span<const DspNodeDescriptor>
	GetDspNodeDescriptors()
	{
		static constexpr std::array<DspNodeDescriptor, 24> descriptors = {{
			{DspNodeType::EventInput, "Event Input", "Events", 0, 0},
			{DspNodeType::WavePlayer, "Wave Player", "Sources", 0, 0},
			{DspNodeType::SineOscillator, "Sine Oscillator", "Sources", 0, 0},
			{DspNodeType::NoiseGenerator, "Noise Generator", "Sources", 0, 0},
			{DspNodeType::ADEnvelope, "AD Envelope", "Sources", 0, 1},
			{DspNodeType::RepeatTrigger, "Repeat Trigger", "Trigger", 0, 0},
			{DspNodeType::DelayedTrigger, "Delayed Trigger", "Trigger", 0, 0},
			{DspNodeType::TriggerCounter, "Trigger Counter", "Trigger", 0, 1},
			{DspNodeType::Gain, "Gain", "Processing", 1, 1},
			{DspNodeType::Pan, "Stereo Pan", "Processing", 1, 1},
			{DspNodeType::LowPass, "Low Pass Filter", "Filters", 1, 1},
			{DspNodeType::HighPass, "High Pass Filter", "Filters", 1, 1},
			{DspNodeType::Delay, "Feedback Delay", "Effects", 1, 1},
			{DspNodeType::Reverb, "Reverb", "Effects", 1, 1},
			{DspNodeType::AudioAdd, "Add (Audio)", "Audio Math", 2, 2},
			{DspNodeType::AudioSubtract, "Subtract (Audio)", "Audio Math", 2, 2},
			{DspNodeType::AudioMultiply, "Multiply (Audio)", "Audio Math", 2, 2},
			{DspNodeType::AudioOffset, "Add (Float + Audio)", "Audio Math", 1, 1},
			{DspNodeType::AudioMinimum, "Minimum (Audio)", "Audio Math", 2, 2},
			{DspNodeType::AudioMaximum, "Maximum (Audio)", "Audio Math", 2, 2},
			{DspNodeType::AudioClamp, "Clamp (Audio)", "Audio Math", 1, 1},
			{DspNodeType::AudioMapRange, "Map Range (Audio)", "Audio Math", 1, 1},
			{DspNodeType::Mixer, "Mixer", "Routing", 1, 8},
			{DspNodeType::Output, "DSP Output", "Routing", 1, 1}
		}};
		return descriptors;
	}

	[[nodiscard]] inline const DspNodeDescriptor*
	FindDspNodeDescriptor(DspNodeType type)
	{
		const auto descriptors = GetDspNodeDescriptors();
		const auto found = std::find_if(
			descriptors.begin(),
			descriptors.end(),
			[type](const DspNodeDescriptor& descriptor)
			{
				return descriptor.type == type;
			});
		return found == descriptors.end() ? nullptr : &*found;
	}

	enum class DspParameterEditor : std::uint8_t
	{
		Scalar,
		Toggle,
		Integer,
		Choice
	};

	struct DspParameterDescriptor
	{
		std::string_view name;
		float defaultValue = 0.0f;
		float minimum = 0.0f;
		float maximum = 1.0f;
		DspParameterEditor editor = DspParameterEditor::Scalar;
		bool logarithmic = false;
		bool runtimeMutable = true;
		std::string_view maximumParameter;
		std::string_view choices;
	};

	[[nodiscard]] inline std::span<const DspParameterDescriptor>
	GetDspParameterDescriptors(DspNodeType type)
	{
		static constexpr std::array<DspParameterDescriptor, 3> wave = {{
			{"Loop", 1.0f, 0.0f, 1.0f, DspParameterEditor::Toggle},
			{"StartTime", 0.0f, 0.0f, 60.0f,
			 DspParameterEditor::Scalar, false, false},
			{"LoopCount", -1.0f, -1.0f, 64.0f,
			 DspParameterEditor::Integer}
		}};
		static constexpr std::array<DspParameterDescriptor, 1> gain = {{
			{"Gain", 1.0f, 0.0f, 2.0f}
		}};
		static constexpr std::array<DspParameterDescriptor, 1> pan = {{
			{"Pan", 0.0f, -1.0f, 1.0f}
		}};
		static constexpr std::array<DspParameterDescriptor, 1> lowPass = {{
			{"Cutoff", 12000.0f, 20.0f, 20000.0f,
			 DspParameterEditor::Scalar, true}
		}};
		static constexpr std::array<DspParameterDescriptor, 1> highPass = {{
			{"Cutoff", 80.0f, 20.0f, 20000.0f,
			 DspParameterEditor::Scalar, true}
		}};
		static constexpr std::array<DspParameterDescriptor, 4> delay = {{
			{"Time", 0.22f, 0.01f, 2.0f,
			 DspParameterEditor::Scalar, false, true, "MaxDelay"},
			{"Feedback", 0.3f, -0.95f, 0.95f},
			{"Mix", 0.22f, 0.0f, 1.0f},
			{"MaxDelay", 2.0f, 0.01f, 10.0f,
			 DspParameterEditor::Scalar, false, false}
		}};
		static constexpr std::array<DspParameterDescriptor, 6> reverb = {{
			{"PreDelay", 25.0f, 0.0f, 500.0f},
			{"RoomSize", 0.65f, 0.0f, 0.98f},
			{"Damping", 0.35f, 0.0f, 0.98f},
			{"Width", 1.0f, 0.0f, 1.0f},
			{"Wet", 0.25f, 0.0f, 1.0f},
			{"Dry", 0.85f, 0.0f, 1.0f}
		}};
		static constexpr std::array<DspParameterDescriptor, 3> sine = {{
			{"Frequency", 440.0f, 0.0f, 22000.0f,
			 DspParameterEditor::Scalar, true},
			{"PhaseOffset", 0.0f, 0.0f, 6.283185307f},
			{"ResetPhase", 0.0f, 0.0f, 1.0f,
			 DspParameterEditor::Toggle}
		}};
		static constexpr std::array<DspParameterDescriptor, 2> noise = {{
			{"Seed", -1.0f, -1.0f, 100000.0f,
			 DspParameterEditor::Integer},
			{"Type", 0.0f, 0.0f, 2.0f,
			 DspParameterEditor::Choice, false, true, {},
			 "White|Pink|Brownian"}
		}};
		static constexpr std::array<DspParameterDescriptor, 6> envelope = {{
			{"Attack", 0.01f, 0.0f, 10.0f},
			{"Decay", 0.25f, 0.0f, 10.0f},
			{"AttackCurve", 1.0f, 0.01f, 8.0f},
			{"DecayCurve", 1.0f, 0.01f, 8.0f},
			{"Loop", 0.0f, 0.0f, 1.0f, DspParameterEditor::Toggle},
			{"Trigger", 1.0f, 0.0f, 1.0f, DspParameterEditor::Toggle}
		}};
		static constexpr std::array<DspParameterDescriptor, 2> clamp = {{
			{"Minimum", -1.0f, -4.0f, 4.0f},
			{"Maximum", 1.0f, -4.0f, 4.0f}
		}};
		static constexpr std::array<DspParameterDescriptor, 1> offset = {{
			{"Value", 0.0f, -4.0f, 4.0f}
		}};
		static constexpr std::array<DspParameterDescriptor, 5> mapRange = {{
			{"InputMinimum", -1.0f, -4.0f, 4.0f},
			{"InputMaximum", 1.0f, -4.0f, 4.0f},
			{"OutputMinimum", -1.0f, -4.0f, 4.0f},
			{"OutputMaximum", 1.0f, -4.0f, 4.0f},
			{"Clamped", 1.0f, 0.0f, 1.0f, DspParameterEditor::Toggle}
		}};
		static constexpr std::array<DspParameterDescriptor, 2> repeatTrigger = {{
			{"Period", 0.5f, 0.0001f, 60.0f,
			 DspParameterEditor::Scalar, true},
			{"Running", 1.0f, 0.0f, 1.0f,
			 DspParameterEditor::Toggle}
		}};
		static constexpr std::array<DspParameterDescriptor, 3> delayedTrigger = {{
			{"Delay", 0.25f, 0.0f, 60.0f},
			{"Trigger", 1.0f, 0.0f, 1.0f,
			 DspParameterEditor::Toggle},
			{"Reset", 0.0f, 0.0f, 1.0f,
			 DspParameterEditor::Toggle}
		}};
		static constexpr std::array<DspParameterDescriptor, 5> triggerCounter = {{
			{"StartValue", 0.0f, -100000.0f, 100000.0f},
			{"StepSize", 1.0f, -100000.0f, 100000.0f},
			{"ResetCount", 0.0f, 0.0f, 100000.0f,
			 DspParameterEditor::Integer},
			{"Trigger", 0.0f, 0.0f, 1.0f,
			 DspParameterEditor::Toggle},
			{"Reset", 0.0f, 0.0f, 1.0f,
			 DspParameterEditor::Toggle}
		}};
		static constexpr std::array<DspParameterDescriptor, 0> none = {};
		switch (type)
		{
		case DspNodeType::WavePlayer: return wave;
		case DspNodeType::Gain:
		case DspNodeType::Mixer: return gain;
		case DspNodeType::Pan: return pan;
		case DspNodeType::LowPass: return lowPass;
		case DspNodeType::HighPass: return highPass;
		case DspNodeType::Delay: return delay;
		case DspNodeType::Reverb: return reverb;
		case DspNodeType::SineOscillator: return sine;
		case DspNodeType::NoiseGenerator: return noise;
		case DspNodeType::ADEnvelope: return envelope;
		case DspNodeType::RepeatTrigger: return repeatTrigger;
		case DspNodeType::DelayedTrigger: return delayedTrigger;
		case DspNodeType::TriggerCounter: return triggerCounter;
		case DspNodeType::EventInput: return none;
		case DspNodeType::AudioClamp: return clamp;
		case DspNodeType::AudioMapRange: return mapRange;
		case DspNodeType::AudioOffset: return offset;
		case DspNodeType::AudioAdd:
		case DspNodeType::AudioSubtract:
		case DspNodeType::AudioMultiply:
		case DspNodeType::AudioMinimum:
		case DspNodeType::AudioMaximum:
		case DspNodeType::Output: return none;
		}
		return none;
	}

	[[nodiscard]] inline std::map<std::string, float>
	CreateDspParameterDefaults(DspNodeType type)
	{
		std::map<std::string, float> values;
		for (const DspParameterDescriptor& descriptor :
			GetDspParameterDescriptors(type))
		{
			values.emplace(
				std::string(descriptor.name),
				descriptor.defaultValue);
		}
		return values;
	}

	struct DspNodeAsset
	{
		std::uint64_t id = 0;
		DspNodeType type = DspNodeType::Gain;
		std::string name;
		AudioClipReference clip;
		std::vector<std::uint64_t> inputs;
		std::map<std::string, float> parameters;
		float2 editorPosition = float2::zero;
		std::string eventName;
	};

	struct DspGraphAsset
	{
		std::string name;
		std::uint32_t sampleRate = 48000;
		std::uint32_t channels = 2;
		std::uint32_t blockFrames = 512;
		std::vector<DspNodeAsset> nodes;
		std::uint64_t outputNode = 0;
	};

	enum class DspDiagnosticSeverity : std::uint8_t
	{
		Warning,
		Error
	};

	struct DspDiagnostic
	{
		DspDiagnosticSeverity severity = DspDiagnosticSeverity::Error;
		std::uint64_t nodeId = 0;
		std::string message;
	};

	struct DspRealtimeStats
	{
		std::uint64_t processedBlocks = 0;
		std::uint64_t processedFrames = 0;
		std::uint64_t droppedParameterCommands = 0;
		float outputPeak = 0.0f;
	};

	class DspGraphStream final : public IAudioStream
	{
	public:
		~DspGraphStream() override;

		DspGraphStream(const DspGraphStream&) = delete;
		DspGraphStream& operator=(const DspGraphStream&) = delete;

		[[nodiscard]] static std::shared_ptr<DspGraphStream> Compile(
			const DspGraphAsset& asset,
			std::vector<DspDiagnostic>& diagnostics);

		[[nodiscard]] std::uint32_t GetChannels() const override;
		[[nodiscard]] std::uint32_t GetSampleRate() const override;
		[[nodiscard]] std::uint64_t GetCursorFrames() const override;
		[[nodiscard]] std::uint64_t GetLengthFrames() const override;
		std::uint64_t ReadFrames(
			float* interleavedOutput,
			std::uint64_t frameCount) noexcept override;
		bool SeekFrame(std::uint64_t frame) noexcept override;
		void SetLooping(bool looping) noexcept override;

		bool SetParameter(
			std::uint64_t nodeId,
			std::string_view parameter,
			float value) noexcept;
		bool TriggerEvent(std::string_view eventName) noexcept;
		[[nodiscard]] bool HasEvent(
			std::string_view eventName) const noexcept;
		[[nodiscard]] DspRealtimeStats GetStats() const noexcept;
		[[nodiscard]] const DspGraphAsset& GetAsset() const;

	private:
		struct Impl;
		explicit DspGraphStream(std::unique_ptr<Impl> implementation);

		std::unique_ptr<Impl> implementation_;
	};

	[[nodiscard]] DspGraphAsset CreateDefaultDspGraph(
		const AudioClipReference& clip);
	[[nodiscard]] DspGraphAsset CreateDefaultDspGraph(
		const std::string& clipPath);
}
