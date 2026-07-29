#include "ReziAudioDspGraph.h"

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <queue>
#include <unordered_map>

namespace EGE::ReziAudio
{
	namespace
	{
		constexpr std::size_t MaxNodeInputs = 8;
		constexpr std::size_t MaxNodeParameters = 12;
		constexpr std::size_t CommandCapacity = 256;
		constexpr float Pi = 3.14159265358979323846f;

		std::uint32_t ParameterHash(std::string_view name)
		{
			std::uint32_t hash = 2166136261u;
			for (const unsigned char character : name)
			{
				hash ^= character;
				hash *= 16777619u;
			}
			return hash == 0 ? 1 : hash;
		}

		float Parameter(
			const DspNodeAsset& node,
			const char* name,
			float fallback)
		{
			const auto found = node.parameters.find(name);
			return found == node.parameters.end()
				? fallback
				: found->second;
		}

		std::size_t RequiredInputs(DspNodeType type)
		{
			switch (type)
			{
			case DspNodeType::WavePlayer: return 0;
			case DspNodeType::Mixer: return 1;
			default: return 1;
			}
		}

		bool DecodeClip(
			const std::string& path,
			std::uint32_t channels,
			std::uint32_t sampleRate,
			std::vector<float>& samples,
			std::uint64_t& frames)
		{
			ma_decoder decoder{};
			const ma_decoder_config config =
				ma_decoder_config_init(
					ma_format_f32, channels, sampleRate);
			if (ma_decoder_init_file(
					path.c_str(), &config, &decoder) != MA_SUCCESS)
			{
				return false;
			}

			ma_uint64 length = 0;
			if (ma_data_source_get_length_in_pcm_frames(
					&decoder, &length) != MA_SUCCESS ||
				length == 0 ||
				length > std::numeric_limits<std::size_t>::max() / channels)
			{
				ma_decoder_uninit(&decoder);
				return false;
			}
			samples.resize(
				static_cast<std::size_t>(length) * channels);
			ma_uint64 read = 0;
			const ma_result result = ma_decoder_read_pcm_frames(
				&decoder, samples.data(), length, &read);
			ma_decoder_uninit(&decoder);
			if (result != MA_SUCCESS && result != MA_AT_END)
				return false;
			frames = read;
			samples.resize(static_cast<std::size_t>(read) * channels);
			return read != 0;
		}

		struct ParameterSlot
		{
			std::uint32_t id = 0;
			float value = 0.0f;
		};

		struct DelayLine
		{
			std::vector<float> samples;
			std::size_t cursor = 0;
			float filterState = 0.0f;
		};

		struct RuntimeNode
		{
			std::uint64_t id = 0;
			DspNodeType type = DspNodeType::Gain;
			std::array<std::uint32_t, MaxNodeInputs> inputs{};
			std::uint32_t inputCount = 0;
			std::array<ParameterSlot, MaxNodeParameters> parameters{};
			std::uint32_t parameterCount = 0;
			std::vector<float> clipSamples;
			std::uint64_t clipFrames = 0;
			std::uint64_t clipCursor = 0;
			std::vector<float> delayBuffer;
			std::size_t delayCursor = 0;
			std::array<DelayLine, 12> reverbLines;
			std::array<float, 2> lowPassState{};
			std::array<float, 2> highPassInput{};
			std::array<float, 2> highPassOutput{};

			void AddParameter(const char* name, float value)
			{
				if (parameterCount < parameters.size())
					parameters[parameterCount++] =
						{ParameterHash(name), value};
			}

			float GetParameter(
				const char* name,
				float fallback) const
			{
				const std::uint32_t id = ParameterHash(name);
				for (std::uint32_t index = 0;
					index < parameterCount;
					++index)
				{
					if (parameters[index].id == id)
						return parameters[index].value;
				}
				return fallback;
			}

			bool SetParameter(std::uint32_t id, float value)
			{
				for (std::uint32_t index = 0;
					index < parameterCount;
					++index)
				{
					if (parameters[index].id == id)
					{
						parameters[index].value = value;
						return true;
					}
				}
				return false;
			}
		};

