#include "../ReziAudioDspGraph.h"
#include "../ReziAudioSystem.h"
#include "../BlueprintNodeStyle.h"
#include "ReziAudioParameterWidgets.h"

#define SDL_MAIN_HANDLED
#include <GL/glew.h>
#include <SDL.h>
#include <imgui.h>
#include <imgui_node_editor.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl.h>
#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ed = ax::NodeEditor;
using namespace EGE::ReziAudio;
namespace blueprint = EGE::BlueprintNodeStyle;
namespace parameterUi = EGE::ReziAudio::TestUi;

namespace
{
	constexpr std::uint64_t PinStride = 32;

	const char* NodeName(DspNodeType type)
	{
		const DspNodeDescriptor* descriptor =
			FindDspNodeDescriptor(type);
		return descriptor
			? descriptor->displayName.data()
			: "DSP Node";
	}

	ImVec4 NodeColor(DspNodeType type)
	{
		switch (type)
		{
		case DspNodeType::EventInput:
			return ImVec4(0.95f, 0.28f, 0.36f, 1);
		case DspNodeType::WavePlayer:
		case DspNodeType::SineOscillator:
		case DspNodeType::NoiseGenerator:
		case DspNodeType::ADEnvelope:
			return ImVec4(0.35f, 0.85f, 0.6f, 1);
		case DspNodeType::RepeatTrigger:
		case DspNodeType::DelayedTrigger:
		case DspNodeType::TriggerCounter:
			return ImVec4(0.92f, 0.32f, 0.46f, 1);
		case DspNodeType::Output:
			return ImVec4(1.0f, 0.45f, 0.35f, 1);
		case DspNodeType::LowPass:
		case DspNodeType::HighPass:
			return ImVec4(0.45f, 0.7f, 1.0f, 1);
		case DspNodeType::Delay:
		case DspNodeType::Reverb:
			return ImVec4(0.75f, 0.45f, 1.0f, 1);
		case DspNodeType::AudioAdd:
		case DspNodeType::AudioSubtract:
		case DspNodeType::AudioMultiply:
		case DspNodeType::AudioMinimum:
		case DspNodeType::AudioMaximum:
		case DspNodeType::AudioClamp:
		case DspNodeType::AudioMapRange:
		case DspNodeType::AudioOffset:
			return ImVec4(0.86f, 0.50f, 0.24f, 1.0f);
		default:
			return ImVec4(0.3f, 0.75f, 1.0f, 1);
		}
	}

	std::uint64_t OutputPin(std::uint64_t node)
	{
		return node * PinStride + 1;
	}

	std::uint64_t InputPin(std::uint64_t node, std::size_t slot)
	{
		return node * PinStride + 2 + slot;
	}

	std::uint64_t LinkId(std::uint64_t node, std::size_t slot)
	{
		return node * PinStride + 16 + slot;
	}

	std::size_t VisibleInputCount(const DspNodeAsset& node)
	{
		const DspNodeDescriptor* descriptor =
			FindDspNodeDescriptor(node.type);
		if (!descriptor)
			return 1;
		if (descriptor->maximumInputs > descriptor->minimumInputs)
		{
			return std::min<std::size_t>(
				descriptor->maximumInputs,
				node.inputs.size() + 1);
		}
		return descriptor->maximumInputs;
	}

	struct PinReference
	{
		DspNodeAsset* node = nullptr;
		std::size_t slot = 0;
		bool output = false;
	};

	PinReference FindPin(DspGraphAsset& graph, std::uint64_t pin)
	{
		for (DspNodeAsset& node : graph.nodes)
		{
			if (node.type != DspNodeType::Output &&
				OutputPin(node.id) == pin)
			{
				return {&node, 0, true};
			}
			for (std::size_t slot = 0;
				slot < VisibleInputCount(node);
				++slot)
			{
				if (InputPin(node.id, slot) == pin)
					return {&node, slot, false};
			}
		}
		return {};
	}

	bool HasOutputLink(
		const DspGraphAsset& graph,
		std::uint64_t nodeId)
	{
		return std::any_of(
			graph.nodes.begin(),
			graph.nodes.end(),
			[nodeId](const DspNodeAsset& candidate)
			{
				return std::find(
					candidate.inputs.begin(),
					candidate.inputs.end(),
					nodeId) != candidate.inputs.end();
			});
	}

