#include "Globals.h"
#include "Application.h"
#include "ModuleEditorCamera.h"
#include "ModuleEditor.h"
#include "ModuleInput.h"
#include "ModuleWindow.h"
#include "ComponentCamera.h"
#include "ModuleRenderer3D.h"
#include "Viewport.h"
#include "SceneViewport.h"
#include "ModuleHints.h"
#include "DebugDraw.h"
#include "ModuleLevelManager.h"
#include "GameObject.h"
#include <SDL.h>
#include <algorithm>
#include <vector>

#include "Leaks.h"

using namespace std;

namespace
{
	bool IsHeld(KeyState state)
	{
		return state == KEY_DOWN || state == KEY_REPEAT;
	}
}

ModuleEditorCamera::ModuleEditorCamera(bool start_enabled) : Module("Camera", start_enabled)
{
	dummy = new ComponentCamera(nullptr);
	picking = LineSegment(float3::zero, float3::unitY);
	last_hit = float3::zero;
}

ModuleEditorCamera::~ModuleEditorCamera()
{
	RELEASE(dummy);
}

// -----------------------------------------------------------------
bool ModuleEditorCamera::Init(Config* config)
{
	dummy->OnLoad(config);
	App->renderer3D->active_camera = dummy;
	App->renderer3D->culling_camera = dummy;

	return true;
}

// -----------------------------------------------------------------
bool ModuleEditorCamera::Start(Config* config)
{
	LOG("Setting up the camera");
	bool ret = true;

	Load(config);

	return ret;
}

// -----------------------------------------------------------------
bool ModuleEditorCamera::CleanUp()
{
	LOG("Cleaning camera");

	SetFlyMode(false);
	App->renderer3D->active_camera = nullptr;
	return true;
}

// -----------------------------------------------------------------
void ModuleEditorCamera::Save(Config * config) const
{
	config->AddFloat("Mov Speed", mov_speed);
	config->AddFloat("Rot Speed", rot_speed);
	config->AddFloat("Zoom Speed", zoom_speed);
	dummy->OnSave(*config);
}

// -----------------------------------------------------------------
void ModuleEditorCamera::Load(Config * config)
{
	// Beware, this method will be called again when loading a level!
	mov_speed  = config->GetFloat("Mov Speed", mov_speed); // global var, not level specific
	rot_speed  = config->GetFloat("Rot Speed", rot_speed); // global var, not level specific
	zoom_speed = config->GetFloat("Zoom Speed", zoom_speed); // global var, not level specific
	dummy->OnLoad(config);
}

// -----------------------------------------------------------------
void ModuleEditorCamera::DrawDebug()
{
	//dd::line(picking.a, picking.b, dd::colors::Blue);
	//dd::point(last_hit, dd::colors::Red);
}