		struct ParameterCommand
		{
			std::uint32_t nodeIndex = 0;
			std::uint32_t parameter = 0;
			float value = 0.0f;
		};

		class ParameterCommandQueue
		{
		public:
			bool Push(const ParameterCommand& command) noexcept
			{
				const std::uint32_t write =
					write_.load(std::memory_order_relaxed);
				const std::uint32_t next =
					(write + 1) % CommandCapacity;
				if (next == read_.load(std::memory_order_acquire))
					return false;
				commands_[write] = command;
				write_.store(next, std::memory_order_release);
				return true;
			}

			bool Pop(ParameterCommand& command) noexcept
			{
				const std::uint32_t read =
					read_.load(std::memory_order_relaxed);
				if (read == write_.load(std::memory_order_acquire))
					return false;
				command = commands_[read];
				read_.store(
					(read + 1) % CommandCapacity,
					std::memory_order_release);
				return true;
			}

		private:
			std::array<ParameterCommand, CommandCapacity> commands_{};
			std::atomic<std::uint32_t> read_{0};
			std::atomic<std::uint32_t> write_{0};
		};
	}

	struct DspGraphStream::Impl
	{
		DspGraphAsset asset;
		std::uint32_t channels = 2;
		std::uint32_t sampleRate = 48000;
		std::uint32_t blockFrames = 512;
		std::vector<RuntimeNode> nodes;
		std::vector<float> buffers;
		std::uint32_t outputIndex = 0;
		std::uint64_t lengthFrames = 0;
		ParameterCommandQueue commands;
		std::atomic<std::uint64_t> pendingSeek{
			std::numeric_limits<std::uint64_t>::max()};
		std::atomic<std::uint64_t> cursorFrames{0};
		std::atomic<std::uint64_t> processedBlocks{0};
		std::atomic<std::uint64_t> processedFrames{0};
		std::atomic<std::uint64_t> droppedCommands{0};
		std::atomic<float> outputPeak{0.0f};
		std::atomic<bool> looping{false};

		float* Buffer(std::uint32_t node)
		{
			return buffers.data() +
				static_cast<std::size_t>(node) *
					blockFrames * channels;
		}

		const float* Buffer(std::uint32_t node) const
		{
			return buffers.data() +
				static_cast<std::size_t>(node) *
					blockFrames * channels;
		}

		bool HasLoopingSource() const
		{
			return looping.load(std::memory_order_acquire);
		}

		void ApplyCommands() noexcept
		{
			ParameterCommand command;
			bool loopChanged = false;
			while (commands.Pop(command))
			{
				if (command.nodeIndex < nodes.size())
				{
					nodes[command.nodeIndex].SetParameter(
						command.parameter, command.value);
					loopChanged |=
						command.parameter == ParameterHash("Loop");
				}
			}
			if (loopChanged)
			{
				bool anyLooping = false;
				for (const RuntimeNode& node : nodes)
				{
					anyLooping |=
						node.type == DspNodeType::WavePlayer &&
						node.GetParameter("Loop", 0.0f) >= 0.5f;
				}
				looping.store(anyLooping, std::memory_order_release);
			}
		}

		void ApplySeek() noexcept
		{
			const std::uint64_t requested = pendingSeek.exchange(
				std::numeric_limits<std::uint64_t>::max(),
				std::memory_order_acq_rel);
			if (requested ==
				std::numeric_limits<std::uint64_t>::max())
			{
				return;
			}
			cursorFrames.store(requested, std::memory_order_release);
			for (RuntimeNode& node : nodes)
			{
				if (node.type == DspNodeType::WavePlayer)
				{
					node.clipCursor = node.clipFrames == 0
						? 0
						: std::min(requested, node.clipFrames);
				}
				std::fill(
					node.delayBuffer.begin(),
					node.delayBuffer.end(),
					0.0f);
				node.delayCursor = 0;
				node.lowPassState = {};
				node.highPassInput = {};
				node.highPassOutput = {};
				for (DelayLine& line : node.reverbLines)
				{
					std::fill(
						line.samples.begin(),
						line.samples.end(),
						0.0f);
					line.cursor = 0;
					line.filterState = 0.0f;
				}
			}
		}

