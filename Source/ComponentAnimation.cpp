#include "Globals.h"
#include "ComponentAnimation.h"
#include "ComponentMeshRenderer.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ResourceAnimation.h"
#include "ResourceMesh.h"
#include "ResourceStateMachine.h"
#include "AnimController.h"
#include "gameObject.h"
#include "Component.h"

#include <list>
#include <algorithm>
#include <limits>

#include "Leaks.h"

using namespace std;

// ---------------------------------------------------------
ComponentAnimation::ComponentAnimation(GameObject* container) : Component(container, Types::Animation)
{
    controller = new AnimController;
}

// ---------------------------------------------------------
ComponentAnimation::~ComponentAnimation()
{
    StopPlayback();
    if (Resource* stateMachine =
            App && App->resources
                ? App->resources->Get(resource)
                : nullptr)
    {
        stateMachine->Release();
    }
    resource = 0;

    delete controller;
	
    if(context)
    {
        ax::NodeEditor::DestroyEditor(context);
        context = nullptr;
    }
}

// ---------------------------------------------------------
void ComponentAnimation::OnPlay()
{
    PlayDefault();
}

// ---------------------------------------------------------
bool ComponentAnimation::ResetState()
{
    StopPlayback();
    return PlayDefault();
}

// ---------------------------------------------------------
void ComponentAnimation::OnStop()
{
    StopPlayback();
}

// ---------------------------------------------------------
void ComponentAnimation::OnUpdate(float dt)
{
    controller->Update(unsigned(dt*1000));

    if(game_object != nullptr)
    {
        UpdateGO(game_object);
    }
}

// ---------------------------------------------------------
void ComponentAnimation::OnSave(Config& config) const 
{
	config.AddUID("Resource", resource);
    config.AddBool("DebugDraw", debug_draw);
    config.AddFloat("Speed", speed);
}

// ---------------------------------------------------------
void ComponentAnimation::OnLoad(Config* config) 
{
    SetResource(config->GetUID("Resource", 0));
    debug_draw = config->GetBool("DebugDraw", false);
    SetSpeed(config->GetFloat("Speed", 1.0f));
}

// ---------------------------------------------------------
void ComponentAnimation::UpdateGO(GameObject* go)
{
    // Rígid update

    float3 position = go->GetLocalPosition();
    Quat rotation = go->GetLocalRotationQ();

    if(controller->GetTransform(go->name, position, rotation))
    {
        go->SetLocalPosition(position);
        go->SetLocalRotation(rotation);
    }

    // Morph targets update
    
    tmp_components.clear();
    go->FindComponents(Component::MeshRenderer, tmp_components);

    for(Component* component : tmp_components)
    {
        ComponentMeshRenderer* mesh_renderer = static_cast<ComponentMeshRenderer*>(component);
        const ResourceMesh* mesh             = mesh_renderer->GetMeshRes();
        if (!mesh)
            continue;
        uint num_morphs                      = mesh->GetNumMorphTargets();

        if(num_morphs > 0)
        {
            std::span<const float> weights = controller->GetWeights(go->name);

            if(!weights.empty())
            {
                const uint count = std::min(
                    num_morphs,
                    static_cast<uint>(weights.size()));
                for (uint i=0; i< count; ++i)
                {
                    mesh_renderer->SetMorphTargetWeight(i, weights[i]);
                }
            }
        }
    }

    for(std::list<GameObject*>::iterator it = go->childs.begin(), end = go->childs.end(); it != end; ++it)
    {
        UpdateGO(*it);
    }
}

// ---------------------------------------------------------
bool ComponentAnimation::SetResource(UID uid)
{
    if (uid == resource)
        return uid == 0 || GetResource() != nullptr;

    Resource* next = nullptr;
    if (uid != 0)
    {
        next = App && App->resources
            ? App->resources->Get(uid)
            : nullptr;
        if (!next ||
            next->GetType() != Resource::state_machine ||
            !next->LoadToMemory())
        {
            return false;
        }
    }

    StopPlayback();
    if (Resource* previous =
            App && App->resources
                ? App->resources->Get(resource)
                : nullptr)
    {
        previous->Release();
    }
    resource = uid;
    return true;
}

// ---------------------------------------------------------
const ResourceStateMachine* ComponentAnimation::GetResource () const
{
    const Resource* stateMachine =
        App && App->resources
            ? App->resources->Get(resource)
            : nullptr;
    return stateMachine &&
        stateMachine->GetType() == Resource::state_machine
        ? static_cast<const ResourceStateMachine*>(stateMachine)
        : nullptr;
}