// -----------------------------------------------------------------
update_status ModuleEditorCamera::Update(float dt)
{
	if (!App->IsStop())
	{
		SetFlyMode(false);
		return UPDATE_CONTINUE;
	}

	if (!App->renderer3D->viewport)
	{
		SetFlyMode(false);
		dummy->OnUpdateFrustum();
		return UPDATE_CONTINUE;
	}

	const bool focused = App->renderer3D->viewport->GetScene()->IsFocused();
	const bool navigating = focused && !App->renderer3D->viewport->GetScene()->IsUsingGuizmo();
	const KeyState rightMouse = App->input->GetMouseButton(SDL_BUTTON_RIGHT);
	const KeyState middleMouse = App->input->GetMouseButton(SDL_BUTTON_MIDDLE);
	const KeyState leftMouse = App->input->GetMouseButton(SDL_BUTTON_LEFT);
	const bool altPressed =
		IsHeld(App->input->GetKey(SDL_SCANCODE_LALT)) ||
		IsHeld(App->input->GetKey(SDL_SCANCODE_RALT));

	const bool wantsFlyMode = navigating && !altPressed &&
		IsHeld(rightMouse);
	SetFlyMode(wantsFlyMode);

	if (navigating)
	{
		int motion_x, motion_y;
		App->input->GetMouseMotion(motion_x, motion_y);
		const bool mouseMoved = motion_x != 0 || motion_y != 0;
		const float mouseSensitivity = rot_speed * 0.005f;

		// Unity-style scene navigation:
		// RMB + mouse/WASD/QE = fly, Alt+LMB = orbit,
		// MMB = pan, Alt+RMB = dolly.
		if (fly_mode)
		{
			if (mouseMoved)
				LookAt(-float(motion_x) * mouseSensitivity,
					-float(motion_y) * mouseSensitivity);
			Move(dt);
		}
		else if (altPressed &&
			IsHeld(leftMouse) && mouseMoved)
		{
			Orbit(-float(motion_x) * mouseSensitivity,
				-float(motion_y) * mouseSensitivity);
		}
		else if (IsHeld(middleMouse) && mouseMoved)
		{
			Pan(float(motion_x), float(motion_y));
		}
		else if (altPressed &&
			IsHeld(rightMouse) && mouseMoved)
		{
			Zoom(-float(motion_y) * zoom_speed * 0.1f);
		}

		const int wheel = App->input->GetMouseWheel();
		if (wheel != 0)
			Zoom(float(wheel) * zoom_speed *
				App->hints->GetFloatValue(ModuleHints::METRIC_PROPORTION));

		dummy->OnUpdateFrustum();
	}
	else
	{
		SetFlyMode(false);
	}

	return UPDATE_CONTINUE;
}

// -----------------------------------------------------------------
void ModuleEditorCamera::Look(const float3& position)
{
	dummy->Look(position);
}

// -----------------------------------------------------------------
void ModuleEditorCamera::CenterOn(const float3& position, float distance)
{
	float3 v = dummy->frustum.front.Neg();
	dummy->frustum.pos = position + (v * distance);
	looking_at = position;
	looking = true;
}

// -----------------------------------------------------------------
ComponentCamera * ModuleEditorCamera::GetDummy() const
{
	return dummy;
}

// -----------------------------------------------------------------
void ModuleEditorCamera::Move(float dt)
{
	Frustum* frustum = &dummy->frustum;

	float adjusted_speed = mov_speed;

	if (IsHeld(App->input->GetKey(SDL_SCANCODE_LSHIFT)) ||
		IsHeld(App->input->GetKey(SDL_SCANCODE_RSHIFT)))
		adjusted_speed *= 5.0f;

	float3 right(frustum->WorldRight());
	float3 forward(frustum->front);

	float3 movement(float3::zero);

    float metric_proportion = App->hints->GetFloatValue(ModuleHints::METRIC_PROPORTION);

	if (IsHeld(App->input->GetKey(SDL_SCANCODE_W))) movement += forward;
	if (IsHeld(App->input->GetKey(SDL_SCANCODE_S))) movement -= forward;
	if (IsHeld(App->input->GetKey(SDL_SCANCODE_A))) movement -= right;
	if (IsHeld(App->input->GetKey(SDL_SCANCODE_D))) movement += right;
	if (IsHeld(App->input->GetKey(SDL_SCANCODE_E))) movement += float3::unitY;
	if (IsHeld(App->input->GetKey(SDL_SCANCODE_Q))) movement -= float3::unitY;

	if (movement.Equals(float3::zero) == false)
	{
		frustum->Translate(movement.Normalized() *
			(metric_proportion * adjusted_speed * dt));
		looking = false;
	}
}

// -----------------------------------------------------------------
void ModuleEditorCamera::Orbit(float dx, float dy)
{
	float3 point = looking_at;

	// fake point should be a ray colliding with something
	if (looking == false)
	{
		LineSegment picking =
			dummy->frustum.UnProjectLineSegment(0.f, 0.f);
		float distance;
		GameObject* hit = App->level->CastRay(picking, distance);

		if (hit != nullptr)
			point = picking.GetPoint(distance);
		else
			point = dummy->frustum.pos +
				dummy->frustum.front * 50.0f;

		looking = true;
		looking_at = point;
	}

	float3 focus = dummy->frustum.pos - point;

	Quat qy = Quat::RotateY(dx);
	Quat qx(dummy->frustum.WorldRight(), dy);

	const float3 pitchedFocus = qx.Transform(focus);
	const float3 pitchedUp = qx.Transform(
		dummy->frustum.up).Normalized();
	if (pitchedUp.y > 0.0f)
		focus = pitchedFocus;
	focus = qy.Transform(focus);

	dummy->frustum.pos = focus + point;

	Look(point);
}