		void ProcessWave(
			RuntimeNode& node,
			float* output,
			std::uint32_t frames) noexcept
		{
			const bool looping =
				node.GetParameter("Loop", 0.0f) >= 0.5f;
			for (std::uint32_t frame = 0; frame < frames; ++frame)
			{
				if (node.clipCursor >= node.clipFrames)
				{
					if (looping && node.clipFrames != 0)
						node.clipCursor = 0;
					else
					{
						for (std::uint32_t channel = 0;
							channel < channels;
							++channel)
						{
							output[
								static_cast<std::size_t>(frame) *
									channels + channel] = 0.0f;
						}
						continue;
					}
				}
				const std::size_t source =
					static_cast<std::size_t>(node.clipCursor) * channels;
				const std::size_t target =
					static_cast<std::size_t>(frame) * channels;
				for (std::uint32_t channel = 0;
					channel < channels;
					++channel)
				{
					output[target + channel] =
						node.clipSamples[source + channel];
				}
				++node.clipCursor;
			}
		}

		void ProcessGain(
			const RuntimeNode& node,
			const float* input,
			float* output,
			std::uint32_t sampleCount) noexcept
		{
			const float gain =
				std::max(0.0f, node.GetParameter("Gain", 1.0f));
			for (std::uint32_t sample = 0;
				sample < sampleCount;
				++sample)
			{
				output[sample] = input[sample] * gain;
			}
		}

		void ProcessPan(
			const RuntimeNode& node,
			const float* input,
			float* output,
			std::uint32_t frames) noexcept
		{
			if (channels != 2)
			{
				std::memcpy(
					output, input, sizeof(float) * frames * channels);
				return;
			}
			const float pan = std::clamp(
				node.GetParameter("Pan", 0.0f), -1.0f, 1.0f);
			const float angle = (pan + 1.0f) * Pi * 0.25f;
			const float leftGain = std::cos(angle);
			const float rightGain = std::sin(angle);
			for (std::uint32_t frame = 0; frame < frames; ++frame)
			{
				output[frame * 2] = input[frame * 2] * leftGain;
				output[frame * 2 + 1] =
					input[frame * 2 + 1] * rightGain;
			}
		}

		void ProcessLowPass(
			RuntimeNode& node,
			const float* input,
			float* output,
			std::uint32_t frames) noexcept
		{
			const float cutoff = std::clamp(
				node.GetParameter("Cutoff", 12000.0f),
				20.0f,
				sampleRate * 0.45f);
			const float timeStep = 1.0f / sampleRate;
			const float rc = 1.0f / (2.0f * Pi * cutoff);
			const float alpha = timeStep / (rc + timeStep);
			for (std::uint32_t frame = 0; frame < frames; ++frame)
			{
				for (std::uint32_t channel = 0;
					channel < channels;
					++channel)
				{
					const std::size_t index =
						static_cast<std::size_t>(frame) *
							channels + channel;
					node.lowPassState[channel] +=
						alpha *
						(input[index] - node.lowPassState[channel]);
					output[index] = node.lowPassState[channel];
				}
			}
		}

		void ProcessHighPass(
			RuntimeNode& node,
			const float* input,
			float* output,
			std::uint32_t frames) noexcept
		{
			const float cutoff = std::clamp(
				node.GetParameter("Cutoff", 80.0f),
				20.0f,
				sampleRate * 0.45f);
			const float timeStep = 1.0f / sampleRate;
			const float rc = 1.0f / (2.0f * Pi * cutoff);
			const float alpha = rc / (rc + timeStep);
			for (std::uint32_t frame = 0; frame < frames; ++frame)
			{
				for (std::uint32_t channel = 0;
					channel < channels;
					++channel)
				{
					const std::size_t index =
						static_cast<std::size_t>(frame) *
							channels + channel;
					const float value = alpha *
						(node.highPassOutput[channel] +
						 input[index] -
						 node.highPassInput[channel]);
					node.highPassInput[channel] = input[index];
					node.highPassOutput[channel] = value;
					output[index] = value;
				}
			}
		}

