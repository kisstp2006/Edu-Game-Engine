#include "Globals.h"
#include "Application.h"
#include "Component.h"
#include "GameObject.h"

#include "Leaks.h"

// ---------------------------------------------------------
Component::Component(GameObject* container, Component::Types type) : game_object(container), type(type)
{
	uid = App && App->random ? App->random->Int() : 0;
	if (game_object != nullptr)
		SetActive(true);
}

// ---------------------------------------------------------
Component::~Component()
{}

void Component::InvalidateBoundingBox()
{
	if (game_object)
		game_object->InvalidateBoundingBox();
}

// ---------------------------------------------------------
void Component::SetActive(bool active)
{
	if (this->active != active)
	{
		this->active = active;
		if (active)
			OnActivate();
		else
			OnDeActivate();
		InvalidateBoundingBox();
	}
}

// ---------------------------------------------------------
bool Component::IsActive() const
{
	return active;
}

// ---------------------------------------------------------
Component::Types Component::GetType() const
{
	return type;
}

// ---------------------------------------------------------
const char * Component::GetTypeStr() const
{
	static_assert(Component::Types::Unknown == 18, "String list needs update");

	static const char* names[] = {
	"MeshRenderer",
	"AudioListener",
	"AudioSource",
	"Camera",
	"RigidBody",
	"Animation",
	"Steering",
	"Path",
	"RootMotion",
	"CharacterController",
	"ParticleSystem",
	"Trail",
    "Line",
    "Grass",
    "Decal",
	"SpotCone",
	"Script",
	"Collider",
	"Invalid" };

	return names[type];
}

uint Component::GetUID() const
{
	return uid;
}

void Component::SetUID(uint value)
{
	if (value != 0)
		uid = value;
}

// ---------------------------------------------------------
const GameObject * Component::GetGameObject() const
{
	return game_object;
}

// ---------------------------------------------------------
GameObject * Component::GetGameObject() 
{
	return game_object;
}
