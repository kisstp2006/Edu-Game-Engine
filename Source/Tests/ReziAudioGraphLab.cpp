#include "../ReziAudioGraph.h"
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
#include <cfloat>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ed = ax::NodeEditor;
using namespace EGE::ReziAudio;
namespace blueprint = EGE::BlueprintNodeStyle;
namespace parameterUi = EGE::ReziAudio::TestUi;

namespace
{
	ImVec4 PinColor(GraphPinType type)
	{
		switch (type)
		{
		case GraphPinType::Bool: return ImVec4(0.85f, 0.25f, 0.35f, 1);
		case GraphPinType::Integer: return ImVec4(0.3f, 0.8f, 0.55f, 1);
		case GraphPinType::Float: return ImVec4(0.35f, 0.7f, 1.0f, 1);
		case GraphPinType::Vector2: return ImVec4(0.25f, 0.85f, 0.8f, 1);
		case GraphPinType::Vector3: return ImVec4(1.0f, 0.65f, 0.25f, 1);
		case GraphPinType::Color: return ImVec4(1.0f, 0.35f, 0.8f, 1);
		case GraphPinType::AudioClip:
		case GraphPinType::String: return ImVec4(0.75f, 0.4f, 1.0f, 1);
		case GraphPinType::FloatArray:
			return ImVec4(0.30f, 0.68f, 0.95f, 1);
		case GraphPinType::IntegerArray:
			return ImVec4(0.28f, 0.78f, 0.52f, 1);
		case GraphPinType::AudioClipArray:
			return ImVec4(0.72f, 0.36f, 0.95f, 1);
		case GraphPinType::AudioBuffer:
			return ImVec4(0.35f, 0.9f, 0.55f, 1);
		default: return ImVec4(0.7f, 0.7f, 0.75f, 1);
		}
	}

	ImVec4 NodeColor(const GraphNode& node)
	{
		if (node.type.find("Output") != std::string::npos)
			return ImVec4(0.85f, 0.24f, 0.20f, 1.0f);
		if (node.type == "Audio.RandomOneShot")
			return ImVec4(0.16f, 0.62f, 0.40f, 1.0f);
		if (node.type.find("Parameter") != std::string::npos)
			return ImVec4(0.52f, 0.32f, 0.78f, 1.0f);
		if (node.type.find("Random") != std::string::npos)
			return ImVec4(0.78f, 0.36f, 0.24f, 1.0f);
		return ImVec4(0.16f, 0.48f, 0.68f, 1.0f);
	}

	blueprint::PinShape PinShape(GraphPinType type)
	{
		if (type == GraphPinType::Flow)
			return blueprint::PinShape::Square;
		if (type == GraphPinType::Bool)
			return blueprint::PinShape::Diamond;
		return blueprint::PinShape::Circle;
	}

	bool IsPinConnected(const SoundGraphAsset& graph, std::uint64_t pinId)
	{
		return std::any_of(
			graph.links.begin(),
			graph.links.end(),
			[pinId](const GraphLink& link)
			{
				return link.outputPin == pinId || link.inputPin == pinId;
			});
	}

	struct PinLocation
	{
		GraphNode* node = nullptr;
		GraphPin* pin = nullptr;
		bool output = false;
	};

	PinLocation FindPin(
		SoundGraphAsset& graph,
		std::uint64_t id)
	{
		for (GraphNode& node : graph.nodes)
		{
			for (GraphPin& pin : node.inputs)
			{
				if (pin.id == id)
					return {&node, &pin, false};
			}
			for (GraphPin& pin : node.outputs)
			{
				if (pin.id == id)
					return {&node, &pin, true};
			}
		}
		return {};
	}

	const char* PinTypeName(GraphPinType type)
	{
		switch (type)
		{
		case GraphPinType::Flow: return "Flow";
		case GraphPinType::Bool: return "Bool";
		case GraphPinType::Integer: return "Integer";
		case GraphPinType::Float: return "Float";
		case GraphPinType::Vector2: return "Vector2";
		case GraphPinType::Vector3: return "Vector3";
		case GraphPinType::Color: return "Color";
		case GraphPinType::String: return "String";
		case GraphPinType::AudioBuffer: return "Audio Buffer";
		case GraphPinType::AudioClip: return "Audio Clip";
		case GraphPinType::FloatArray: return "Float Array";
		case GraphPinType::IntegerArray: return "Integer Array";
		case GraphPinType::AudioClipArray: return "Audio Clip Array";
		}
		return "Unknown";
	}

