#include "MaterialPreviewRenderer.h"

#include "Application.h"
#include "ComponentCamera.h"
#include "IBLData.h"
#include "ModuleLevelManager.h"
#include "ModuleResources.h"
#include "OGL.h"
#include "OpenGL.h"
#include "ResourceMaterial.h"
#include "ResourceMesh.h"
#include "ResourceTexture.h"

#include <algorithm>
#include <cmath>

namespace
{
	const char* VertexSource = R"GLSL(
#version 330 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 5) in vec4 Tangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 WorldPosition;
out vec3 WorldNormal;
out vec3 WorldTangent;
out vec2 UV;

void main()
{
	vec4 world = uModel * vec4(Position, 1.0);
	WorldPosition = world.xyz;
	WorldNormal = normalize(mat3(uModel) * Normal);
	WorldTangent = normalize(mat3(uModel) * Tangent.xyz);
	UV = TexCoord;
	gl_Position = uProjection * uView * world;
}
)GLSL";

	const char* FragmentSource = R"GLSL(
#version 330 core

in vec3 WorldPosition;
in vec3 WorldNormal;
in vec3 WorldTangent;
in vec2 UV;

layout(location = 0) out vec4 FragColor;

uniform vec3 uCameraPosition;
uniform vec4 uBaseColor;
uniform vec3 uSpecularColor;
uniform vec3 uEmissiveColor;
uniform float uMetalness;
uniform float uRoughness;
uniform float uNormalStrength;
uniform float uOcclusionStrength;
uniform int uWorkflow;
uniform vec2 uTiling;
uniform vec2 uOffset;

uniform bool uHasBaseColor;
uniform bool uHasSurface;
uniform bool uHasNormal;
uniform bool uHasOcclusion;
uniform bool uHasEmissive;
uniform bool uHasEnvironment;

uniform sampler2D uBaseColorMap;
uniform sampler2D uSurfaceMap;
uniform sampler2D uNormalMap;
uniform sampler2D uOcclusionMap;
uniform sampler2D uEmissiveMap;
uniform samplerCube uEnvironment;

const float PI = 3.14159265359;

