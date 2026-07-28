#pragma once

#include <memory>

#include <imgui.h>

class Framebuffer;
class Program;
class ResourceMaterial;
class Shader;
class Texture2D;

namespace EGE
{
	class MaterialPreviewRenderer final
	{
	public:
		MaterialPreviewRenderer();
		~MaterialPreviewRenderer();

		MaterialPreviewRenderer(const MaterialPreviewRenderer&) = delete;
		MaterialPreviewRenderer& operator=(
			const MaterialPreviewRenderer&) = delete;

		void Draw(ResourceMaterial& material, const ImVec2& size);

	private:
		enum class Shape
		{
			Sphere,
			Cube
		};

		bool Initialize();
		void Render(const ResourceMaterial& material);
		void DrawShapeButton(
			const char* label,
			Shape shape,
			float height);

	private:
		static constexpr unsigned int PreviewSize = 512;

		std::unique_ptr<Framebuffer> framebuffer_;
		std::unique_ptr<Texture2D> color_;
		std::unique_ptr<Texture2D> depth_;
		std::unique_ptr<Program> materialProgram_;

		Shape shape_ = Shape::Sphere;
		bool showSkybox_ = true;
		bool initialized_ = false;
		bool initializationFailed_ = false;
		float yaw_ = -0.55f;
		float pitch_ = 0.18f;
		float distance_ = 3.2f;
		float targetYaw_ = -0.55f;
		float targetPitch_ = 0.18f;
		float targetDistance_ = 3.2f;
		float yawVelocity_ = 0.0f;
		float pitchVelocity_ = 0.0f;
		float distanceVelocity_ = 0.0f;
	};
}