		void ProcessDelay(
			RuntimeNode& node,
			const float* input,
			float* output,
			std::uint32_t frames) noexcept
		{
			const std::size_t capacityFrames =
				node.delayBuffer.size() / channels;
			const std::size_t delayFrames = std::clamp<std::size_t>(
				static_cast<std::size_t>(
					std::max(
						0.001f,
						node.GetParameter("Time", 0.25f)) *
					sampleRate),
				1,
				std::max<std::size_t>(1, capacityFrames - 1));
			const float feedback = std::clamp(
				node.GetParameter("Feedback", 0.35f),
				-0.98f,
				0.98f);
			const float mix = std::clamp(
				node.GetParameter("Mix", 0.3f), 0.0f, 1.0f);
			for (std::uint32_t frame = 0; frame < frames; ++frame)
			{
				const std::size_t readFrame =
					(node.delayCursor + capacityFrames - delayFrames) %
					capacityFrames;
				for (std::uint32_t channel = 0;
					channel < channels;
					++channel)
				{
					const std::size_t source =
						static_cast<std::size_t>(frame) *
							channels + channel;
					const std::size_t read =
						readFrame * channels + channel;
					const std::size_t write =
						node.delayCursor * channels + channel;
					const float delayed = node.delayBuffer[read];
					node.delayBuffer[write] =
						input[source] + delayed * feedback;
					output[source] =
						input[source] * (1.0f - mix) +
						delayed * mix;
				}
				node.delayCursor =
					(node.delayCursor + 1) % capacityFrames;
			}
		}

		float ProcessComb(
			DelayLine& line,
			float input,
			float feedback,
			float damping) noexcept
		{
			const float delayed = line.samples[line.cursor];
			line.filterState =
				delayed * (1.0f - damping) +
				line.filterState * damping;
			line.samples[line.cursor] =
				input + line.filterState * feedback;
			line.cursor = (line.cursor + 1) % line.samples.size();
			return delayed;
		}

		float ProcessAllPass(
			DelayLine& line,
			float input) noexcept
		{
			const float delayed = line.samples[line.cursor];
			const float output = delayed - input;
			line.samples[line.cursor] = input + delayed * 0.5f;
			line.cursor = (line.cursor + 1) % line.samples.size();
			return output;
		}

		void ProcessReverb(
			RuntimeNode& node,
			const float* input,
			float* output,
			std::uint32_t frames) noexcept
		{
			const float room = std::clamp(
				node.GetParameter("RoomSize", 0.65f), 0.0f, 0.98f);
			const float damping = std::clamp(
				node.GetParameter("Damping", 0.35f), 0.0f, 0.98f);
			const float wet = std::clamp(
				node.GetParameter("Wet", 0.3f), 0.0f, 1.0f);
			const float dry = std::clamp(
				node.GetParameter("Dry", 0.8f), 0.0f, 1.0f);
			for (std::uint32_t frame = 0; frame < frames; ++frame)
			{
				for (std::uint32_t channel = 0;
					channel < channels;
					++channel)
				{
					const std::size_t index =
						static_cast<std::size_t>(frame) *
							channels + channel;
					const std::size_t base = channel * 6;
					float reverberated = 0.0f;
					for (std::size_t comb = 0; comb < 4; ++comb)
					{
						reverberated += ProcessComb(
							node.reverbLines[base + comb],
							input[index],
							room,
							damping);
					}
					reverberated *= 0.25f;
					reverberated = ProcessAllPass(
						node.reverbLines[base + 4],
						reverberated);
					reverberated = ProcessAllPass(
						node.reverbLines[base + 5],
						reverberated);
					output[index] =
						input[index] * dry + reverberated * wet;
				}
			}
		}