float DistributionGGX(vec3 normal, vec3 halfway, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float nDotH = max(dot(normal, halfway), 0.0);
	float denominator =
		nDotH * nDotH * (alpha2 - 1.0) + 1.0;
	return alpha2 / max(PI * denominator * denominator, 0.0001);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	return nDotV / max(nDotV * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(
	vec3 normal, vec3 viewDirection, vec3 lightDirection,
	float roughness)
{
	return GeometrySchlickGGX(
			max(dot(normal, viewDirection), 0.0), roughness) *
		GeometrySchlickGGX(
			max(dot(normal, lightDirection), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
	return f0 + (1.0 - f0) *
		pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
	vec2 uv = UV * uTiling + uOffset;
	vec4 base = uBaseColor;
	if (uHasBaseColor)
		base *= texture(uBaseColorMap, uv);

	float metalness = clamp(uMetalness, 0.0, 1.0);
	float roughness = clamp(uRoughness, 0.045, 1.0);
	vec3 specularColor = uSpecularColor;
	if (uHasSurface)
	{
		vec4 surface = texture(uSurfaceMap, uv);
		if (uWorkflow == 0)
		{
			metalness *= surface.b;
			roughness *= surface.g;
		}
		else
		{
			specularColor *= surface.rgb;
			roughness *= 1.0 - surface.a;
		}
	}

	vec3 normal = normalize(WorldNormal);
	if (!gl_FrontFacing)
		normal = -normal;
	if (uHasNormal)
	{
		vec3 tangent = normalize(
			WorldTangent - normal * dot(WorldTangent, normal));
		vec3 bitangent = normalize(cross(normal, tangent));
		vec3 mapped = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
		mapped.xy *= uNormalStrength;
		normal = normalize(
			mat3(tangent, bitangent, normal) * mapped);
	}

	vec3 viewDirection =
		normalize(uCameraPosition - WorldPosition);
	vec3 lightDirection = normalize(vec3(0.45, 0.75, 0.55));
	vec3 halfway = normalize(viewDirection + lightDirection);
	vec3 radiance = vec3(4.5, 4.2, 3.8);

	vec3 f0 = uWorkflow == 0
		? mix(vec3(0.04), base.rgb, metalness)
		: clamp(specularColor, 0.0, 1.0);
	vec3 fresnel = FresnelSchlick(
		max(dot(halfway, viewDirection), 0.0), f0);
	float distribution =
		DistributionGGX(normal, halfway, roughness);
	float geometry =
		GeometrySmith(
			normal, viewDirection, lightDirection, roughness);
	vec3 specular =
		(distribution * geometry * fresnel) /
		max(
			4.0 * max(dot(normal, viewDirection), 0.0) *
			max(dot(normal, lightDirection), 0.0),
			0.0001);

	vec3 kS = fresnel;
	vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
	float nDotL = max(dot(normal, lightDirection), 0.0);
	vec3 color =
		(kD * base.rgb / PI + specular) * radiance * nDotL;

	float occlusion = 1.0;
	if (uHasOcclusion)
	{
		float sampledOcclusion = texture(uOcclusionMap, uv).r;
		occlusion = mix(
			1.0, sampledOcclusion,
			clamp(uOcclusionStrength, 0.0, 1.0));
	}

	if (uHasEnvironment)
	{
		vec3 reflection = reflect(-viewDirection, normal);
		vec3 diffuseEnvironment =
			textureLod(uEnvironment, normal, 5.0).rgb;
		vec3 specularEnvironment =
			textureLod(
				uEnvironment, reflection, roughness * 6.0).rgb;
		vec3 environmentFresnel = FresnelSchlick(
			max(dot(normal, viewDirection), 0.0), f0);
		color +=
			(kD * diffuseEnvironment * base.rgb * 0.45 +
			 specularEnvironment * environmentFresnel * 0.65) *
			occlusion;
	}
	else
	{
		color += base.rgb * 0.055 * occlusion;
	}

	vec3 emissive = uEmissiveColor;
	if (uHasEmissive)
		emissive *= texture(uEmissiveMap, uv).rgb;
	color += emissive;

	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0 / 2.2));
	FragColor = vec4(color, base.a);
}
)GLSL";

	const Texture* GetTexture(const ResourceTexture* resource)
	{
		return resource ? resource->GetTexture() : nullptr;
	}

	void RestoreCapability(unsigned int capability, bool enabled)
	{
		if (enabled)
			glEnable(capability);
		else
			glDisable(capability);
	}

	float SmoothDamp(
		float current,
		float target,
		float& velocity,
		float smoothTime,
		float deltaTime)
	{
		if (deltaTime <= 0.0f)
			return current;

		smoothTime = std::max(0.0001f, smoothTime);
		const float omega = 2.0f / smoothTime;
		const float step = omega * deltaTime;
		const float decay =
			1.0f /
			(1.0f + step + 0.48f * step * step +
				0.235f * step * step * step);
		const float difference = current - target;
		const float movement =
			(velocity + omega * difference) * deltaTime;

		velocity = (velocity - omega * movement) * decay;
		const float result =
			target + (difference + movement) * decay;

		const bool targetIsAhead = target > current;
		const bool movedPastTarget = result > target;
		if (targetIsAhead == movedPastTarget)
		{
			velocity = 0.0f;
			return target;
		}

		return result;
	}
}

namespace EGE
{
	MaterialPreviewRenderer::MaterialPreviewRenderer() = default;
	MaterialPreviewRenderer::~MaterialPreviewRenderer() = default;