	bool Compatible(GraphPinType from, GraphPinType to)
	{
		return from == to ||
			((from == GraphPinType::Integer ||
			  from == GraphPinType::Float) &&
			 (to == GraphPinType::Integer ||
			  to == GraphPinType::Float));
	}

	bool DrawNode(
		GraphNode& node,
		SoundGraphAsset& graph,
		std::span<const AudioClipReference> availableClips)
	{
		bool changed = false;
		blueprint::NodeBuilder builder(
			node.id,
			node.displayName,
			NodeColor(node),
			220.0f);
		builder.Begin();
		const std::size_t rows =
			std::max(node.inputs.size(), node.outputs.size());
		for (std::size_t row = 0; row < rows; ++row)
		{
			std::optional<blueprint::Pin> input;
			std::optional<blueprint::Pin> output;
			if (row < node.inputs.size())
			{
				const GraphPin& pin = node.inputs[row];
				input = blueprint::Pin{
					pin.id,
					pin.name,
					PinColor(pin.type),
					PinShape(pin.type),
					IsPinConnected(graph, pin.id)};
			}
			if (row < node.outputs.size())
			{
				const GraphPin& pin = node.outputs[row];
				output = blueprint::Pin{
					pin.id,
					pin.name,
					PinColor(pin.type),
					PinShape(pin.type),
					IsPinConnected(graph, pin.id)};
			}
			builder.PinRow(
				input ? &*input : nullptr,
				output ? &*output : nullptr);
		}
		if (auto found = node.properties.find("Name");
			found != node.properties.end())
		{
			if (std::string* name =
					std::get_if<std::string>(&found->second))
			{
				char buffer[96]{};
				strncpy_s(buffer, name->c_str(), _TRUNCATE);
				ImGui::SetNextItemWidth(builder.ContentWidth());
				if (ImGui::InputText("##Parameter", buffer, sizeof(buffer)))
				{
					*name = buffer;
					changed = true;
				}
			}
		}
		if (node.type == "Constant.Clip" &&
			!node.inputs.empty())
		{
			if (AudioClipReference* clip =
					std::get_if<AudioClipReference>(
						&node.inputs.front().defaultValue))
			{
				ImGui::SetNextItemWidth(builder.ContentWidth());
				changed |= parameterUi::DrawAudioClip(
					"##AudioAsset",
					*clip,
					availableClips);
			}
		}
		if (node.type == "Audio.RandomOneShot")
		{
			builder.Text(
				"Random each play - no immediate repeat",
				ImVec4(0.58f, 0.72f, 0.64f, 1.0f));
			if (node.inputs.size() < 32 &&
				ImGui::SmallButton("+ Add Clip Input"))
			{
				const std::size_t number =
					node.inputs.size() + 1;
				node.inputs.push_back(
					{
						NextGraphPinId(graph),
						"Clip " + std::to_string(number),
						GraphPinType::AudioClip,
						AudioClipReference{}
					});
				changed = true;
			}
		}
		builder.End();
		return changed;
	}

	bool DrawParameterEditor(
		const NamedParameter& definition,
		SoundGraphInstance& instance,
		std::span<const AudioClipReference> availableClips)
	{
		ParameterValue value = definition.defaultValue;
		if (const ParameterValue* current =
				instance.GetParameter(definition.name))
			value = *current;
		ImGui::PushID(static_cast<int>(definition.id));
		const bool changed = parameterUi::DrawParameterValue(
			definition.name.c_str(),
			value,
			availableClips);
		if (changed)
			instance.SetParameter(definition.id, value);
		ImGui::PopID();
		return changed;
	}