	void DrawNode(DspNodeAsset& node, const DspGraphAsset& graph)
	{
		blueprint::NodeBuilder builder(
			node.id,
			node.name,
			NodeColor(node.type),
			220.0f);
		builder.Begin();
		const std::size_t inputCount = VisibleInputCount(node);
		const std::size_t rows = std::max<std::size_t>(inputCount, 1);
		for (std::size_t row = 0; row < rows; ++row)
		{
			std::optional<blueprint::Pin> input;
			std::optional<blueprint::Pin> output;
			if (row < inputCount)
			{
				const bool triggerInput =
					node.type == DspNodeType::ADEnvelope ||
					node.type == DspNodeType::TriggerCounter;
				input = blueprint::Pin{
					InputPin(node.id, row),
					node.type == DspNodeType::Mixer
						? "Input " + std::to_string(row + 1)
						: triggerInput
							? "Trigger"
						: inputCount == 2
							? (row == 0 ? "A" : "B")
							: "Audio",
					ImVec4(0.35f, 0.82f, 0.62f, 1.0f),
					blueprint::PinShape::Circle,
					row < node.inputs.size() && node.inputs[row] != 0};
			}
			if (node.type != DspNodeType::Output && row == 0)
			{
				const bool triggerOutput =
					node.type == DspNodeType::EventInput ||
					node.type == DspNodeType::RepeatTrigger ||
					node.type == DspNodeType::DelayedTrigger;
				output = blueprint::Pin{
					OutputPin(node.id),
					triggerOutput ? "Trigger" : "Audio",
					ImVec4(0.35f, 0.82f, 0.62f, 1.0f),
					blueprint::PinShape::Circle,
					HasOutputLink(graph, node.id)};
			}
			builder.PinRow(
				input ? &*input : nullptr,
				output ? &*output : nullptr);
		}
		if (node.type == DspNodeType::WavePlayer)
		{
			const std::string filename =
				parameterUi::AudioClipLabel(node.clip);
			builder.Text(
				filename.c_str(),
				ImVec4(0.62f, 0.65f, 0.72f, 1.0f));
		}
		else if (node.type == DspNodeType::EventInput)
		{
			builder.Text(
				node.eventName.c_str(),
				ImVec4(1.0f, 0.62f, 0.68f, 1.0f));
		}
		builder.End();
	}

	struct DspConnection
	{
		DspNodeAsset* target = nullptr;
		std::size_t slot = 0;
	};