		void ProcessMixer(
			const RuntimeNode& node,
			float* output,
			std::uint32_t sampleCount) noexcept
		{
			std::fill(output, output + sampleCount, 0.0f);
			for (std::uint32_t inputIndex = 0;
				inputIndex < node.inputCount;
				++inputIndex)
			{
				const float* input = Buffer(node.inputs[inputIndex]);
				for (std::uint32_t sample = 0;
					sample < sampleCount;
					++sample)
				{
					output[sample] += input[sample];
				}
			}
			const float gain =
				std::max(0.0f, node.GetParameter("Gain", 1.0f));
			for (std::uint32_t sample = 0;
				sample < sampleCount;
				++sample)
			{
				output[sample] *= gain;
			}
		}

		void ProcessBlock(std::uint32_t frames) noexcept
		{
			const std::uint32_t sampleCount = frames * channels;
			for (std::uint32_t nodeIndex = 0;
				nodeIndex < nodes.size();
				++nodeIndex)
			{
				RuntimeNode& node = nodes[nodeIndex];
				float* output = Buffer(nodeIndex);
				const float* input = node.inputCount == 0
					? nullptr
					: Buffer(node.inputs[0]);
				switch (node.type)
				{
				case DspNodeType::WavePlayer:
					ProcessWave(node, output, frames);
					break;
				case DspNodeType::Gain:
					ProcessGain(node, input, output, sampleCount);
					break;
				case DspNodeType::Pan:
					ProcessPan(node, input, output, frames);
					break;
				case DspNodeType::LowPass:
					ProcessLowPass(node, input, output, frames);
					break;
				case DspNodeType::HighPass:
					ProcessHighPass(node, input, output, frames);
					break;
				case DspNodeType::Delay:
					ProcessDelay(node, input, output, frames);
					break;
				case DspNodeType::Reverb:
					ProcessReverb(node, input, output, frames);
					break;
				case DspNodeType::Mixer:
					ProcessMixer(node, output, sampleCount);
					break;
				case DspNodeType::Output:
					std::memcpy(
						output,
						input,
						sizeof(float) * sampleCount);
					break;
				}
			}
			float peak = 0.0f;
			const float* output = Buffer(outputIndex);
			for (std::uint32_t sample = 0;
				sample < sampleCount;
				++sample)
			{
				peak = std::max(peak, std::abs(output[sample]));
			}
			outputPeak.store(peak, std::memory_order_release);
			processedBlocks.fetch_add(1, std::memory_order_relaxed);
			processedFrames.fetch_add(frames, std::memory_order_relaxed);
		}
	};

	DspGraphStream::DspGraphStream(
		std::unique_ptr<Impl> implementation)
		: implementation_(std::move(implementation))
	{
	}

	DspGraphStream::~DspGraphStream() = default;