	void RemoveNode(SoundGraphAsset& graph, std::uint64_t nodeId)
	{
		auto found = std::find_if(
			graph.nodes.begin(),
			graph.nodes.end(),
			[nodeId](const GraphNode& node)
			{
				return node.id == nodeId;
			});
		if (found == graph.nodes.end())
			return;
		std::vector<std::uint64_t> pins;
		for (const GraphPin& pin : found->inputs)
			pins.push_back(pin.id);
		for (const GraphPin& pin : found->outputs)
			pins.push_back(pin.id);
		graph.links.erase(
			std::remove_if(
				graph.links.begin(),
				graph.links.end(),
				[&pins](const GraphLink& link)
				{
					return std::find(
						pins.begin(), pins.end(), link.inputPin) !=
							pins.end() ||
						std::find(
							pins.begin(), pins.end(), link.outputPin) !=
							pins.end();
				}),
			graph.links.end());
		graph.nodes.erase(found);
	}

	bool OverlapsExistingNode(
		const SoundGraphAsset& graph,
		const ImVec2& position,
		const ImVec2& size)
	{
		constexpr float margin = 10.0f;
		for (const GraphNode& node : graph.nodes)
		{
			const ImVec2 existingPosition =
				ed::GetNodePosition(ed::NodeId(node.id));
			const ImVec2 existingSize =
				ed::GetNodeSize(ed::NodeId(node.id));
			if (existingPosition.x == FLT_MAX ||
				existingPosition.y == FLT_MAX)
				continue;
			const bool separated =
				position.x + size.x + margin <
					existingPosition.x ||
				existingPosition.x + existingSize.x + margin <
					position.x ||
				position.y + size.y + margin <
					existingPosition.y ||
				existingPosition.y + existingSize.y + margin <
					position.y;
			if (!separated)
				return true;
		}
		return false;
	}

	ImVec2 FindParameterNodePosition(
		const SoundGraphAsset& graph,
		const ImVec2& ownerPosition,
		float rowOffset)
	{
		constexpr float nodeWidth = 220.0f;
		constexpr float estimatedHeight = 78.0f;
		constexpr float columnSpacing = 270.0f;
		constexpr float rowSpacing = 110.0f;
		const ImVec2 size(nodeWidth, estimatedHeight);

		for (int column = 1; column <= 4; ++column)
		{
			const float x =
				ownerPosition.x - columnSpacing * column;
			for (int distance = 0; distance <= 7; ++distance)
			{
				const int directions =
					distance == 0 ? 1 : 2;
				for (int direction = 0;
					direction < directions;
					++direction)
				{
					const float sign =
						direction == 0 ? -1.0f : 1.0f;
					const float offset =
						distance == 0
							? 0.0f
							: sign * rowSpacing * distance;
					const ImVec2 candidate(
						x,
						std::max(
							8.0f,
							ownerPosition.y +
								rowOffset +
								offset));
					if (!OverlapsExistingNode(
							graph, candidate, size))
						return candidate;
				}
			}
		}
		return ImVec2(
			ownerPosition.x - columnSpacing,
			std::max(8.0f, ownerPosition.y + rowOffset));
	}

	void MarkGraphDirty(
		bool& compiled,
		bool& compileFailed)
	{
		compiled = false;
		compileFailed = false;
	}