// -----------------------------------------------------------------
void ModuleEditorCamera::LookAt(float dx, float dy)
{
	looking = false;

	// x motion make the camera rotate in Y absolute axis (0,1,0) (not local)
	if (dx != 0.f)
	{
		Quat q = Quat::RotateY(dx);
		dummy->frustum.front =
			q.Mul(dummy->frustum.front).Normalized();
		// would not need this is we were rotating in the local Y, but that is too disorienting
		dummy->frustum.up =
			q.Mul(dummy->frustum.up).Normalized();
	}

	// y motion makes the camera rotate in X local axis, with tops
	if(dy != 0.f)
	{
		Quat q = Quat::RotateAxisAngle(
			dummy->frustum.WorldRight(), dy);

		float3 new_up =
			q.Mul(dummy->frustum.up).Normalized();

		if (new_up.y > 0.0f)
		{
			dummy->frustum.up = new_up;
			dummy->frustum.front =
				q.Mul(dummy->frustum.front).Normalized();
		}
	}
}

// -----------------------------------------------------------------
void ModuleEditorCamera::Zoom(float zoom)
{
	if (looking == true)
	{
		float dist = looking_at.Distance(dummy->frustum.pos);

		// Slower on closer distances
		if (dist < 15.0f)
			zoom *= 0.5f;
		if (dist < 7.5f)
			zoom *= 0.25f;
		if (dist < 1.0f && zoom > 0)
			zoom = 0;
	}

	if (IsHeld(App->input->GetKey(SDL_SCANCODE_LSHIFT)) ||
		IsHeld(App->input->GetKey(SDL_SCANCODE_RSHIFT)))
		zoom *= 5.0f;

	float3 p = dummy->frustum.front * zoom;
	dummy->frustum.pos += p;
}

// -----------------------------------------------------------------
void ModuleEditorCamera::Pan(float motion_x, float motion_y)
{
	Frustum& frustum = dummy->frustum;
	float distance = looking
		? looking_at.Distance(frustum.pos)
		: 10.0f;
	distance = std::max(distance, 1.0f);

	const float metricProportion =
		App->hints->GetFloatValue(ModuleHints::METRIC_PROPORTION);
	const float panScale = distance * 0.0025f * metricProportion;
	const float3 movement =
		frustum.WorldRight() * (-motion_x * panScale) +
		frustum.up * (motion_y * panScale);

	frustum.Translate(movement);
	if (looking)
		looking_at += movement;
}

// -----------------------------------------------------------------
void ModuleEditorCamera::SetFlyMode(bool enabled)
{
	if (fly_mode == enabled)
		return;

	fly_mode = enabled;
	App->input->SetCursorLocked(enabled);
}

// -----------------------------------------------------------------
GameObject* ModuleEditorCamera::Pick(float3* hit_point) const
{
	// The point (1, 1) corresponds to the top-right corner of the near plane
	// (-1, -1) is bottom-left

	float width = (float) App->window->GetWidth();
	float height = (float) App->window->GetHeight();

	int mouse_x, mouse_y;
	App->input->GetMousePosition(mouse_x, mouse_y);

	float normalized_x = -(1.0f - (float(mouse_x) * 2.0f ) / width);
	float normalized_y = 1.0f - (float(mouse_y) * 2.0f ) / height;

	LineSegment picking = App->renderer3D->active_camera->frustum.UnProjectLineSegment(normalized_x, normalized_y);

	float distance;
	GameObject* hit = App->level->CastRay(picking, distance);

	if (hit != nullptr && hit_point != nullptr)
		*hit_point = picking.GetPoint(distance);

	return hit;
}
