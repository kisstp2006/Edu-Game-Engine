#ifndef _MODULE_DEBUGDRAW_H_
#define _MODULE_DEBUGDRAW_H_

#include "Module.h"
#include "Math.h"

#include <array>
#include <chrono>
#include <string>

class DDRenderInterfaceCoreGL;
class ComponentCamera;

enum class DebugDrawChannel
{
    Engine,
    Physics,
    Script,
    Count
};

class ModuleDebugDraw : public Module
{

public:

    ModuleDebugDraw();
    ~ModuleDebugDraw();

	bool            Init(Config* config = nullptr) override;
	bool            CleanUp() override;

    void            Draw(ComponentCamera* camera, unsigned fbo, unsigned fb_width, unsigned fb_height);
    void            Clear();

    void            SetChannelEnabled(DebugDrawChannel channel, bool enabled);
    bool            IsChannelEnabled(DebugDrawChannel channel) const;

    void DrawPoint(
        const float3& position,
        const float3& color,
        float size = 1.0f,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawLine(
        const float3& from,
        const float3& to,
        const float3& color,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawRay(
        const float3& origin,
        const float3& direction,
        const float3& color,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawArrow(
        const float3& from,
        const float3& to,
        const float3& color,
        float headSize = 0.1f,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawCross(
        const float3& center,
        const float3& color,
        float size = 1.0f,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawCircle(
        const float3& center,
        const float3& normal,
        const float3& color,
        float radius,
        int segments = 32,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawSphere(
        const float3& center,
        const float3& color,
        float radius,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawCapsule(
        const float3& center,
        const float3& direction,
        const float3& color,
        float radius,
        float height,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawCone(
        const float3& apex,
        const float3& direction,
        const float3& color,
        float baseRadius,
        float apexRadius = 0.0f,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawBox(
        const float3& center,
        const float3& size,
        const float3& color,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawBox(
        const std::array<float3, 8>& corners,
        const float3& color,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawBounds(
        const float3& minimum,
        const float3& maximum,
        const float3& color,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawPlane(
        const float3& center,
        const float3& normal,
        const float3& color,
        float size,
        float normalSize = 0.0f,
        float duration = 0.0f,
        bool depthTest = true,
        DebugDrawChannel channel = DebugDrawChannel::Engine);
    void DrawScreenText(
        const std::string& text,
        const float2& position,
        const float3& color,
        float scale = 1.0f,
        float duration = 0.0f,
        DebugDrawChannel channel = DebugDrawChannel::Engine);

private:
    static int DurationMilliseconds(float duration);
    static std::size_t ChannelIndex(DebugDrawChannel channel);

    static DDRenderInterfaceCoreGL* implementation;
    std::array<bool, static_cast<std::size_t>(DebugDrawChannel::Count)>
        channelEnabled_ = {true, true, true};
    std::chrono::steady_clock::time_point clockOrigin_;
    bool clockStarted_ = false;
};

#endif /* _MODULE_DEBUGDRAW_H_ */