	void DrawPinContextMenu(
		SoundGraphAsset& graph,
		const NodeRegistry& registry,
		std::uint64_t contextPinId,
		bool& compiled,
		bool& compileFailed)
	{
		std::optional<std::uint64_t> disconnectLink;
		bool disconnectAll = false;
		bool promote = false;
		bool removeClipInput = false;
		std::optional<std::string> connectParameter;
		std::optional<ImVec2> parameterNodePosition;

		if (ImGui::BeginPopup("Graph Pin Context Menu"))
		{
			const PinLocation location =
				FindPin(graph, contextPinId);
			if (!location.pin || !location.node)
			{
				ImGui::TextDisabled("The pin no longer exists.");
				ImGui::EndPopup();
				return;
			}

			ImGui::TextUnformatted(location.pin->name.c_str());
			ImGui::SameLine();
			ImGui::TextDisabled(
				"(%s %s)",
				location.output ? "Output" : "Input",
				PinTypeName(location.pin->type));
			ImGui::Separator();

			std::vector<const GraphLink*> connections;
			for (const GraphLink& link : graph.links)
			{
				if (link.outputPin == contextPinId ||
					link.inputPin == contextPinId)
				{
					connections.push_back(&link);
				}
			}

			if (!connections.empty())
			{
				if (connections.size() == 1)
				{
					if (ImGui::MenuItem("Disconnect"))
						disconnectLink = connections.front()->id;
				}
				else if (ImGui::BeginMenu("Disconnect connection"))
				{
					for (const GraphLink* link : connections)
					{
						const std::uint64_t otherPinId =
							link->inputPin == contextPinId
								? link->outputPin
								: link->inputPin;
						const PinLocation other =
							FindPin(graph, otherPinId);
						const std::string label =
							other.node && other.pin
								? other.node->displayName + "." +
									other.pin->name
								: "Unknown connection";
						ImGui::PushID(
							static_cast<int>(link->id));
						if (ImGui::MenuItem(label.c_str()))
							disconnectLink = link->id;
						ImGui::PopID();
					}
					ImGui::EndMenu();
				}

				if (connections.size() > 1)
				{
					ImGui::Separator();
					const std::string label =
						"Disconnect all (" +
						std::to_string(connections.size()) + ")";
					if (ImGui::MenuItem(label.c_str()))
						disconnectAll = true;
				}
			}
			else if (!location.output)
			{
				const ImVec2 ownerPosition =
					ed::GetNodePosition(
						ed::NodeId(location.node->id));
				const auto pinIndex = std::find_if(
					location.node->inputs.begin(),
					location.node->inputs.end(),
					[contextPinId](const GraphPin& pin)
					{
						return pin.id == contextPinId;
					});
				const float rowOffset =
					pinIndex == location.node->inputs.end()
						? 0.0f
						: static_cast<float>(
							std::distance(
								location.node->inputs.begin(),
								pinIndex)) *
							24.0f;
				parameterNodePosition =
					FindParameterNodePosition(
						graph,
						ownerPosition,
						rowOffset);

				if (CanPromoteInputToParameter(
						graph, contextPinId))
				{
					if (ImGui::MenuItem("Promote to Variable"))
						promote = true;
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip(
							"Creates a typed graph parameter, "
							"a getter node and this connection.");
					}

					bool hasCompatibleParameter = false;
					for (const NamedParameter& parameter :
						graph.parameters)
					{
						hasCompatibleParameter |=
							CanConnectParameterToInput(
								graph,
								contextPinId,
								parameter.name);
					}
					if (ImGui::BeginMenu(
							"Connect Existing Variable",
							hasCompatibleParameter))
					{
						for (const NamedParameter& parameter :
							graph.parameters)
						{
							if (!CanConnectParameterToInput(
									graph,
									contextPinId,
									parameter.name))
								continue;
							if (ImGui::MenuItem(
									parameter.name.c_str()))
								connectParameter =
									parameter.name;
						}
						ImGui::EndMenu();
					}
				}
				else
				{
					ImGui::TextDisabled(
						"This pin type cannot be stored "
						"as a graph variable.");
				}
			}
			else
			{
				ImGui::TextDisabled(
					"Connect this output to an input.");
			}

			if (!location.output &&
				location.node->type == "Audio.RandomOneShot")
			{
				ImGui::Separator();
				if (location.node->inputs.size() > 2)
				{
					const char* label =
						connections.empty()
							? "Remove Clip Input"
							: "Disconnect and Remove Clip Input";
					if (ImGui::MenuItem(label))
						removeClipInput = true;
				}
				else
				{
					ImGui::TextDisabled(
						"At least two clip inputs are required.");
				}
			}
			ImGui::EndPopup();
		}

		if (removeClipInput)
		{
			const PinLocation location =
				FindPin(graph, contextPinId);
			if (location.node &&
				location.node->type == "Audio.RandomOneShot")
			{
				DisconnectGraphPin(graph, contextPinId);
				std::erase_if(
					location.node->inputs,
					[contextPinId](const GraphPin& pin)
					{
						return pin.id == contextPinId;
					});
				for (std::size_t index = 0;
					index < location.node->inputs.size();
					++index)
				{
					location.node->inputs[index].name =
						"Clip " + std::to_string(index + 1);
				}
				MarkGraphDirty(compiled, compileFailed);
			}
			return;
		}