	std::shared_ptr<DspGraphStream> DspGraphStream::Compile(
		const DspGraphAsset& asset,
		std::vector<DspDiagnostic>& diagnostics)
	{
		diagnostics.clear();
		auto error = [&diagnostics](
			std::uint64_t nodeId,
			std::string message)
		{
			diagnostics.push_back({
				DspDiagnosticSeverity::Error,
				nodeId,
				std::move(message)});
		};

		if (asset.sampleRate < 8000 || asset.sampleRate > 192000)
			error(0, "Sample rate must be between 8000 and 192000 Hz.");
		if (asset.channels == 0 || asset.channels > 2)
			error(0, "The DSP runtime currently supports mono or stereo.");
		if (asset.blockFrames < 16 || asset.blockFrames > 4096)
			error(0, "Block size must be between 16 and 4096 frames.");

		std::unordered_map<std::uint64_t, const DspNodeAsset*> sourceNodes;
		std::unordered_map<std::uint64_t, std::uint32_t> indegree;
		std::unordered_map<
			std::uint64_t,
			std::vector<std::uint64_t>> edges;
		for (const DspNodeAsset& node : asset.nodes)
		{
			if (node.id == 0 || !sourceNodes.emplace(node.id, &node).second)
			{
				error(node.id, "DSP node IDs must be unique and non-zero.");
				continue;
			}
			indegree[node.id] = 0;
			if (node.inputs.size() < RequiredInputs(node.type))
				error(node.id, "The DSP node has too few audio inputs.");
			if (node.type != DspNodeType::Mixer &&
				node.inputs.size() > RequiredInputs(node.type))
			{
				error(node.id, "This DSP node accepts only one audio input.");
			}
			if (node.inputs.size() > MaxNodeInputs)
				error(node.id, "A DSP node supports at most eight inputs.");
		}

		for (const DspNodeAsset& node : asset.nodes)
		{
			for (const std::uint64_t input : node.inputs)
			{
				if (!sourceNodes.contains(input))
				{
					error(node.id, "DSP input references a missing node.");
					continue;
				}
				edges[input].push_back(node.id);
				++indegree[node.id];
			}
		}

		const auto outputFound = sourceNodes.find(asset.outputNode);
		if (outputFound == sourceNodes.end() ||
			outputFound->second->type != DspNodeType::Output)
		{
			error(0, "The graph requires a valid DSP Output node.");
		}

		std::queue<std::uint64_t> ready;
		for (const auto& [id, degree] : indegree)
		{
			if (degree == 0)
				ready.push(id);
		}
		std::vector<std::uint64_t> order;
		while (!ready.empty())
		{
			const std::uint64_t id = ready.front();
			ready.pop();
			order.push_back(id);
			for (const std::uint64_t target : edges[id])
			{
				if (--indegree[target] == 0)
					ready.push(target);
			}
		}
		if (order.size() != asset.nodes.size())
			error(0, "The DSP graph contains an audio routing cycle.");

		if (std::any_of(
			diagnostics.begin(),
			diagnostics.end(),
			[](const DspDiagnostic& diagnostic)
			{
				return diagnostic.severity ==
					DspDiagnosticSeverity::Error;
			}))
		{
			return {};
		}

		auto implementation = std::make_unique<Impl>();
		implementation->asset = asset;
		implementation->channels = asset.channels;
		implementation->sampleRate = asset.sampleRate;
		implementation->blockFrames = asset.blockFrames;
		implementation->nodes.reserve(order.size());
		std::unordered_map<std::uint64_t, std::uint32_t> runtimeIndices;
		std::uint64_t maximumClipFrames = 0;
		std::uint64_t maximumTailFrames = 0;

		for (const std::uint64_t id : order)
		{
			const DspNodeAsset& source = *sourceNodes[id];
			RuntimeNode runtime;
			runtime.id = source.id;
			runtime.type = source.type;
			for (const std::uint64_t input : source.inputs)
				runtime.inputs[runtime.inputCount++] = runtimeIndices[input];

			switch (source.type)
			{
			case DspNodeType::WavePlayer:
			{
				runtime.AddParameter(
					"Loop", Parameter(source, "Loop", 0.0f));
				if (source.clipPath.empty() ||
					!DecodeClip(
						source.clipPath,
						asset.channels,
						asset.sampleRate,
						runtime.clipSamples,
						runtime.clipFrames))
				{
					error(source.id, "Could not decode the Wave Player clip.");
					return {};
				}
				maximumClipFrames =
					std::max(maximumClipFrames, runtime.clipFrames);
				break;
			}
			case DspNodeType::Gain:
				runtime.AddParameter(
					"Gain", Parameter(source, "Gain", 1.0f));
				break;
			case DspNodeType::Pan:
				runtime.AddParameter(
					"Pan", Parameter(source, "Pan", 0.0f));
				break;
			case DspNodeType::LowPass:
				runtime.AddParameter(
					"Cutoff", Parameter(source, "Cutoff", 12000.0f));
				break;
			case DspNodeType::HighPass:
				runtime.AddParameter(
					"Cutoff", Parameter(source, "Cutoff", 80.0f));
				break;
			case DspNodeType::Delay:
			{
				const float maximumDelay = std::clamp(
					Parameter(source, "MaxDelay", 2.0f),
					0.05f,
					10.0f);
				runtime.AddParameter(
					"Time", Parameter(source, "Time", 0.25f));
				runtime.AddParameter(
					"Feedback", Parameter(source, "Feedback", 0.35f));
				runtime.AddParameter(
					"Mix", Parameter(source, "Mix", 0.3f));
				const std::size_t delayFrames =
					static_cast<std::size_t>(
						maximumDelay * asset.sampleRate) + 1;
				runtime.delayBuffer.resize(
					delayFrames * asset.channels, 0.0f);
				maximumTailFrames = std::max<std::uint64_t>(
					maximumTailFrames,
					static_cast<std::uint64_t>(
						maximumDelay * asset.sampleRate));
				break;
			}
			case DspNodeType::Reverb:
			{
				runtime.AddParameter(
					"RoomSize", Parameter(source, "RoomSize", 0.65f));
				runtime.AddParameter(
					"Damping", Parameter(source, "Damping", 0.35f));
				runtime.AddParameter(
					"Wet", Parameter(source, "Wet", 0.3f));
				runtime.AddParameter(
					"Dry", Parameter(source, "Dry", 0.8f));
				const std::array<int, 6> lengths{
					1557, 1617, 1491, 1422, 225, 556};
				for (std::uint32_t channel = 0;
					channel < asset.channels;
					++channel)
				{
					for (std::size_t line = 0;
						line < lengths.size();
						++line)
					{
						const float scale =
							static_cast<float>(asset.sampleRate) / 44100.0f;
						const std::size_t length = std::max<std::size_t>(
							1,
							static_cast<std::size_t>(
								lengths[line] * scale) +
								channel * 23);
						runtime.reverbLines[channel * 6 + line]
							.samples.resize(length, 0.0f);
					}
				}
				maximumTailFrames = std::max<std::uint64_t>(
					maximumTailFrames,
					static_cast<std::uint64_t>(asset.sampleRate) * 3);
				break;
			}
			case DspNodeType::Mixer:
				runtime.AddParameter(
					"Gain", Parameter(source, "Gain", 1.0f));
				break;
			case DspNodeType::Output:
				break;
			}

			const std::uint32_t runtimeIndex =
				static_cast<std::uint32_t>(implementation->nodes.size());
			runtimeIndices[id] = runtimeIndex;
			implementation->nodes.push_back(std::move(runtime));
		}

		implementation->outputIndex =
			runtimeIndices[asset.outputNode];
		for (const RuntimeNode& node : implementation->nodes)
		{
			if (node.type == DspNodeType::WavePlayer &&
				node.GetParameter("Loop", 0.0f) >= 0.5f)
			{
				implementation->looping.store(
					true, std::memory_order_release);
				break;
			}
		}
		implementation->lengthFrames =
			maximumClipFrames + maximumTailFrames;
		implementation->buffers.resize(
			implementation->nodes.size() *
			asset.blockFrames *
			asset.channels,
			0.0f);
		return std::shared_ptr<DspGraphStream>(
			new DspGraphStream(std::move(implementation)));
	}