	void DrawPinContextMenu(
		DspGraphAsset& graph,
		std::uint64_t contextPinId,
		bool& dirty)
	{
		std::optional<DspConnection> disconnectOne;
		bool disconnectAll = false;

		if (ImGui::BeginPopup("DSP Pin Context Menu"))
		{
			const PinReference location =
				FindPin(graph, contextPinId);
			if (!location.node)
			{
				ImGui::TextDisabled("The pin no longer exists.");
				ImGui::EndPopup();
				return;
			}

			ImGui::TextUnformatted(
				location.output ? "Audio Output" : "Audio Input");
			ImGui::SameLine();
			ImGui::TextDisabled("(Audio Buffer)");
			ImGui::Separator();

			std::vector<DspConnection> connections;
			if (location.output)
			{
				for (DspNodeAsset& candidate : graph.nodes)
				{
					for (std::size_t slot = 0;
						slot < candidate.inputs.size();
						++slot)
					{
						if (candidate.inputs[slot] ==
							location.node->id)
						{
							connections.push_back(
								{&candidate, slot});
						}
					}
				}
			}
			else if (
				location.slot < location.node->inputs.size() &&
				location.node->inputs[location.slot] != 0)
			{
				connections.push_back(
					{location.node, location.slot});
			}

			if (connections.size() == 1)
			{
				if (ImGui::MenuItem("Disconnect"))
					disconnectOne = connections.front();
			}
			else if (connections.size() > 1)
			{
				if (ImGui::BeginMenu("Disconnect connection"))
				{
					for (const DspConnection& connection :
						connections)
					{
						ImGui::PushID(
							static_cast<int>(
								connection.target->id * PinStride +
								connection.slot));
						const std::string label =
							connection.target->name +
							" Input " +
							std::to_string(connection.slot + 1);
						if (ImGui::MenuItem(label.c_str()))
							disconnectOne = connection;
						ImGui::PopID();
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				const std::string label =
					"Disconnect all (" +
					std::to_string(connections.size()) + ")";
				if (ImGui::MenuItem(label.c_str()))
					disconnectAll = true;
			}
			else
			{
				ImGui::TextDisabled("Not connected.");
				ImGui::TextDisabled(
					"Audio buffers cannot be promoted "
					"to graph variables.");
			}
			ImGui::EndPopup();
		}

		if (disconnectAll)
		{
			const PinReference location =
				FindPin(graph, contextPinId);
			if (location.node && location.output)
			{
				for (DspNodeAsset& candidate : graph.nodes)
					std::erase(
						candidate.inputs,
						location.node->id);
				dirty = true;
			}
		}
		else if (
			disconnectOne &&
			disconnectOne->target &&
			disconnectOne->slot <
				disconnectOne->target->inputs.size())
		{
			disconnectOne->target->inputs.erase(
				disconnectOne->target->inputs.begin() +
				disconnectOne->slot);
			dirty = true;
		}
	}

	DspNodeAsset CreateNode(
		DspNodeType type,
		std::uint64_t id,
		const AudioClipReference& defaultClip)
	{
		DspNodeAsset node;
		node.id = id;
		node.type = type;
		node.name = NodeName(type);
		if (type == DspNodeType::WavePlayer)
			node.clip = defaultClip;
		if (type == DspNodeType::EventInput)
			node.eventName = "Play";
		node.parameters = CreateDspParameterDefaults(type);
		return node;
	}

	bool DrawDspParameter(
		DspNodeAsset& node,
		const DspParameterDescriptor& descriptor,
		std::shared_ptr<DspGraphStream>& runtime,
		bool& graphDirty)
	{
		const std::string name(descriptor.name);
		auto found = node.parameters.find(name);
		if (found == node.parameters.end())
			return false;
		bool changed = false;
		if (descriptor.editor == DspParameterEditor::Toggle)
		{
			bool value = found->second >= 0.5f;
			changed = ImGui::Checkbox(name.c_str(), &value);
			if (changed)
				found->second = value ? 1.0f : 0.0f;
		}
		else if (descriptor.editor == DspParameterEditor::Integer)
		{
			int value = static_cast<int>(std::round(found->second));
			changed = ImGui::SliderInt(
				name.c_str(),
				&value,
				static_cast<int>(descriptor.minimum),
				static_cast<int>(descriptor.maximum));
			if (changed)
				found->second = static_cast<float>(value);
		}
		else if (descriptor.editor == DspParameterEditor::Choice)
		{
			std::vector<std::string> choices;
			std::size_t start = 0;
			while (start <= descriptor.choices.size())
			{
				const std::size_t separator =
					descriptor.choices.find('|', start);
				const std::size_t end = separator ==
					std::string_view::npos
						? descriptor.choices.size()
						: separator;
				choices.emplace_back(
					descriptor.choices.substr(start, end - start));
				if (separator == std::string_view::npos)
					break;
				start = separator + 1;
			}
			int selected = std::clamp(
				static_cast<int>(std::round(found->second)),
				0,
				static_cast<int>(choices.size()) - 1);
			const char* preview = choices.empty()
				? "None"
				: choices[selected].c_str();
			if (ImGui::BeginCombo(name.c_str(), preview))
			{
				for (int index = 0;
					index < static_cast<int>(choices.size());
					++index)
				{
					if (ImGui::Selectable(
							choices[index].c_str(),
							selected == index))
					{
						selected = index;
						found->second = static_cast<float>(index);
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
		}
		else
		{
			const ImGuiSliderFlags flags = descriptor.logarithmic
				? ImGuiSliderFlags_Logarithmic
				: 0;
			float maximum = descriptor.maximum;
			if (!descriptor.maximumParameter.empty())
			{
				const auto linkedMaximum = node.parameters.find(
					std::string(descriptor.maximumParameter));
				if (linkedMaximum != node.parameters.end())
					maximum = linkedMaximum->second;
			}
			changed = ImGui::SliderFloat(
				name.c_str(),
				&found->second,
				descriptor.minimum,
				maximum,
				"%.3f",
				flags);
		}
		if (!changed)
			return false;
		if (descriptor.runtimeMutable && runtime)
			runtime->SetParameter(
				node.id, descriptor.name, found->second);
		if (!descriptor.runtimeMutable)
			graphDirty = true;
		return true;
	}
}

int main(int argc, char** argv)
{
	const bool smoke =
		argc > 1 && std::string(argv[1]) == "--smoke";
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
		return 1;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(
		SDL_GL_CONTEXT_PROFILE_MASK,
		SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_Window* window = SDL_CreateWindow(
		"ReziAudio Real-Time DSP Graph Lab",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		1360,
		800,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
			(smoke ? SDL_WINDOW_HIDDEN : 0));
	if (!window)
		return 1;
	SDL_GLContext glContext = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, glContext);
	SDL_GL_SetSwapInterval(1);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
		return 1;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(window, glContext);
	ImGui_ImplOpenGL3_Init("#version 330 core");
	ed::Config nodeConfig;
	nodeConfig.SettingsFile = nullptr;
	ed::EditorContext* nodeEditor = ed::CreateEditor(&nodeConfig);
	ed::SetCurrentEditor(nodeEditor);
	blueprint::Apply();

	ma_engine engine{};
	ma_engine_config engineConfig = ma_engine_config_init();
	if (smoke)
	{
		engineConfig.noDevice = MA_TRUE;
		engineConfig.channels = 2;
		engineConfig.sampleRate = 48000;
	}
	if (ma_engine_init(&engineConfig, &engine) != MA_SUCCESS)
		return 1;
	System audio;
	if (!audio.Initialize(engine))
		return 1;

	const std::filesystem::path clip =
		argc > 1 && !smoke
		? std::filesystem::path(argv[1])
		: std::filesystem::path(EGE_SOURCE_ROOT) /
			"Game/Assets/Audio/Effects/ding.wav";
	const AudioClipReference defaultClip =
		parameterUi::MakeAudioClipReference(clip);
	const std::vector<AudioClipReference> availableClips =
		parameterUi::BuildAudioClipCatalog(clip);
	DspGraphAsset graph = CreateDefaultDspGraph(defaultClip);
	std::shared_ptr<DspGraphStream> runtime;
	std::vector<DspNodeAsset> runtimeNodes;
	std::vector<DspDiagnostic> diagnostics;
	PlaybackHandle voice;
	bool dirty = true;
	bool firstLayout = true;
	std::uint64_t nextNodeId = 100;
	std::uint64_t contextPinId = 0;
	bool running = true;
	int frameCount = 0;

	const auto compile = [&]()
	{
		audio.DestroyVoice(voice);
		runtime = DspGraphStream::Compile(graph, diagnostics);
		dirty = runtime == nullptr;
		if (runtime)
			runtimeNodes = graph.nodes;
		return runtime != nullptr;
	};
	const auto play = [&]()
	{
		if ((dirty || !runtime) && !compile())
			return false;
		audio.DestroyVoice(voice);
		VoiceSettings settings;
		settings.spatial.enabled = false;
		voice = audio.CreateStreamVoice(runtime, settings);
		return voice.IsValid() && audio.Play(voice);
	};
	compile();

	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				running = false;
		}
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(1344, 784), ImGuiCond_Once);
		ImGui::Begin("Allocation-Free Audio Callback DSP Graph");
		if (ImGui::Button(dirty ? "Compile *" : "Compile"))
			compile();
		ImGui::SameLine();
		if (ImGui::Button("Play DSP Graph"))
			play();
		ImGui::SameLine();
		if (ImGui::Button("Stop"))
			audio.Stop(voice);
		ImGui::SameLine();
		ImGui::TextColored(
			runtime
				? ImVec4(0.3f, 0.9f, 0.55f, 1)
				: ImVec4(1.0f, 0.35f, 0.3f, 1),
			runtime
				? "Compiled real-time program"
				: "Compile failed");

		ImGui::BeginChild("DSP Palette", ImVec2(210, 0), true);
		ImGui::TextUnformatted("DSP NODE PALETTE");
		ImGui::Separator();
		std::string_view paletteCategory;
		for (const DspNodeDescriptor& descriptor :
			GetDspNodeDescriptors())
		{
			if (descriptor.category != paletteCategory)
			{
				paletteCategory = descriptor.category;
				ImGui::Spacing();
				ImGui::TextDisabled(
					"%s", paletteCategory.data());
			}
			ImGui::PushID(static_cast<int>(descriptor.type));
			if (ImGui::Button(
					descriptor.displayName.data(),
					ImVec2(-1, 0)))
			{
				DspNodeAsset node =
					CreateNode(
						descriptor.type,
						nextNodeId++,
						defaultClip);
				node.editorPosition = float2(300.0f, 200.0f);
				if (descriptor.type == DspNodeType::Output &&
					graph.outputNode == 0)
				{
					graph.outputNode = node.id;
				}
				graph.nodes.push_back(std::move(node));
				dirty = true;
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::TextWrapped(
			"Graph compilation and WAV decoding happen outside "
			"the audio callback.");
		ImGui::EndChild();
		ImGui::SameLine();

		ImGui::BeginChild("DSP Editor", ImVec2(-330, 0), true);
		ed::Begin("DspNodeCanvas");
		for (DspNodeAsset& node : graph.nodes)
			DrawNode(node, graph);
		for (const DspNodeAsset& node : graph.nodes)
		{
			for (std::size_t slot = 0;
				slot < node.inputs.size();
				++slot)
			{
				if (node.inputs[slot] == 0)
					continue;
				ed::Link(
					ed::LinkId(LinkId(node.id, slot)),
					ed::PinId(OutputPin(node.inputs[slot])),
					ed::PinId(InputPin(node.id, slot)),
					ImVec4(0.35f, 0.8f, 0.65f, 1),
					2.5f);
			}
		}
		if (firstLayout)
		{
			for (const DspNodeAsset& node : graph.nodes)
			{
				ed::SetNodePosition(
					ed::NodeId(node.id),
					ImVec2(
						node.editorPosition.x,
						node.editorPosition.y));
			}
			ed::NavigateToContent();
			firstLayout = false;
		}

		const bool creating = ed::BeginCreate();
		if (creating)
		{
			ed::PinId start;
			ed::PinId end;
			if (ed::QueryNewLink(&start, &end))
			{
				PinReference startPin = FindPin(graph, start.Get());
				PinReference endPin = FindPin(graph, end.Get());
				if (startPin.node && endPin.node &&
					startPin.output != endPin.output &&
					startPin.node != endPin.node)
				{
					if (!startPin.output)
					{
						std::swap(startPin, endPin);
						std::swap(start, end);
					}
					if (ed::AcceptNewItem(
							ImVec4(0.35f, 0.8f, 0.65f, 1),
							2.5f))
					{
						DspNodeAsset& target = *endPin.node;
						if (target.type == DspNodeType::Mixer)
						{
							if (endPin.slot == target.inputs.size())
								target.inputs.push_back(startPin.node->id);
							else
								target.inputs[endPin.slot] =
									startPin.node->id;
						}
						else
							target.inputs = {startPin.node->id};
						dirty = true;
					}
				}
				else
					ed::RejectNewItem();
			}
		}
		ed::EndCreate();

		const bool deleting = ed::BeginDelete();
		if (deleting)
		{
			ed::LinkId deletedLink;
			ed::PinId start;
			ed::PinId end;
			while (ed::QueryDeletedLink(
				&deletedLink, &start, &end))
			{
				if (ed::AcceptDeletedItem())
				{
					PinReference input = FindPin(graph, end.Get());
					if (input.node && !input.output &&
						input.slot < input.node->inputs.size())
					{
						input.node->inputs.erase(
							input.node->inputs.begin() + input.slot);
						dirty = true;
					}
				}
			}
			ed::NodeId deletedNode;
			while (ed::QueryDeletedNode(&deletedNode))
			{
				if (ed::AcceptDeletedItem())
				{
					const std::uint64_t id = deletedNode.Get();
					graph.nodes.erase(
						std::remove_if(
							graph.nodes.begin(),
							graph.nodes.end(),
							[id](const DspNodeAsset& node)
							{
								return node.id == id;
							}),
						graph.nodes.end());
					for (DspNodeAsset& node : graph.nodes)
					{
						std::erase(node.inputs, id);
					}
					if (graph.outputNode == id)
						graph.outputNode = 0;
					dirty = true;
				}
			}
		}
		ed::EndDelete();

		ed::Suspend();
		ed::PinId contextPin;
		if (ed::ShowPinContextMenu(&contextPin))
		{
			contextPinId = contextPin.Get();
			ImGui::OpenPopup("DSP Pin Context Menu");
		}
		DrawPinContextMenu(graph, contextPinId, dirty);
		ed::Resume();

		ed::End();
		ImGui::EndChild();
		ImGui::SameLine();

		ImGui::BeginChild("DSP Runtime", ImVec2(0, 0), true);
		ImGui::TextUnformatted("LIVE CALLBACK PARAMETERS");
		ImGui::TextWrapped(
			"These controls use a bounded lock-free SPSC command queue.");
		ImGui::Separator();
		for (DspNodeAsset& node : runtimeNodes)
		{
			if (node.parameters.empty() &&
				node.type != DspNodeType::WavePlayer &&
				node.type != DspNodeType::EventInput)
				continue;
			ImGui::PushID(static_cast<int>(node.id));
			if (ImGui::CollapsingHeader(
				node.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (node.type == DspNodeType::WavePlayer)
				{
					if (parameterUi::DrawAudioClip(
							"Audio Clip",
							node.clip,
							availableClips))
					{
						const auto authoring = std::find_if(
							graph.nodes.begin(),
							graph.nodes.end(),
							[&node](const DspNodeAsset& candidate)
							{
								return candidate.id == node.id;
							});
						if (authoring != graph.nodes.end())
							authoring->clip = node.clip;
						dirty = true;
					}
				}
				if (node.type == DspNodeType::EventInput)
				{
					std::array<char, 128> eventName{};
					const std::size_t length = std::min(
						node.eventName.size(),
						eventName.size() - 1);
					std::memcpy(
						eventName.data(),
						node.eventName.data(),
						length);
					if (ImGui::InputText(
							"Event Name",
							eventName.data(),
							eventName.size()))
					{
						node.eventName = eventName.data();
						const auto authoring = std::find_if(
							graph.nodes.begin(),
							graph.nodes.end(),
							[&node](const DspNodeAsset& candidate)
							{
								return candidate.id == node.id;
							});
						if (authoring != graph.nodes.end())
							authoring->eventName = node.eventName;
						dirty = true;
					}
					if (runtime && ImGui::Button("Post Event"))
						runtime->TriggerEvent(node.eventName);
					ImGui::SameLine();
					ImGui::TextDisabled("compile-time name");
				}
				for (const DspParameterDescriptor& parameter :
					GetDspParameterDescriptors(node.type))
				{
					if (DrawDspParameter(
						node,
						parameter,
						runtime,
						dirty))
					{
						const auto authoring = std::find_if(
							graph.nodes.begin(),
							graph.nodes.end(),
							[&node](const DspNodeAsset& candidate)
							{
								return candidate.id == node.id;
							});
						if (authoring != graph.nodes.end())
						{
							authoring->parameters[
								std::string(parameter.name)] =
								node.parameters[
									std::string(parameter.name)];
						}
					}
					if (!parameter.runtimeMutable)
					{
						ImGui::SameLine();
						ImGui::TextDisabled("compile-time");
					}
				}
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		if (runtime)
		{
			const DspRealtimeStats stats = runtime->GetStats();
			ImGui::Text(
				"Blocks: %llu",
				static_cast<unsigned long long>(stats.processedBlocks));
			ImGui::Text(
				"Frames: %llu",
				static_cast<unsigned long long>(stats.processedFrames));
			ImGui::Text(
				"Dropped commands: %llu",
				static_cast<unsigned long long>(
					stats.droppedParameterCommands));
			ImGui::ProgressBar(
				std::clamp(stats.outputPeak, 0.0f, 1.0f),
				ImVec2(-1, 0),
				"Output peak");
		}
		for (const DspDiagnostic& diagnostic : diagnostics)
		{
			ImGui::TextColored(
				ImVec4(1.0f, 0.35f, 0.3f, 1),
				"Node %llu: %s",
				static_cast<unsigned long long>(diagnostic.nodeId),
				diagnostic.message.c_str());
		}
		ImGui::EndChild();
		ImGui::End();

		if (smoke)
		{
			if (!voice.IsValid() && !play())
				return 2;
			std::array<float, 512> callbackOutput{};
			ma_engine_read_pcm_frames(
				&engine,
				callbackOutput.data(),
				256,
				nullptr);
			if (runtime && frameCount == 2)
				runtime->SetParameter(3, "Cutoff", 1200.0f);
		}

		ImGui::Render();
		int width = 0;
		int height = 0;
		SDL_GL_GetDrawableSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
		if (smoke && ++frameCount >= 8)
			running = false;
	}

	audio.DestroyVoice(voice);
	runtime.reset();
	audio.Shutdown();
	ma_engine_uninit(&engine);
	ed::SetCurrentEditor(nullptr);
	ed::DestroyEditor(nodeEditor);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