		if (disconnectAll)
		{
			if (DisconnectGraphPin(graph, contextPinId) > 0)
				MarkGraphDirty(compiled, compileFailed);
		}
		else if (disconnectLink)
		{
			const std::size_t previousSize = graph.links.size();
			std::erase_if(
				graph.links,
				[disconnectLink](const GraphLink& link)
				{
					return link.id == *disconnectLink;
				});
			if (graph.links.size() != previousSize)
				MarkGraphDirty(compiled, compileFailed);
		}

		if (!parameterNodePosition)
			return;
		const float2 position(
			parameterNodePosition->x,
			parameterNodePosition->y);
		std::optional<GraphParameterNodeResult> result;
		if (promote)
		{
			result = PromoteInputToParameter(
				graph,
				registry,
				contextPinId,
				position);
		}
		else if (connectParameter)
		{
			result = ConnectParameterToInput(
				graph,
				registry,
				contextPinId,
				*connectParameter,
				position);
		}
		if (result)
		{
			ed::SetNodePosition(
				ed::NodeId(result->nodeId),
				*parameterNodePosition);
			ed::SelectNode(
				ed::NodeId(result->nodeId),
				false);
			MarkGraphDirty(compiled, compileFailed);
		}
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
		"ReziAudio Graph Lab",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		1280,
		760,
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
	ed::Config editorConfig;
	editorConfig.SettingsFile = nullptr;
	ed::EditorContext* nodeEditor = ed::CreateEditor(&editorConfig);
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