	std::uint32_t DspGraphStream::GetChannels() const
	{
		return implementation_->channels;
	}

	std::uint32_t DspGraphStream::GetSampleRate() const
	{
		return implementation_->sampleRate;
	}

	std::uint64_t DspGraphStream::GetCursorFrames() const
	{
		return implementation_->cursorFrames.load(
			std::memory_order_acquire);
	}

	std::uint64_t DspGraphStream::GetLengthFrames() const
	{
		return implementation_->HasLoopingSource()
			? 0
			: implementation_->lengthFrames;
	}

	std::uint64_t DspGraphStream::ReadFrames(
		float* output,
		std::uint64_t frameCount) noexcept
	{
		if (!output || frameCount == 0)
			return 0;
		Impl& impl = *implementation_;
		impl.ApplyCommands();
		impl.ApplySeek();
		std::uint64_t written = 0;
		while (written < frameCount)
		{
			const bool looping = impl.HasLoopingSource();
			const std::uint64_t cursor = impl.cursorFrames.load(
				std::memory_order_relaxed);
			if (!looping && cursor >= impl.lengthFrames)
				break;
			std::uint64_t remaining = frameCount - written;
			if (!looping)
				remaining = std::min(
					remaining, impl.lengthFrames - cursor);
			const std::uint32_t chunk =
				static_cast<std::uint32_t>(std::min<std::uint64_t>(
					remaining, impl.blockFrames));
			impl.ProcessBlock(chunk);
			std::memcpy(
				output + written * impl.channels,
				impl.Buffer(impl.outputIndex),
				sizeof(float) * chunk * impl.channels);
			written += chunk;
			impl.cursorFrames.fetch_add(
				chunk, std::memory_order_release);
		}
		return written;
	}