	bool MaterialPreviewRenderer::Initialize()
	{
		if (initialized_)
			return true;
		if (initializationFailed_)
			return false;

		color_ = std::make_unique<Texture2D>(
			PreviewSize, PreviewSize,
			GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr, false);
		depth_ = std::make_unique<Texture2D>(
			PreviewSize, PreviewSize,
			GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT,
			GL_FLOAT, nullptr, false);
		framebuffer_ = std::make_unique<Framebuffer>();
		framebuffer_->AttachColor(color_.get());
		framebuffer_->AttachDepthStencil(
			depth_.get(), GL_DEPTH_ATTACHMENT);

		const char* vertexSources[] = {VertexSource};
		const char* fragmentSources[] = {FragmentSource};
		std::unique_ptr<Shader> vertex =
			std::make_unique<Shader>(
				GL_VERTEX_SHADER, vertexSources, 1);
		std::unique_ptr<Shader> fragment =
			std::make_unique<Shader>(
				GL_FRAGMENT_SHADER, fragmentSources, 1);
		if (!vertex->Compiled() || !fragment->Compiled())
		{
			initializationFailed_ = true;
			return false;
		}

		materialProgram_ = std::make_unique<Program>(
			vertex.get(), fragment.get(), "Material preview");
		initialized_ =
			materialProgram_->Linked() &&
			framebuffer_->Check() == GL_FRAMEBUFFER_COMPLETE;
		initializationFailed_ = !initialized_;
		return initialized_;
	}