	const std::filesystem::path clipPath =
		argc > 1 && !smoke
		? std::filesystem::path(argv[1])
		: std::filesystem::path(EGE_SOURCE_ROOT) /
			"Game/Assets/Audio/Effects/ding.wav";
	const AudioClipReference defaultClip =
		parameterUi::MakeAudioClipReference(clipPath);
	const std::vector<AudioClipReference> availableClips =
		parameterUi::BuildAudioClipCatalog(clipPath);
	NodeRegistry registry;
	SoundGraphAsset graph =
		CreateDefaultSoundGraph(registry, defaultClip);
	SoundGraphInstance instance;
	bool compiled = instance.Load(graph, registry);
	bool compileFailed = !compiled;
	std::uint64_t contextPinId = 0;
	PlaybackHandle voice;
	AudioTransform listener;
	bool firstLayout = true;
	bool running = true;
	int frameCount = 0;

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
		ImGui::SetNextWindowSize(ImVec2(1264, 744), ImGuiCond_Once);
		ImGui::Begin("ReziAudio Graph Prototype + Runtime Instance");
		if (ImGui::Button("Compile"))
		{
			compiled = instance.Load(graph, registry);
			compileFailed = !compiled;
		}
		ImGui::SameLine();
		if (ImGui::Button("Play Graph") && compiled)
		{
			const SoundGraphEvaluation evaluation = instance.Evaluate();
			if (evaluation.succeeded)
			{
				audio.DestroyVoice(voice);
				voice = audio.CreateVoice(evaluation.voice);
				audio.Play(voice);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop"))
			audio.Stop(voice);
		ImGui::SameLine();
		ImGui::TextColored(
			compiled
				? ImVec4(0.3f, 0.9f, 0.55f, 1)
				: compileFailed
					? ImVec4(1.0f, 0.35f, 0.3f, 1)
					: ImVec4(1.0f, 0.72f, 0.25f, 1),
			compiled
				? "Compiled"
				: compileFailed
					? "Compile failed"
					: "Graph changed - compile required");

		ImGui::BeginChild("Palette", ImVec2(225, 0), true);
		ImGui::TextUnformatted("NODE PALETTE");
		ImGui::TextDisabled("%zu registered nodes", registry.Descriptors().size());
		ImGui::Separator();
		std::string category;
		for (const NodeDescriptor& descriptor : registry.Descriptors())
		{
			if (descriptor.category != category)
			{
				category = descriptor.category;
				ImGui::Spacing();
				ImGui::TextColored(
					ImVec4(0.35f, 0.75f, 1.0f, 1),
					"%s",
					category.c_str());
			}
			ImGui::PushID(descriptor.type.c_str());
			if (ImGui::SmallButton(descriptor.displayName.c_str()))
			{
				std::uint64_t nextPinId =
					NextGraphPinId(graph);
				GraphNode node = registry.CreateNode(
					descriptor.type,
					NextGraphNodeId(graph),
					nextPinId);
				if (node.type == "Constant.Clip" &&
					!node.inputs.empty())
				{
					node.inputs.front().defaultValue =
						defaultClip;
				}
				node.editorPosition = float2(250, 200);
				graph.nodes.push_back(std::move(node));
				MarkGraphDirty(compiled, compileFailed);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::BeginChild("GraphArea", ImVec2(-300, 0), true);
		ed::Begin("ReziAudioNodeEditor");
		bool nodeEdited = false;
		for (GraphNode& node : graph.nodes)
			nodeEdited |= DrawNode(node, graph, availableClips);
		if (nodeEdited)
			MarkGraphDirty(compiled, compileFailed);
		for (const GraphLink& link : graph.links)
			ed::Link(
				ed::LinkId(link.id),
				ed::PinId(link.outputPin),
				ed::PinId(link.inputPin),
				ImVec4(0.35f, 0.75f, 1.0f, 1),
				2.5f);

		if (firstLayout)
		{
			for (const GraphNode& node : graph.nodes)
			{
				ed::SetNodePosition(
					ed::NodeId(node.id),
					ImVec2(node.editorPosition.x, node.editorPosition.y));
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
				PinLocation startPin =
					FindPin(graph, start.Get());
				PinLocation endPin =
					FindPin(graph, end.Get());
				if (startPin.pin && endPin.pin &&
					startPin.output != endPin.output)
				{
					if (!startPin.output)
					{
						std::swap(startPin, endPin);
						std::swap(start, end);
					}
					if (Compatible(
							startPin.pin->type,
							endPin.pin->type))
					{
						if (ed::AcceptNewItem(
								PinColor(startPin.pin->type),
								2.0f))
						{
							graph.links.erase(
								std::remove_if(
									graph.links.begin(),
									graph.links.end(),
									[end](const GraphLink& link)
									{
										return link.inputPin == end.Get();
									}),
								graph.links.end());
							graph.links.push_back({
								NextGraphLinkId(graph),
								start.Get(),
								end.Get()});
							MarkGraphDirty(
								compiled,
								compileFailed);
						}
					}
					else
						ed::RejectNewItem(
							ImVec4(1, 0.25f, 0.25f, 1), 2.0f);
				}
				else
					ed::RejectNewItem();
			}
		}
		ed::EndCreate();
		const bool deleting = ed::BeginDelete();
		if (deleting)
		{
			ed::LinkId linkId;
			while (ed::QueryDeletedLink(&linkId))
			{
				if (ed::AcceptDeletedItem())
				{
					graph.links.erase(
						std::remove_if(
						graph.links.begin(),
						graph.links.end(),
						[linkId](const GraphLink& link)
						{
							return link.id == linkId.Get();
						}),
					graph.links.end());
					MarkGraphDirty(
						compiled,
						compileFailed);
				}
			}
			ed::NodeId nodeId;
			while (ed::QueryDeletedNode(&nodeId))
			{
				if (ed::AcceptDeletedItem())
				{
					RemoveNode(graph, nodeId.Get());
					MarkGraphDirty(
						compiled,
						compileFailed);
				}
			}
		}
		ed::EndDelete();

		ed::Suspend();
		ed::PinId contextPin;
		if (ed::ShowPinContextMenu(&contextPin))
		{
			contextPinId = contextPin.Get();
			ImGui::OpenPopup("Graph Pin Context Menu");
		}
		DrawPinContextMenu(
			graph,
			registry,
			contextPinId,
			compiled,
			compileFailed);
		ed::Resume();

		ed::End();
		ImGui::EndChild();
		ImGui::EndGroup();
		ImGui::SameLine();

		ImGui::BeginChild("Runtime", ImVec2(0, 0), true);
		ImGui::TextUnformatted("RUNTIME INSTANCE");
		ImGui::PushTextWrapPos();
		ImGui::TextDisabled(
			"Values update without recompiling the graph.");
		ImGui::PopTextWrapPos();
		ImGui::Separator();
		const auto addParameter = [&](
			const ParameterTypeDescriptor& descriptor)
		{
			std::size_t suffix = graph.parameters.size() + 1;
			std::string name;
			do
			{
				name = std::string(descriptor.defaultName) +
					std::to_string(suffix++);
			}
			while (std::any_of(
				graph.parameters.begin(),
				graph.parameters.end(),
				[&name](const NamedParameter& parameter)
				{
					return parameter.name == name;
				}));
			graph.parameters.push_back(
				{name, HashAudioParameter(name), descriptor.defaultValue});
			MarkGraphDirty(compiled, compileFailed);
		};
		if (ImGui::Button("+ Add Variable"))
			ImGui::OpenPopup("Add Graph Variable");
		if (ImGui::BeginPopup("Add Graph Variable"))
		{
			for (const ParameterTypeDescriptor& descriptor :
				GetParameterTypeDescriptors())
			{
				if (!descriptor.availableInAudioGraph)
					continue;
				if (ImGui::MenuItem(
						std::string(descriptor.displayName).c_str()))
				{
					addParameter(descriptor);
				}
			}
			ImGui::EndPopup();
		}
		ImGui::TextDisabled(
			"New variables become active after Compile.");
		ImGui::Separator();
		ImGui::TextUnformatted("GRAPH VARIABLES");
		std::optional<std::size_t> removeParameter;
		for (std::size_t index = 0;
			index < graph.parameters.size();
			++index)
		{
			const NamedParameter& parameter = graph.parameters[index];
			ImGui::PushID(static_cast<int>(index + 100000));
			ImGui::TextUnformatted(parameter.name.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("x##RemoveGraphVariable"))
				removeParameter = index;
			ImGui::PopID();
		}
		if (removeParameter)
		{
			graph.parameters.erase(
				graph.parameters.begin() + *removeParameter);
			MarkGraphDirty(compiled, compileFailed);
		}
		ImGui::Separator();
		ImGui::TextUnformatted("COMPILED RUNTIME PARAMETERS");
		if (!compiled)
			ImGui::TextDisabled(
				"Showing the last compiled parameter set.");
		bool runtimeChanged = false;
		for (const NamedParameter& parameter :
			instance.Parameters().Definitions())
		{
			runtimeChanged |= DrawParameterEditor(
				parameter,
				instance,
				availableClips);
		}
		if (runtimeChanged && compiled && voice.IsValid())
		{
			const SoundGraphEvaluation evaluation = instance.Evaluate();
			if (evaluation.succeeded)
			{
				audio.SetSettings(voice, evaluation.voice.settings);
				audio.SetTransform(voice, evaluation.voice.transform);
			}
		}
		audio.SetListener(listener);
		ImGui::Separator();
		ImGui::Text("Nodes: %zu", graph.nodes.size());
		ImGui::Text("Links: %zu", graph.links.size());
		ImGui::Text(
			"Voice: %d",
			static_cast<int>(audio.GetState(voice)));
		ImGui::Separator();
		const auto& diagnostics =
			compiled
			? instance.Prototype().diagnostics
			: SoundGraphCompiler::Compile(graph, registry).diagnostics;
		if (diagnostics.empty())
			ImGui::TextColored(
				ImVec4(0.3f, 0.9f, 0.55f, 1),
				"No diagnostics.");
		for (const GraphDiagnostic& diagnostic : diagnostics)
		{
			ImGui::TextWrapped(
				"[%llu] %s",
				static_cast<unsigned long long>(diagnostic.nodeId),
				diagnostic.message.c_str());
		}
		ImGui::EndChild();
		ImGui::End();

		if (smoke && frameCount == 0 && compiled)
		{
			const SoundGraphEvaluation evaluation = instance.Evaluate();
			if (!evaluation.succeeded)
				return 2;
			voice = audio.CreateVoice(evaluation.voice);
			audio.Play(voice);
			instance.SetParameter("Volume", 0.25f);
		}

		ImGui::Render();
		int width = 0;
		int height = 0;
		SDL_GL_GetDrawableSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.025f, 0.035f, 0.055f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
		if (smoke && ++frameCount >= 6)
			running = false;
	}

	audio.DestroyVoice(voice);
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
