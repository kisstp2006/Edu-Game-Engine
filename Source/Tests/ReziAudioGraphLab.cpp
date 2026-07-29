#include "../ReziAudioGraph.h"
#include "../ReziAudioSystem.h"

#define SDL_MAIN_HANDLED
#include <GL/glew.h>
#include <SDL.h>
#include <imgui.h>
#include <imgui_node_editor.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl.h>
#include <miniaudio.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>

namespace ed = ax::NodeEditor;
using namespace EGE::ReziAudio;

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
		default: return ImVec4(0.7f, 0.7f, 0.75f, 1);
		}
	}

	GraphPin* FindPin(
		SoundGraphAsset& graph,
		std::uint64_t id,
		bool& output)
	{
		for (GraphNode& node : graph.nodes)
		{
			for (GraphPin& pin : node.inputs)
			{
				if (pin.id == id)
				{
					output = false;
					return &pin;
				}
			}
			for (GraphPin& pin : node.outputs)
			{
				if (pin.id == id)
				{
					output = true;
					return &pin;
				}
			}
		}
		return nullptr;
	}

	bool Compatible(GraphPinType from, GraphPinType to)
	{
		return from == to ||
			((from == GraphPinType::Integer ||
			  from == GraphPinType::Float) &&
			 (to == GraphPinType::Integer ||
			  to == GraphPinType::Float)) ||
			((from == GraphPinType::AudioClip ||
			  from == GraphPinType::String) &&
			 (to == GraphPinType::AudioClip ||
			  to == GraphPinType::String));
	}

	void DrawPin(const GraphPin& pin, bool output)
	{
		ed::BeginPin(
			ed::PinId(pin.id),
			output ? ed::PinKind::Output : ed::PinKind::Input);
		const ImVec4 color = PinColor(pin.type);
		ImGui::TextColored(
			color,
			output ? "%s  >" : "<  %s",
			pin.name.c_str());
		ed::EndPin();
	}

	void DrawNode(GraphNode& node)
	{
		ed::BeginNode(ed::NodeId(node.id));
		ImGui::PushID(static_cast<int>(node.id));
		ImGui::TextColored(
			ImVec4(0.35f, 0.8f, 1.0f, 1.0f),
			"%s",
			node.displayName.c_str());
		ImGui::Separator();
		const std::size_t rows =
			std::max(node.inputs.size(), node.outputs.size());
		for (std::size_t row = 0; row < rows; ++row)
		{
			if (row < node.inputs.size())
				DrawPin(node.inputs[row], false);
			else
				ImGui::Dummy(ImVec2(20, ImGui::GetTextLineHeight()));
			if (row < node.outputs.size())
			{
				ImGui::SameLine(150.0f);
				DrawPin(node.outputs[row], true);
			}
		}
		if (auto found = node.properties.find("Name");
			found != node.properties.end())
		{
			if (std::string* name =
					std::get_if<std::string>(&found->second))
			{
				char buffer[96]{};
				strncpy_s(buffer, name->c_str(), _TRUNCATE);
				ImGui::SetNextItemWidth(180.0f);
				if (ImGui::InputText("Parameter", buffer, sizeof(buffer)))
					*name = buffer;
			}
		}
		ImGui::PopID();
		ed::EndNode();
	}

	bool DrawParameterEditor(
		NamedParameter& definition,
		SoundGraphInstance& instance)
	{
		ParameterValue value = definition.defaultValue;
		if (const ParameterValue* current =
				instance.GetParameter(definition.name))
			value = *current;
		bool changed = false;
		ImGui::PushID(static_cast<int>(definition.id));
		if (float* number = std::get_if<float>(&value))
			changed = ImGui::DragFloat(
				definition.name.c_str(), number, 0.01f);
		else if (int* number = std::get_if<int>(&value))
			changed = ImGui::DragInt(definition.name.c_str(), number);
		else if (bool* enabled = std::get_if<bool>(&value))
			changed = ImGui::Checkbox(definition.name.c_str(), enabled);
		else if (float3* vector = std::get_if<float3>(&value))
		{
			float data[3] = {vector->x, vector->y, vector->z};
			changed = ImGui::DragFloat3(
				definition.name.c_str(), data, 0.05f);
			if (changed)
				*vector = float3(data[0], data[1], data[2]);
		}
		else if (float2* vector = std::get_if<float2>(&value))
		{
			float data[2] = {vector->x, vector->y};
			changed = ImGui::DragFloat2(
				definition.name.c_str(), data, 0.05f);
			if (changed)
				*vector = float2(data[0], data[1]);
		}
		else if (float4* color = std::get_if<float4>(&value))
		{
			float data[4] = {color->x, color->y, color->z, color->w};
			changed = ImGui::ColorEdit4(definition.name.c_str(), data);
			if (changed)
				*color = float4(data[0], data[1], data[2], data[3]);
		}
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
	NodeRegistry registry;
	SoundGraphAsset graph =
		CreateDefaultSoundGraph(registry, clipPath.string());
	SoundGraphInstance instance;
	bool compiled = instance.Load(graph, registry);
	std::uint64_t nextNodeId = 100;
	std::uint64_t nextPinId = 10000;
	std::uint64_t nextLinkId = 20000;
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
			compiled = instance.Load(graph, registry);
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
				: ImVec4(1.0f, 0.35f, 0.3f, 1),
			compiled ? "Compiled" : "Compile failed");

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
				GraphNode node = registry.CreateNode(
					descriptor.type, nextNodeId++, nextPinId);
				node.editorPosition = float2(250, 200);
				graph.nodes.push_back(std::move(node));
				compiled = false;
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::BeginChild("GraphArea", ImVec2(-300, 0), true);
		ed::Begin("ReziAudioNodeEditor");
		for (GraphNode& node : graph.nodes)
			DrawNode(node);
		for (const GraphLink& link : graph.links)
			ed::Link(
				ed::LinkId(link.id),
				ed::PinId(link.outputPin),
				ed::PinId(link.inputPin),
				ImVec4(0.35f, 0.75f, 1.0f, 1),
				2.0f);

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
				bool startOutput = false;
				bool endOutput = false;
				GraphPin* startPin =
					FindPin(graph, start.Get(), startOutput);
				GraphPin* endPin =
					FindPin(graph, end.Get(), endOutput);
				if (startPin && endPin && startOutput != endOutput)
				{
					if (!startOutput)
					{
						std::swap(startPin, endPin);
						std::swap(start, end);
					}
					if (Compatible(startPin->type, endPin->type))
					{
						if (ed::AcceptNewItem(
								PinColor(startPin->type), 2.0f))
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
								nextLinkId++, start.Get(), end.Get()});
							compiled = false;
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
				graph.links.erase(
					std::remove_if(
						graph.links.begin(),
						graph.links.end(),
						[linkId](const GraphLink& link)
						{
							return link.id == linkId.Get();
						}),
					graph.links.end());
				compiled = false;
			}
			ed::NodeId nodeId;
			while (ed::QueryDeletedNode(&nodeId))
			{
				if (ed::AcceptDeletedItem())
				{
					RemoveNode(graph, nodeId.Get());
					compiled = false;
				}
			}
		}
		ed::EndDelete();
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
			const char* prefix,
			ParameterValue value)
		{
			std::size_t suffix = graph.parameters.size() + 1;
			std::string name;
			do
			{
				name = std::string(prefix) + std::to_string(suffix++);
			}
			while (std::any_of(
				graph.parameters.begin(),
				graph.parameters.end(),
				[&name](const NamedParameter& parameter)
				{
					return parameter.name == name;
				}));
			graph.parameters.push_back(
				{name, HashAudioParameter(name), std::move(value)});
			compiled = false;
		};
		if (ImGui::SmallButton("+ Float"))
			addParameter("Float", 0.0f);
		ImGui::SameLine();
		if (ImGui::SmallButton("+ Int"))
			addParameter("Int", 0);
		ImGui::SameLine();
		if (ImGui::SmallButton("+ Bool"))
			addParameter("Bool", false);
		ImGui::SameLine();
		if (ImGui::SmallButton("+ Vector"))
			addParameter("Vector", float3::zero);
		if (ImGui::SmallButton("+ Vector2"))
			addParameter("Vector2", float2::zero);
		ImGui::SameLine();
		if (ImGui::SmallButton("+ Color"))
			addParameter(
				"Color", float4(1.0f, 1.0f, 1.0f, 1.0f));
		ImGui::TextDisabled(
			"New variables become active after Compile.");
		bool runtimeChanged = false;
		std::optional<std::size_t> removeParameter;
		for (std::size_t index = 0;
			index < graph.parameters.size();
			++index)
		{
			NamedParameter& parameter = graph.parameters[index];
			runtimeChanged |= DrawParameterEditor(parameter, instance);
			ImGui::SameLine();
			ImGui::PushID(static_cast<int>(index + 100000));
			if (ImGui::SmallButton("x"))
				removeParameter = index;
			ImGui::PopID();
		}
		if (removeParameter)
		{
			graph.parameters.erase(
				graph.parameters.begin() + *removeParameter);
			compiled = false;
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