	void MaterialPreviewRenderer::DrawShapeButton(
		const char* label,
		Shape shape,
		float height)
	{
		const bool selected = shape_ == shape;
		if (selected)
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				ImVec4(0.20f, 0.58f, 0.95f, 0.75f));
		if (ImGui::Button(
				label,
				ImVec2(
					ImGui::CalcTextSize(label).x +
						ImGui::GetStyle().FramePadding.x * 2.0f,
					height)))
			shape_ = shape;
		if (selected)
			ImGui::PopStyleColor();
	}

	void MaterialPreviewRenderer::Draw(
		ResourceMaterial& material,
		const ImVec2& requestedSize)
	{
		const float controlHeight = ImGui::GetFrameHeight();
		const ImGuiStyle& style = ImGui::GetStyle();
		bool firstControl = true;
		auto placeControl = [&](float width)
		{
			if (!firstControl &&
				ImGui::GetCursorPosX() + style.ItemSpacing.x + width <=
					ImGui::GetWindowContentRegionMax().x)
			{
				ImGui::SameLine();
			}
			firstControl = false;
		};

		const auto buttonWidth = [&](const char* label)
		{
			return ImGui::CalcTextSize(label).x +
				style.FramePadding.x * 2.0f;
		};
		placeControl(buttonWidth("Sphere"));
		DrawShapeButton("Sphere", Shape::Sphere, controlHeight);
		placeControl(buttonWidth("Cube"));
		DrawShapeButton("Cube", Shape::Cube, controlHeight);
		const float skyboxWidth = controlHeight +
			style.ItemInnerSpacing.x +
			ImGui::CalcTextSize("Skybox").x;
		placeControl(skyboxWidth);
		ImGui::Checkbox("Skybox", &showSkybox_);
		placeControl(buttonWidth("Reset view"));
		if (ImGui::Button(
				"Reset view",
				ImVec2(buttonWidth("Reset view"), controlHeight)))
		{
			yaw_ = targetYaw_ = -0.55f;
			pitch_ = targetPitch_ = 0.18f;
			distance_ = targetDistance_ = 3.2f;
			yawVelocity_ = 0.0f;
			pitchVelocity_ = 0.0f;
			distanceVelocity_ = 0.0f;
		}

		const float side =
			std::max(64.0f, std::min(requestedSize.x, requestedSize.y));
		if (!Initialize())
		{
			ImGui::Dummy(ImVec2(side, side));
			const ImVec2 minimum = ImGui::GetItemRectMin();
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(minimum.x + 12.0f, minimum.y + 12.0f),
				ImGui::GetColorU32(ImGuiCol_TextDisabled),
				"Material preview could not be initialized.");
			return;
		}

		const ImVec2 previewSize(side, side);
		ImGui::InvisibleButton(
			"##MaterialPreviewOrbit",
			previewSize,
			ImGuiButtonFlags_MouseButtonLeft |
			ImGuiButtonFlags_MouseButtonRight);
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		ImGuiIO& io = ImGui::GetIO();

		if (active &&
			ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
		{
			targetYaw_ += io.MouseDelta.x * 0.0085f;
			targetPitch_ = std::clamp(
				targetPitch_ - io.MouseDelta.y * 0.0085f,
				-1.30f, 1.30f);
		}
		if (active &&
			ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
		{
			targetDistance_ = std::clamp(
				targetDistance_ *
					std::exp(io.MouseDelta.y * 0.010f),
				2.0f, 6.0f);
		}
		if (hovered && io.MouseWheel != 0.0f)
		{
			targetDistance_ = std::clamp(
				targetDistance_ *
					std::pow(0.86f, io.MouseWheel),
				2.0f, 6.0f);
		}
		if (hovered &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			targetYaw_ = -0.55f;
			targetPitch_ = 0.18f;
			targetDistance_ = 3.2f;
			yawVelocity_ = 0.0f;
			pitchVelocity_ = 0.0f;
			distanceVelocity_ = 0.0f;
		}

		constexpr float TwoPi = 6.28318530718f;
		const float deltaTime =
			std::clamp(io.DeltaTime, 0.0f, 1.0f / 30.0f);
		const float closestTargetYaw =
			yaw_ + std::remainder(targetYaw_ - yaw_, TwoPi);

		yaw_ = SmoothDamp(
			yaw_, closestTargetYaw, yawVelocity_, 0.065f, deltaTime);
		pitch_ = SmoothDamp(
			pitch_, targetPitch_, pitchVelocity_, 0.065f, deltaTime);
		distance_ = SmoothDamp(
			distance_,
			targetDistance_,
			distanceVelocity_,
			0.085f,
			deltaTime);

		Render(material);
		const ImVec2 previewMinimum = ImGui::GetItemRectMin();
		const ImVec2 previewMaximum = ImGui::GetItemRectMax();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddImage(
			reinterpret_cast<ImTextureID>(
				static_cast<size_t>(color_->Id())),
			previewMinimum,
			previewMaximum,
			ImVec2(0.0f, 1.0f),
			ImVec2(1.0f, 0.0f));
		drawList->AddRect(
			previewMinimum,
			previewMaximum,
			ImGui::GetColorU32(ImGuiCol_Border),
			6.0f);

		const char* controls = side >= 340.0f
			? "LMB orbit  |  RMB/wheel zoom  |  Double-click reset"
			: side >= 275.0f
				? "LMB orbit  |  RMB zoom"
				: "Orbit / zoom";
		const ImVec2 controlsSize = ImGui::CalcTextSize(controls);
		const ImVec2 controlsMinimum(
			previewMinimum.x + 8.0f,
			previewMaximum.y - controlsSize.y - 12.0f);
		const ImVec2 controlsMaximum(
			previewMaximum.x - 8.0f,
			previewMaximum.y - 6.0f);
		drawList->AddRectFilled(
			controlsMinimum,
			controlsMaximum,
			IM_COL32(8, 12, 20, 190),
			4.0f);
		drawList->AddText(
			ImVec2(
				controlsMinimum.x + 6.0f,
				controlsMinimum.y + 3.0f),
			IM_COL32(215, 225, 240, 225),
			controls);

		if (hovered || active)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		if (hovered && !active)
		{
			ImGui::SetTooltip(
				"LMB drag: orbit\n"
				"RMB drag or wheel: zoom\n"
				"Double-click: reset view");
		}
	}

	void MaterialPreviewRenderer::Render(
		const ResourceMaterial& material)
	{
		GLint previousFramebuffer = 0;
		GLint previousProgram = 0;
		GLint previousVertexArray = 0;
		GLint previousDepthFunction = 0;
		GLint previousCullFaceMode = 0;
		GLint previousActiveTexture = 0;
		GLint previousViewport[4] = {};
		GLfloat previousClearColor[4] = {};
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);
		glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
		glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunction);
		glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFaceMode);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
		glGetIntegerv(GL_VIEWPORT, previousViewport);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor);
		const bool depthWasEnabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
		const bool cullWasEnabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
		const bool blendWasEnabled = glIsEnabled(GL_BLEND) == GL_TRUE;

		framebuffer_->Bind();
		glViewport(0, 0, PreviewSize, PreviewSize);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDisable(GL_BLEND);
		if (material.GetDoubleSided())
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glClearColor(0.018f, 0.024f, 0.038f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ComponentCamera camera(nullptr);
		camera.frustum.type = FrustumType::PerspectiveFrustum;
		const float cosPitch = std::cos(pitch_);
		camera.frustum.pos = float3(
			std::sin(yaw_) * cosPitch,
			std::sin(pitch_),
			std::cos(yaw_) * cosPitch) * distance_;
		camera.frustum.front =
			(-camera.frustum.pos).Normalized();
		camera.frustum.up = float3::unitY;
		camera.frustum.nearPlaneDistance = 0.1f;
		camera.frustum.farPlaneDistance = 100.0f;
		camera.frustum.verticalFov = DEGTORAD * 45.0f;
		camera.SetAspectRatio(1.0f);

		IBLData* skybox =
			App->level ? App->level->GetSkyBox() : nullptr;
		const TextureCube* environment =
			skybox ? skybox->GetEnvironment() : nullptr;
		if (showSkybox_ && skybox)
		{
			skybox->DrawEnvironment(
				camera.GetProjectionMatrix(),
				camera.GetViewMatrix());
		}

		const ResourceMesh* mesh =
			shape_ == Shape::Sphere
				? App->resources->GetDefaultSphere()
				: App->resources->GetDefaultCube();
		if (mesh)
		{
			const float previewScale =
				shape_ == Shape::Sphere ? 0.95f : 1.45f;
			const float4x4 model =
				float4x4::Scale(float3(previewScale));

			materialProgram_->Use();
			materialProgram_->BindUniformFromName("uModel", model);
			materialProgram_->BindUniformFromName(
				"uView", camera.GetViewMatrix());
			materialProgram_->BindUniformFromName(
				"uProjection", camera.GetProjectionMatrix());
			materialProgram_->BindUniformFromName(
				"uCameraPosition", camera.GetPos());
			materialProgram_->BindUniformFromName(
				"uTiling", material.GetUVTiling());
			materialProgram_->BindUniformFromName(
				"uOffset", material.GetUVOffset());

			const Texture* baseTexture = nullptr;
			const Texture* surfaceTexture = nullptr;
			const Texture* normalTexture = nullptr;
			const Texture* occlusionTexture = nullptr;
			const Texture* emissiveTexture = nullptr;
			float4 baseColor = float4::one;
			float3 specularColor(0.04f);
			float3 emissiveColor = float3::zero;
			float metalness = 0.0f;
			float roughness = 0.5f;
			float normalStrength = 1.0f;
			float occlusionStrength = 1.0f;
			int workflow = 0;

			if (material.GetWorkFlow() == MetallicRoughness)
			{
				const MetallicRoughData& data =
					material.GetMetallicRoughData();
				baseColor = data.baseColor;
				emissiveColor =
					data.emissive_color * data.emissive_intensity;
				metalness = data.metalness;
				roughness = data.roughness;
				normalStrength = data.normal_strength;
				occlusionStrength = data.occlusion_strength;
				baseTexture = GetTexture(
					material.GetTextureRes(MR_TextureBaseColor));
				surfaceTexture = GetTexture(
					material.GetTextureRes(MR_TextureMetallicRough));
				normalTexture = GetTexture(
					material.GetTextureRes(MR_TextureNormal));
				occlusionTexture = GetTexture(
					material.GetTextureRes(MR_TextureOcclusion));
				emissiveTexture = GetTexture(
					material.GetTextureRes(MR_TextureEmissive));
			}
			else
			{
				const SpecularGlossData& data =
					material.GetSpecularGlossData();
				workflow = 1;
				baseColor = data.diffuse_color;
				specularColor =
					data.specular_color * data.specular_intensity;
				emissiveColor =
					data.emissive_color * data.emissive_intensity;
				roughness =
					1.0f - std::clamp(data.smoothness, 0.0f, 1.0f);
				normalStrength = data.normal_strength;
				occlusionStrength = data.occlusion_strength;
				baseTexture = GetTexture(
					material.GetTextureRes(SG_TextureDiffuse));
				surfaceTexture = GetTexture(
					material.GetTextureRes(SG_TextureSpecular));
				normalTexture = GetTexture(
					material.GetTextureRes(SG_TextureNormal));
				occlusionTexture = GetTexture(
					material.GetTextureRes(SG_TextureOcclusion));
				emissiveTexture = GetTexture(
					material.GetTextureRes(SG_TextureEmissive));
			}

			materialProgram_->BindUniformFromName(
				"uBaseColor", baseColor);
			materialProgram_->BindUniformFromName(
				"uSpecularColor", specularColor);
			materialProgram_->BindUniformFromName(
				"uEmissiveColor", emissiveColor);
			materialProgram_->BindUniformFromName(
				"uMetalness", metalness);
			materialProgram_->BindUniformFromName(
				"uRoughness", roughness);
			materialProgram_->BindUniformFromName(
				"uNormalStrength", normalStrength);
			materialProgram_->BindUniformFromName(
				"uOcclusionStrength", occlusionStrength);
			materialProgram_->BindUniformFromName(
				"uWorkflow", workflow);

			materialProgram_->BindUniformFromName(
				"uHasBaseColor", baseTexture ? 1 : 0);
			materialProgram_->BindUniformFromName(
				"uHasSurface", surfaceTexture ? 1 : 0);
			materialProgram_->BindUniformFromName(
				"uHasNormal", normalTexture ? 1 : 0);
			materialProgram_->BindUniformFromName(
				"uHasOcclusion", occlusionTexture ? 1 : 0);
			materialProgram_->BindUniformFromName(
				"uHasEmissive", emissiveTexture ? 1 : 0);
			materialProgram_->BindUniformFromName(
				"uHasEnvironment",
				showSkybox_ && environment ? 1 : 0);

			if (baseTexture)
				materialProgram_->BindTextureFromName(
					"uBaseColorMap", 0, baseTexture);
			if (surfaceTexture)
				materialProgram_->BindTextureFromName(
					"uSurfaceMap", 1, surfaceTexture);
			if (normalTexture)
				materialProgram_->BindTextureFromName(
					"uNormalMap", 2, normalTexture);
			if (occlusionTexture)
				materialProgram_->BindTextureFromName(
					"uOcclusionMap", 3, occlusionTexture);
			if (emissiveTexture)
				materialProgram_->BindTextureFromName(
					"uEmissiveMap", 4, emissiveTexture);
			if (showSkybox_ && environment)
				materialProgram_->BindTextureFromName(
					"uEnvironment", 5, environment);

			mesh->Draw();
		}

		glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
		glViewport(
			previousViewport[0], previousViewport[1],
			previousViewport[2], previousViewport[3]);
		glUseProgram(previousProgram);
		glBindVertexArray(previousVertexArray);
		glDepthFunc(previousDepthFunction);
		glCullFace(previousCullFaceMode);
		glActiveTexture(previousActiveTexture);
		glClearColor(
			previousClearColor[0], previousClearColor[1],
			previousClearColor[2], previousClearColor[3]);
		RestoreCapability(GL_DEPTH_TEST, depthWasEnabled);
		RestoreCapability(GL_CULL_FACE, cullWasEnabled);
		RestoreCapability(GL_BLEND, blendWasEnabled);
	}
}
