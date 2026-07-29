#ifndef __COMPONENT_ANIMATION_H__
#define __COMPONENT_ANIMATION_H__

#include "Component.h"
#include "HashString.h"
#include "imgui_node_editor.h"

#include <vector>
#include <list>
#include <limits>

class AnimController;
class ResourceStateMachine;

class ComponentAnimation : public Component
{
public:
    typedef ax::NodeEditor::EditorContext EditorContext;

	ComponentAnimation (GameObject* container);
	~ComponentAnimation ();

	virtual void                OnPlay          () override;
	virtual void                OnStop          () override;
	virtual void                OnUpdate        (float dt) override;

	virtual void                OnSave          (Config& config) const override;
	virtual void                OnLoad          (Config* config) override;

	bool                        SetResource     (UID uid);
    const ResourceStateMachine* GetResource     () const;
    ResourceStateMachine*       GetResource     ();
    UID                         GetResourceUID  () const { return resource; }

    bool                        GetDebugDraw    () const {return debug_draw;}
    void                        SetDebugDraw    (bool enable) { debug_draw = enable; }

    static Types                GetClassType    () { return Animation; }

    HashString                  GetActiveNode   () const;
    bool                        IsPlaying       () const;
    float                       GetSpeed        () const { return speed; }
    void                        SetSpeed        (float value);

    bool                        PlayDefault     ();
    bool                        PlayState       (
        const HashString& state,
        uint blendMilliseconds = 0);
    bool                        SendTrigger     (const HashString& trigger);
    bool                        ResetState      ();
    void                        StopPlayback    ();
    bool                        IsInState       (const HashString& state) const;

    EditorContext*              GetEditorContext();

private:

    void                        UpdateGO        (GameObject* go);
    bool                        PlayNode        (const HashString& node, uint blend);
    bool                        PlayNode        (uint node_idx, uint blend);

private:

    UID                      resource    = 0;
    AnimController*          controller  = nullptr;
    unsigned                 active_node =
        (std::numeric_limits<unsigned>::max)();
    float                    speed       = 1.0f;
    bool                     debug_draw  = false;
    EditorContext*           context     = nullptr;
    std::vector<Component*>  tmp_components;
};

#endif // __COMPONENT_AUDIOSOURCE_H__
