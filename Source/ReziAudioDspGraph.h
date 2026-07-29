#pragma once

#include "ReziAudioBackend.h"

#include <cstdint>
#include <map>
#include <memory>
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
		Output
	};

	struct DspNodeAsset
	{
		std::uint64_t id = 0;
		DspNodeType type = DspNodeType::Gain;
		std::string name;
		std::string clipPath;
		std::vector<std::uint64_t> inputs;
		std::map<std::string, float> parameters;
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
		[[nodiscard]] DspRealtimeStats GetStats() const noexcept;
		[[nodiscard]] const DspGraphAsset& GetAsset() const;

	private:
		struct Impl;
		explicit DspGraphStream(std::unique_ptr<Impl> implementation);

		std::unique_ptr<Impl> implementation_;
	};

	[[nodiscard]] DspGraphAsset CreateDefaultDspGraph(
		const std::string& clipPath);
}