	bool DspGraphStream::SeekFrame(std::uint64_t frame) noexcept
	{
		implementation_->pendingSeek.store(
			frame, std::memory_order_release);
		return true;
	}

	void DspGraphStream::SetLooping(bool looping) noexcept
	{
		for (std::uint32_t index = 0;
			index < implementation_->nodes.size();
			++index)
		{
			if (implementation_->nodes[index].type ==
				DspNodeType::WavePlayer)
			{
				const ParameterCommand command{
					index,
					ParameterHash("Loop"),
					looping ? 1.0f : 0.0f};
				if (!implementation_->commands.Push(command))
				{
					implementation_->droppedCommands.fetch_add(
						1, std::memory_order_relaxed);
				}
			}
		}
	}

	bool DspGraphStream::SetParameter(
		std::uint64_t nodeId,
		std::string_view parameter,
		float value) noexcept
	{
		for (std::uint32_t index = 0;
			index < implementation_->nodes.size();
			++index)
		{
			const RuntimeNode& node = implementation_->nodes[index];
			if (node.id != nodeId)
				continue;
			const std::uint32_t parameterId = ParameterHash(parameter);
			bool found = false;
			for (std::uint32_t slot = 0;
				slot < node.parameterCount;
				++slot)
			{
				found |= node.parameters[slot].id == parameterId;
			}
			if (!found)
				return false;
			if (!implementation_->commands.Push(
					{index, parameterId, value}))
			{
				implementation_->droppedCommands.fetch_add(
					1, std::memory_order_relaxed);
				return false;
			}
			return true;
		}
		return false;
	}

	DspRealtimeStats DspGraphStream::GetStats() const noexcept
	{
		return {
			implementation_->processedBlocks.load(
				std::memory_order_acquire),
			implementation_->processedFrames.load(
				std::memory_order_acquire),
			implementation_->droppedCommands.load(
				std::memory_order_acquire),
			implementation_->outputPeak.load(
				std::memory_order_acquire)};
	}

	const DspGraphAsset& DspGraphStream::GetAsset() const
	{
		return implementation_->asset;
	}

	DspGraphAsset CreateDefaultDspGraph(const std::string& clipPath)
	{
		DspGraphAsset graph;
		graph.name = "ReziAudio DSP Demo";
		graph.nodes = {
			{1, DspNodeType::WavePlayer, "Wave Player", clipPath, {},
			 {{"Loop", 1.0f}}},
			{2, DspNodeType::Gain, "Gain", {}, {1},
			 {{"Gain", 0.8f}}},
			{3, DspNodeType::LowPass, "Low Pass", {}, {2},
			 {{"Cutoff", 9000.0f}}},
			{4, DspNodeType::Delay, "Delay", {}, {3},
			 {{"Time", 0.22f}, {"Feedback", 0.3f},
			  {"Mix", 0.22f}, {"MaxDelay", 2.0f}}},
			{5, DspNodeType::Reverb, "Reverb", {}, {4},
			 {{"RoomSize", 0.65f}, {"Damping", 0.35f},
			  {"Wet", 0.25f}, {"Dry", 0.85f}}},
			{6, DspNodeType::Pan, "Pan", {}, {5},
			 {{"Pan", 0.0f}}},
			{7, DspNodeType::Output, "Output", {}, {6}, {}}
		};
		graph.outputNode = 7;
		return graph;
	}
}