// ---------------------------------------------------------
ResourceStateMachine* ComponentAnimation::GetResource ()
{
    return const_cast<ResourceStateMachine*>(
        static_cast<const ComponentAnimation*>(this)->GetResource());
}

// ---------------------------------------------------------
HashString ComponentAnimation::GetActiveNode() const
{
    const ResourceStateMachine* res = GetResource();

    if(res != nullptr && active_node <  res->GetNumNodes() )
    {
        return res->GetNodeName(active_node);
    }

    return HashString();
}

// ---------------------------------------------------------
bool ComponentAnimation::IsPlaying() const
{
    return controller && controller->IsPlaying();
}

// ---------------------------------------------------------
void ComponentAnimation::SetSpeed(float value)
{
    speed = std::max(value, 0.0f);
    if (controller)
        controller->SetSpeed(speed);
}

// ---------------------------------------------------------
bool ComponentAnimation::PlayDefault()
{
    const ResourceStateMachine* stateMachine = GetResource();
    if (!stateMachine ||
        stateMachine->GetNumNodes() == 0 ||
        stateMachine->GetDefaultNode() >=
            stateMachine->GetNumNodes())
    {
        return false;
    }
    return PlayState(
        stateMachine->GetNodeName(
            stateMachine->GetDefaultNode()),
        0);
}

// ---------------------------------------------------------
bool ComponentAnimation::PlayState(
    const HashString& state,
    uint blendMilliseconds)
{
    const ResourceStateMachine* stateMachine = GetResource();
    if (!stateMachine || !state)
        return false;
    const uint stateIndex = stateMachine->FindNode(state);
    if (stateIndex >= stateMachine->GetNumNodes())
        return false;
    return PlayNode(stateIndex, blendMilliseconds);
}

// ---------------------------------------------------------
bool ComponentAnimation::SendTrigger(const HashString& trigger)
{
    const ResourceStateMachine* state_res = GetResource();
    if (!state_res || !trigger || !IsPlaying())
        return false;
    HashString active = GetActiveNode();

    for(uint i=0; i< state_res->GetNumTransitions(); ++i)
    {
        if(state_res->GetTransitionSource(i) == active && state_res->GetTransitionTrigger(i) == trigger)
        {
            return PlayState(
                state_res->GetTransitionTarget(i),
                state_res->GetTransitionBlend(i));
        }
    }
    return false;
}

// ---------------------------------------------------------
void ComponentAnimation::StopPlayback()
{
    if (controller)
        controller->Stop();
    active_node = (std::numeric_limits<unsigned>::max)();
}

// ---------------------------------------------------------
bool ComponentAnimation::IsInState(
    const HashString& state) const
{
    return IsPlaying() && state && GetActiveNode() == state;
}

// ---------------------------------------------------------
bool ComponentAnimation::PlayNode(const HashString& node, uint blend)
{
    ResourceStateMachine* stateMachine = GetResource();
    return stateMachine &&
        PlayNode(stateMachine->FindNode(node), blend);
}

// ---------------------------------------------------------
bool ComponentAnimation::PlayNode(uint node_idx, uint blend)
{
    ResourceStateMachine* state_res = GetResource();

    if(state_res && node_idx < state_res->GetNumNodes())
    {
        uint clip_idx = state_res->FindClip(state_res->GetNodeClip(node_idx));

        if(clip_idx < state_res->GetNumClips())
        {
            UID anim_res = state_res->GetClipRes(clip_idx);

            if(anim_res != 0)
            {
                controller->Play(anim_res, state_res->GetClipLoop(clip_idx), blend);
                controller->SetSpeed(speed);
                if (controller->IsPlaying())
                {
                    active_node = node_idx;
                    return true;
                }
                controller->Stop();
            }
        }
    }
    return false;
}

// ---------------------------------------------------------
ComponentAnimation::EditorContext* ComponentAnimation::GetEditorContext()
{
    if(context == nullptr)
    {
        Resource* res = GetResource();
        if (!res)
            return nullptr;
        char* tmp = (char*)malloc(sizeof(char)*255);

        sprintf_s(tmp, 255, ".%s%s.json", App->resources->GetDirByType(res->GetType()), res->GetExportedFile());

        ax::NodeEditor::Config cfg;
        cfg.SettingsFile = tmp;
        context = ax::NodeEditor::CreateEditor(&cfg);
    }

    return context;
}

