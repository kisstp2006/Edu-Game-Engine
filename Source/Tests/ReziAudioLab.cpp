#include "../ReziAudioSystem.h"

#define SDL_MAIN_HANDLED
#include <GL/glew.h>
#include <SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl.h>
#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace
{
	void DrawVector3(const char* label, float3& value)
	{
		float data[3] = {value.x, value.y, value.z};
		if (ImGui::DragFloat3(label, data, 0.05f))
			value = float3(data[0], data[1], data[2]);
	}
}

int main(int argc, char** argv)
{
	const bool smokeTest =
		argc > 1 && std::string(argv[1]) == "--smoke";
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
		return 1;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(
		SDL_GL_CONTEXT_PROFILE_MASK,
		SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_Window* window = SDL_CreateWindow(
		"ReziAudio Lab",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		920,
		620,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
			(smokeTest ? SDL_WINDOW_HIDDEN : 0));
	if (!window)
		return 1;

	SDL_GLContext context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, context);
	SDL_GL_SetSwapInterval(1);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
		return 1;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(window, context);
	ImGui_ImplOpenGL3_Init("#version 330 core");

	ma_engine engine{};
	ma_engine_config engineConfig = ma_engine_config_init();
	engineConfig.listenerCount = 1;
	if (smokeTest)
	{
		engineConfig.noDevice = MA_TRUE;
		engineConfig.channels = 2;
		engineConfig.sampleRate = 48000;
	}
	if (ma_engine_init(&engineConfig, &engine) != MA_SUCCESS)
		return 1;

	EGE::ReziAudio::System audio;
	if (!audio.Initialize(engine))
		return 1;

	std::filesystem::path audioPath = argc > 1 && !smokeTest
		? std::filesystem::path(argv[1])
		: std::filesystem::path(EGE_SOURCE_ROOT) /
			"Game/Assets/Audio/Effects/ding.wav";

	EGE::ReziAudio::VoiceSettings settings;
	settings.spatial.enabled = true;
	settings.looping = true;
	EGE::ReziAudio::AudioTransform sourceTransform;
	sourceTransform.position = float3(3.0f, 0.0f, 0.0f);
	EGE::ReziAudio::AudioTransform listenerTransform;
	EGE::ReziAudio::PlaybackHandle voice;
	bool orbit = false;
	float orbitAngle = 0.0f;
	bool running = true;
	int renderedFrames = 0;

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

		if (smokeTest && !voice.IsValid())
		{
			EGE::ReziAudio::VoiceCreateInfo createInfo;
			createInfo.filePath = audioPath.string();
			createInfo.settings = settings;
			createInfo.transform = sourceTransform;
			voice = audio.CreateVoice(createInfo);
			audio.Play(voice);
		}

		ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(888, 588), ImGuiCond_Once);
		ImGui::Begin("ReziAudio Backend + 3D Component Lab");
		ImGui::TextWrapped("Clip: %s", audioPath.string().c_str());
		if (ImGui::Button("Create / Play"))
		{
			if (!voice.IsValid())
			{
				EGE::ReziAudio::VoiceCreateInfo createInfo;
				createInfo.filePath = audioPath.string();
				createInfo.settings = settings;
				createInfo.transform = sourceTransform;
				voice = audio.CreateVoice(createInfo);
			}
			audio.Play(voice);
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause"))
			audio.Pause(voice);
		ImGui::SameLine();
		if (ImGui::Button("Stop"))
			audio.Stop(voice);
		ImGui::SameLine();
		if (ImGui::Button("Release"))
			audio.DestroyVoice(voice);

		ImGui::Separator();
		ImGui::Checkbox("Loop", &settings.looping);
		ImGui::Checkbox("3D Spatialization", &settings.spatial.enabled);
		ImGui::SliderFloat("Volume", &settings.volume, 0.0f, 2.0f);
		ImGui::SliderFloat("Pitch", &settings.pitch, 0.1f, 4.0f);
		ImGui::SliderFloat("Pan", &settings.pan, -1.0f, 1.0f);
		ImGui::DragFloat(
			"Min Distance",
			&settings.spatial.minDistance,
			0.1f,
			0.0f,
			1000.0f);
		ImGui::DragFloat(
			"Max Distance",
			&settings.spatial.maxDistance,
			0.1f,
			settings.spatial.minDistance,
			1000.0f);
		ImGui::SliderFloat(
			"Rolloff", &settings.spatial.rolloff, 0.0f, 10.0f);
		ImGui::SliderFloat(
			"Doppler",
			&settings.spatial.dopplerFactor,
			0.0f,
			10.0f);
		ImGui::Checkbox("Orbit source around listener", &orbit);
		DrawVector3("Source Position", sourceTransform.position);
		DrawVector3("Source Velocity", sourceTransform.velocity);
		DrawVector3("Listener Position", listenerTransform.position);

		if (orbit)
		{
			orbitAngle += ImGui::GetIO().DeltaTime;
			sourceTransform.position = float3(
				std::cos(orbitAngle) * 4.0f,
				0.0f,
				std::sin(orbitAngle) * 4.0f);
			sourceTransform.velocity = float3(
				-std::sin(orbitAngle) * 4.0f,
				0.0f,
				std::cos(orbitAngle) * 4.0f);
		}

		audio.SetSettings(voice, settings);
		audio.SetTransform(voice, sourceTransform);
		audio.SetListener(listenerTransform);

		const auto stats = audio.GetStats();
		ImGui::Separator();
		ImGui::Text(
			"Voice state: %d | Active: %zu | Capacity: %zu",
			static_cast<int>(audio.GetState(voice)),
			stats.activeVoices,
			stats.voiceCapacity);
		ImGui::TextDisabled(
			"This lab drives the same backend used by "
			"ReziAudioEmitter and ReziAudioListener.");
		ImGui::End();

		ImGui::Render();
		int width = 0;
		int height = 0;
		SDL_GL_GetDrawableSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.035f, 0.045f, 0.065f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
		++renderedFrames;
		if (smokeTest && renderedFrames >= 6)
			running = false;
	}

	audio.DestroyVoice(voice);
	audio.Shutdown();
	ma_engine_uninit(&engine);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
