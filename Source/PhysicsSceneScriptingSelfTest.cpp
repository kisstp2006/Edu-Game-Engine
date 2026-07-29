#include "PhysicsSceneScriptingSelfTest.h"

#include "Application.h"
#include "ComponentCollider.h"
#include "ComponentAnimation.h"
#include "ComponentMeshRenderer.h"
#include "ComponentRigidBody.h"
#include "ComponentScript.h"
#include "Event.h"
#include "GameObject.h"
#include "ModuleFileSystem.h"
#include "ModuleLevelManager.h"
#include "ModulePhysics3D.h"
#include "ModuleResources.h"
#include "ModuleScripting.h"
#include "Reflection/PropertySerializer.h"
#include "Resource.h"
#include "ResourceMaterial.h"
#include "Scripting/ScriptObjectReference.h"

#include <SDL.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace
{
	void Trace(const char* stage)
	{
		std::fprintf(stderr, "[PhysicsSceneScriptingE2E] %s\n", stage);
		std::fflush(stderr);
	}

	class TemporaryPhysicsProject final
	{
	public:
		TemporaryPhysicsProject()
		{
			const auto suffix =
				std::chrono::steady_clock::now()
					.time_since_epoch()
					.count();
			root_ = std::filesystem::temp_directory_path() /
				("ege-physics-scene-e2e-" +
					std::to_string(suffix));
			std::filesystem::create_directories(
				root_ / "Assets" / "Scripts");
			std::filesystem::create_directories(
				root_ / "Assets" / "Scenes");
			std::filesystem::create_directories(
				root_ / "Assets" / "Materials");
			std::filesystem::create_directories(
				root_ / "Assets" / "Models");
		}

		~TemporaryPhysicsProject()
		{
			std::error_code ignored;
			std::filesystem::remove_all(root_, ignored);
		}

		[[nodiscard]] const std::filesystem::path& Root() const
		{
			return root_;
		}

		[[nodiscard]] bool WriteProbeScript() const
		{
			std::ofstream stream(
				root_ / "Assets" / "Scripts" /
					"PhysicsSceneProbe.as",
				std::ios::binary | std::ios::trunc);
			if (!stream)
				return false;

			stream <<
				"// EGE-ScriptId: "
				"8eb36921-f992-44f6-b50d-7362df8100f5\n"
				"[ScriptComponent]\n"
				"class ScriptPeer : EGEBehaviour\n"
				"{\n"
				"    int value = 41;\n"
				"    void Increment() { value++; }\n"
				"}\n"
				"\n"
				"[ScriptComponent]\n"
				"class PhysicsSceneProbe : EGEBehaviour\n"
				"{\n"
				"    [SerializeField] int collisionEnter = 0;\n"
				"    [SerializeField] int collisionStay = 0;\n"
				"    [SerializeField] int collisionExit = 0;\n"
				"    [SerializeField] int triggerEnter = 0;\n"
				"    [SerializeField] int triggerStay = 0;\n"
				"    [SerializeField] int triggerExit = 0;\n"
				"    [SerializeField] bool referencesValid = true;\n"
				"    [SerializeField] bool rendererApiValid = false;\n"
				"    [SerializeField] bool queryApiValid = false;\n"
				"    [SerializeField] bool scriptLookupValid = false;\n"
				"    [SerializeField] Mesh@ meshAsset;\n"
				"    [SerializeField] Material@ materialAsset;\n"
				"    [SerializeField] Texture@ textureAsset;\n"
				"    [SerializeField] AudioClip@ audioAsset;\n"
				"    [SerializeField] AnimationClip@ animationAsset;\n"
				"    [SerializeField] Model@ modelAsset;\n"
				"    [SerializeField] AnimationStateMachine@ "
					"stateMachineAsset;\n"
				"\n"
				"    void OnStart()\n"
				"    {\n"
				"        ScriptPeer@ peer =\n"
				"            gameObject.GetComponent<ScriptPeer>();\n"
				"        ScriptPeer@ triedPeer;\n"
				"        bool foundPeer =\n"
				"            gameObject.TryGetComponent<ScriptPeer>(\n"
				"                triedPeer);\n"
				"        array<ScriptPeer@>@ peers =\n"
				"            gameObject.GetComponents<ScriptPeer@>();\n"
				"        scriptLookupValid = peer !is null &&\n"
				"            gameObject.HasComponent<ScriptPeer>() &&\n"
				"            foundPeer && triedPeer is peer &&\n"
				"            peers !is null && peers.length() == 1 &&\n"
				"            peers[0] is peer && peer.value == 41;\n"
				"        if (peer !is null)\n"
				"        {\n"
				"            peer.Increment();\n"
				"            scriptLookupValid = scriptLookupValid &&\n"
				"                peer.value == 42;\n"
				"        }\n"
				"\n"
				"        RaycastHit@ hit;\n"
				"        bool rayHit = Physics::Raycast(\n"
				"            Vector3(-5, 0, 0),\n"
				"            Vector3(1, 0, 0), hit, 20.0f);\n"
				"        array<RaycastHit@>@ rayHits =\n"
				"            Physics::RaycastAll(\n"
				"                Vector3(-5, 0, 0),\n"
				"                Vector3(1, 0, 0), 20.0f);\n"
				"        RaycastHit@ sphereHit;\n"
				"        bool sphereCastHit = Physics::SphereCast(\n"
				"            Vector3(-5, 0, 0), 0.25f,\n"
				"            Vector3(1, 0, 0), sphereHit, 20.0f);\n"
				"        array<RaycastHit@>@ overlaps =\n"
				"            Physics::OverlapSphere(\n"
				"                Vector3(0, 0, 0), 2.0f);\n"
				"        queryApiValid = rayHit && hit !is null &&\n"
				"            hit.gameObject !is null &&\n"
				"            hit.rigidBody !is null &&\n"
				"            hit.collider !is null &&\n"
				"            hit.distance >= 0.0f &&\n"
				"            rayHits !is null && rayHits.length() >= 1 &&\n"
				"            sphereCastHit && sphereHit !is null &&\n"
				"            overlaps !is null && overlaps.length() >= 1;\n"
				"\n"
				"        MeshRenderer@ renderer =\n"
				"            gameObject.GetComponent<MeshRenderer>();\n"
				"        Mesh@ projectMesh = Resources::FindMesh(\n"
				"            \"Assets/Models/RuntimeMesh.edumesh.json\");\n"
				"        Material@ projectMaterial =\n"
				"            Resources::FindMaterial(\n"
				"                \"Assets/Materials/RuntimeMaterial.edumaterial.json\");\n"
				"        rendererApiValid = renderer !is null &&\n"
				"            renderer.mesh is null &&\n"
				"            renderer.GetMesh() is null &&\n"
				"            projectMesh !is null &&\n"
				"            renderer.material == projectMaterial &&\n"
				"            meshAsset is null &&\n"
				"            materialAsset is null &&\n"
				"            textureAsset is null &&\n"
				"            audioAsset is null &&\n"
				"            animationAsset is null &&\n"
				"            modelAsset is null &&\n"
				"            stateMachineAsset is null &&\n"
				"            renderer.materialCount == 3 &&\n"
				"            renderer.GetMaterials().length() == 3;\n"
				"        if (renderer !is null)\n"
				"        {\n"
				"            renderer.SetMesh(null);\n"
				"            @renderer.material = projectMaterial;\n"
				"            array<Material@>@ assigned =\n"
				"                renderer.GetMaterials();\n"
				"            @renderer.materials = assigned;\n"
				"            projectMaterial.baseColor =\n"
				"                Color(0.2f, 0.4f, 0.6f, 1.0f);\n"
				"            projectMaterial.metallic = 0.35f;\n"
				"            projectMaterial.roughness = 0.65f;\n"
				"            projectMaterial.doubleSided = true;\n"
				"            projectMaterial.uvTilingX = 2.0f;\n"
				"            projectMaterial.uvTilingY = 3.0f;\n"
				"            renderer.AddMaterial(null);\n"
				"            rendererApiValid = rendererApiValid &&\n"
				"                renderer.materialCount == 4;\n"
				"            renderer.RemoveMaterialAt(3);\n"
				"            renderer.visible = true;\n"
				"            renderer.castShadows = true;\n"
				"            rendererApiValid = rendererApiValid &&\n"
				"                renderer.material.metallic == 0.35f &&\n"
				"                renderer.material.roughness == 0.65f;\n"
				"        }\n"
				"    }\n"
				"\n"
				"    bool Validate(CollisionInfo@ info)\n"
				"    {\n"
				"        return info !is null &&\n"
				"            info.gameObject !is null &&\n"
				"            info.selfCollider !is null &&\n"
				"            info.collider !is null &&\n"
				"            info.selfColliderId != 0 &&\n"
				"            info.otherColliderId != 0;\n"
				"    }\n"
				"\n"
				"    void OnCollisionEnter(CollisionInfo@ info)\n"
				"    {\n"
				"        referencesValid = referencesValid && "
					"Validate(info);\n"
				"        collisionEnter++;\n"
				"    }\n"
				"\n"
				"    void OnCollisionStay(CollisionInfo@ info)\n"
				"    {\n"
				"        referencesValid = referencesValid && "
					"Validate(info);\n"
				"        collisionStay++;\n"
				"    }\n"
				"\n"
				"    void OnCollisionExit(CollisionInfo@ info)\n"
				"    {\n"
				"        referencesValid = referencesValid && "
					"Validate(info);\n"
				"        collisionExit++;\n"
				"    }\n"
				"\n"
				"    void OnTriggerEnter(CollisionInfo@ info)\n"
				"    {\n"
				"        referencesValid = referencesValid && "
					"Validate(info);\n"
				"        triggerEnter++;\n"
				"    }\n"
				"\n"
				"    void OnTriggerStay(CollisionInfo@ info)\n"
				"    {\n"
				"        referencesValid = referencesValid && "
					"Validate(info);\n"
				"        triggerStay++;\n"
				"    }\n"
				"\n"
				"    void OnTriggerExit(CollisionInfo@ info)\n"
				"    {\n"
				"        referencesValid = referencesValid && "
					"Validate(info);\n"
				"        triggerExit++;\n"
				"    }\n"
				"}\n";
			return stream.good();
		}

	private:
		std::filesystem::path root_;
	};

	bool ReadInteger(
		const EGE::ReflectedScriptObject& object,
		const char* name,
		std::int64_t& result)
	{
		if (!object.type || !object.object)
			return false;
		const EGE::PropertyDescriptor* property =
			object.type->FindProperty(name);
		EGE::PropertyValue value;
		if (!property || !property->Read(object.object, value))
			return false;
		if (const auto* signedValue =
				std::get_if<std::int64_t>(&value))
		{
			result = *signedValue;
			return true;
		}
		if (const auto* unsignedValue =
				std::get_if<std::uint64_t>(&value))
		{
			result = static_cast<std::int64_t>(*unsignedValue);
			return true;
		}
		return false;
	}

	bool ReadBoolean(
		const EGE::ReflectedScriptObject& object,
		const char* name,
		bool& result)
	{
		if (!object.type || !object.object)
			return false;
		const EGE::PropertyDescriptor* property =
			object.type->FindProperty(name);
		EGE::PropertyValue value;
		if (!property || !property->Read(object.object, value))
			return false;
		const bool* boolean = std::get_if<bool>(&value);
		if (!boolean)
			return false;
		result = *boolean;
		return true;
	}

	bool HasResourceProperty(
		const EGE::ReflectedScriptObject& object,
		const char* name,
		Resource::Type type)
	{
		if (!object.type || !object.object)
			return false;
		const EGE::PropertyDescriptor* property =
			object.type->FindProperty(name);
		EGE::PropertyValue value;
		if (!property ||
			property->kind !=
				EGE::PropertyKind::ResourceReference ||
			!property->Read(object.object, value))
		{
			return false;
		}
		const auto* resource =
			std::get_if<EGE::ResourceReferenceValue>(&value);
		return resource &&
			resource->resourceId == 0 &&
			resource->resourceType == static_cast<int>(type);
	}

	std::vector<ComponentCollider*> FindColliders(
		GameObject& gameObject)
	{
		std::vector<Component*> components;
		gameObject.FindComponents(Component::Collider, components);
		std::vector<ComponentCollider*> colliders;
		colliders.reserve(components.size());
		for (Component* component : components)
			colliders.push_back(
				static_cast<ComponentCollider*>(component));
		return colliders;
	}

	ComponentScript* FindScript(
		GameObject& gameObject,
		const char* className)
	{
		for (Component* component : gameObject.components)
		{
			if (!component ||
				component->GetType() != Component::Script)
			{
				continue;
			}
			auto* script =
				static_cast<ComponentScript*>(component);
			if (script->GetScriptClass() == className)
				return script;
		}
		return nullptr;
	}

	void ConfigureSphere(
		ComponentCollider& collider,
		float radius,
		bool isTrigger)
	{
		collider.SetShapeType(
			ComponentCollider::ShapeType::Sphere);
		collider.SetSphereCenter(float3::zero);
		collider.SetSphereRadius(radius);
		collider.SetTrigger(isTrigger);
	}
}

namespace EGE
{
	bool RunPhysicsSceneScriptingSelfTest()
	{
		Trace("begin");
		if (App)
			return false;

		SDL_SetMainReady();
		TemporaryPhysicsProject project;
		if (!project.WriteProbeScript())
			return false;

		std::unique_ptr<Application> application =
			std::make_unique<Application>(EngineMode::Runtime);
		App = application.get();
		Trace("application created");

		bool fileSystemMounted = false;
		bool physicsInitialized = false;
		bool physicsStarted = false;
		bool scriptingInitialized = false;
		bool levelInitialized = false;
		bool playStarted = false;
		bool passed = false;

		do
		{
			fileSystemMounted =
				App->fs->SetProjectRoot(project.Root());
			if (!fileSystemMounted)
				break;
			Trace("project mounted");

			const auto materialAsset =
				App->resources->CreateMaterialAsset(
					"Assets/Materials/"
						"RuntimeMaterial.edumaterial.json",
					"Runtime Material",
					ModuleResources::MaterialAssetWorkflow::
						MetallicRoughness);
			ModuleResources::ProceduralMeshSettings meshSettings;
			meshSettings.shape =
				ModuleResources::ProceduralMeshShape::Cube;
			meshSettings.width = 1.0f;
			const auto meshAsset =
				App->resources->CreateProceduralMeshAsset(
					"Assets/Models/RuntimeMesh.edumesh.json",
					"Runtime Mesh",
					meshSettings);
			if (!materialAsset || !meshAsset)
				break;
			Trace("script assets created");

			Config physicsConfig;
			physicsInitialized =
				App->physics3D->Init(&physicsConfig);
			if (!physicsInitialized)
				break;
			physicsStarted =
				App->physics3D->Start(&physicsConfig);
			if (!physicsStarted)
				break;
			Trace("physics started");

			scriptingInitialized =
				App->scripting->Init(nullptr);
			if (!scriptingInitialized)
				break;
			EGE::ScriptRuntime& runtime =
				App->scripting->GetRuntime();
			if (!runtime.SetProjectRoot(project.Root()) ||
				!runtime.HasClass("PhysicsSceneProbe") ||
				!runtime.HasClass("ScriptPeer"))
			{
				break;
			}
			std::ifstream definitions(
				project.Root() / "as.predefined");
			const std::string languageServerApi(
				(std::istreambuf_iterator<char>(definitions)),
				std::istreambuf_iterator<char>());
			if (languageServerApi.find("class RaycastHit") ==
					std::string::npos ||
				languageServerApi.find(
					"namespace Physics") ==
					std::string::npos ||
				languageServerApi.find(
					"GetComponent<T>()") ==
					std::string::npos)
			{
				break;
			}
			Trace("scripting started");

			levelInitialized = App->level->Init(nullptr);
			if (!levelInitialized)
				break;
			App->level->CreateNewEmpty("Physics Scene E2E");
			Trace("level initialized");

			GameObject* transformParent =
				App->level->CreateGameObject("Transform Parent");
			GameObject* transformChild =
				App->level->CreateGameObject(
					"Transform Child", transformParent);
			if (!transformParent || !transformChild)
			{
				Trace("transform objects missing");
				break;
			}
			transformParent->SetLocalPosition({10.0f, 2.0f, -3.0f});
			transformParent->SetLocalRotation(
				Quat::FromEulerXYZ(0.2f, 0.6f, -0.1f));
			transformParent->SetLocalScale({2.0f, 2.0f, 2.0f});
			transformChild->SetLocalPosition({1.0f, -2.0f, 3.0f});
			transformChild->SetLocalRotation(
				Quat::FromEulerXYZ(-0.3f, 0.1f, 0.4f));
			const float3 expectedImmediatePosition =
				(transformParent->GetLocalTransform() *
				 transformChild->GetLocalTransform()).TranslatePart();
			if (!transformChild->GetGlobalPosition().Equals(
					expectedImmediatePosition, 0.0001f))
			{
				Trace("immediate world transform failed");
				break;
			}
			const float3 requestedWorldPosition{4.0f, 5.0f, 6.0f};
			transformChild->SetGlobalPosition(requestedWorldPosition);
			if (!transformChild->GetGlobalPosition().Equals(
					requestedWorldPosition, 0.0001f))
			{
				Trace("world position setter failed");
				break;
			}
			const float4x4 worldBeforeReparent =
				transformChild->GetCalculatedGlobalTransform();
			transformChild->SetNewParent(
				App->level->GetRoot(), true);
			if (!transformChild->GetCalculatedGlobalTransform().Equals(
					worldBeforeReparent, 0.0001f))
			{
				Trace("world-position-stays reparent failed");
				break;
			}
			Trace("transform API verified");

			GameObject* observer =
				App->level->CreateGameObject("Observer");
			GameObject* obstacle =
				App->level->CreateGameObject("Obstacle");
			if (!observer || !obstacle)
				break;
			Trace("game objects created");

			auto* solidCollider =
				static_cast<ComponentCollider*>(
					observer->CreateComponent(Component::Collider));
			auto* triggerCollider =
				static_cast<ComponentCollider*>(
					observer->CreateComponent(Component::Collider));
			auto* observerBody =
				static_cast<ComponentRigidBody*>(
					observer->CreateComponent(Component::RigidBody));
			auto* meshRenderer =
				static_cast<ComponentMeshRenderer*>(
					observer->CreateComponent(
						Component::MeshRenderer));
			auto* animation =
				static_cast<ComponentAnimation*>(
					observer->CreateComponent(
						Component::Animation));
			auto* observerScript =
				static_cast<ComponentScript*>(
					observer->CreateComponent(Component::Script));
			auto* peerScript =
				static_cast<ComponentScript*>(
					observer->CreateComponent(Component::Script));
			auto* obstacleCollider =
				static_cast<ComponentCollider*>(
					obstacle->CreateComponent(Component::Collider));
			auto* obstacleBody =
				static_cast<ComponentRigidBody*>(
					obstacle->CreateComponent(Component::RigidBody));
			if (!solidCollider || !triggerCollider ||
				!observerBody || !meshRenderer || !animation ||
				!observerScript || !peerScript ||
				!obstacleCollider || !obstacleBody ||
				!meshRenderer->GetMaterialRes() ||
				meshRenderer->GetMaterialUID() != 0)
			{
				break;
			}
			Trace("components created");
			animation->SetSpeed(1.5f);
			if (animation->PlayDefault() ||
				animation->ResetState() ||
				animation->SendTrigger(HashString("Missing")) ||
				animation->IsPlaying() ||
				animation->GetActiveNode() ||
				std::abs(animation->GetSpeed() - 1.5f) > 0.0001f)
			{
				break;
			}
			animation->StopPlayback();

			ConfigureSphere(*solidCollider, 1.0f, false);
			ConfigureSphere(*triggerCollider, 1.25f, true);
			observerBody->SetBehaviour(
				ComponentRigidBody::BodyBehaviour::dynamic);
			observerBody->SetUseWorldGravity(false);
			observerBody->SetGravity(float3::zero);
			observerBody->SetLinearFactor(float3::zero);
			observerBody->SetAngularFactor(float3::zero);
			meshRenderer->SetMaterialCount(3);
			if (!meshRenderer->SetMaterialRes(
					0, materialAsset.uid))
			{
				break;
			}
			observerScript->SetScriptClass("PhysicsSceneProbe");
			peerScript->SetScriptClass("ScriptPeer");
			Trace("observer configured");

			ConfigureSphere(*obstacleCollider, 1.0f, false);
			obstacleBody->SetBehaviour(
				ComponentRigidBody::BodyBehaviour::fixed);
			obstacle->SetLocalPosition(float3(0.5f, 0.0f, 0.0f));
			Trace("obstacle configured");

			App->level->GetRoot()->RecursiveCalcGlobalTransform(
				float4x4::identity, true);
			Trace("transforms calculated");
			const char* scenePath =
				"Assets/Scenes/PhysicsSceneE2E.eduscene";
			if (!App->level->Save(scenePath))
				break;
			Trace("scene saved");
			if (!App->level->Load(scenePath) ||
				!App->level->HasScenePath())
			{
				break;
			}
			Trace("scene saved and loaded");

			observer = App->level->Find("Observer");
			obstacle = App->level->Find("Obstacle");
			if (!observer || !obstacle)
				break;

			const std::vector<ComponentCollider*> observerColliders =
				FindColliders(*observer);
			const std::vector<ComponentCollider*> obstacleColliders =
				FindColliders(*obstacle);
			observerBody = static_cast<ComponentRigidBody*>(
				observer->FindFirstComponent(Component::RigidBody));
			observerScript = FindScript(
				*observer, "PhysicsSceneProbe");
			peerScript = FindScript(*observer, "ScriptPeer");
			meshRenderer = static_cast<ComponentMeshRenderer*>(
				observer->FindFirstComponent(
					Component::MeshRenderer));
			animation = static_cast<ComponentAnimation*>(
				observer->FindFirstComponent(
					Component::Animation));
			obstacleBody = static_cast<ComponentRigidBody*>(
				obstacle->FindFirstComponent(Component::RigidBody));
			if (observerColliders.size() != 2 ||
				obstacleColliders.size() != 1 ||
				!observerBody || !meshRenderer || !animation ||
				!observerScript || !peerScript || !obstacleBody ||
				meshRenderer->GetMeshUID() != 0 ||
				meshRenderer->GetMaterialUID() != materialAsset.uid ||
				meshRenderer->GetMaterialCount() != 3 ||
				std::abs(animation->GetSpeed() - 1.5f) > 0.0001f ||
				animation->IsPlaying() ||
				!observerScript->IsBound())
			{
				break;
			}

			const int triggerCount = static_cast<int>(
				observerColliders[0]->IsTrigger()) +
				static_cast<int>(
					observerColliders[1]->IsTrigger());
			if (triggerCount != 1)
				break;

			const char* prefabPath =
				"Assets/Models/Observer.egeprefab";
			std::string prefabError;
			if (!App->level->SavePrefab(
					observer, prefabPath, &prefabError))
			{
				break;
			}

			App->physics3D->ReceiveEvent(Event(Event::play));
			App->scripting->ReceiveEvent(Event(Event::play));
			App->level->ReceiveEvent(Event(Event::play));
			playStarted = true;
			Trace("play started");

			constexpr float fixedDeltaTime = 1.0f / 60.0f;
			App->physics3D->Step(fixedDeltaTime);
			App->physics3D->Step(fixedDeltaTime);
			Trace("enter and stay stepped");

			obstacleColliders.front()->SetSphereCenter(
				float3(10.0f, 0.0f, 0.0f));
			App->physics3D->Step(fixedDeltaTime);
			Trace("exit stepped");

			const EGE::ReflectedScriptObject reflected =
				runtime.GetReflectedInstance(
					observerScript->GetInstanceHandle());
			std::int64_t collisionEnter = 0;
			std::int64_t collisionStay = 0;
			std::int64_t collisionExit = 0;
			std::int64_t triggerEnter = 0;
			std::int64_t triggerStay = 0;
			std::int64_t triggerExit = 0;
			bool referencesValid = false;
			bool rendererApiValid = false;
			bool queryApiValid = false;
			bool scriptLookupValid = false;
			ResourceMaterial* runtimeMaterial =
				static_cast<ResourceMaterial*>(
					App->resources->Get(materialAsset.uid));
			const EGE::PropertyDescriptor* meshAssetProperty =
				reflected.type
					? reflected.type->FindProperty("meshAsset")
					: nullptr;
			const EGE::PropertyDescriptor* materialAssetProperty =
				reflected.type
					? reflected.type->FindProperty("materialAsset")
					: nullptr;
			EGE::PropertyValue meshAssetValue;
			EGE::PropertyValue materialAssetValue;
			passed =
				ReadInteger(
					reflected, "collisionEnter", collisionEnter) &&
				ReadInteger(
					reflected, "collisionStay", collisionStay) &&
				ReadInteger(
					reflected, "collisionExit", collisionExit) &&
				ReadInteger(
					reflected, "triggerEnter", triggerEnter) &&
				ReadInteger(
					reflected, "triggerStay", triggerStay) &&
				ReadInteger(
					reflected, "triggerExit", triggerExit) &&
				ReadBoolean(
					reflected, "referencesValid", referencesValid) &&
				ReadBoolean(
					reflected,
					"rendererApiValid",
					rendererApiValid) &&
				ReadBoolean(
					reflected,
					"queryApiValid",
					queryApiValid) &&
				ReadBoolean(
					reflected,
					"scriptLookupValid",
					scriptLookupValid) &&
				meshAssetProperty &&
				meshAssetProperty->kind ==
					EGE::PropertyKind::ResourceReference &&
				meshAssetProperty->Read(
					reflected.object, meshAssetValue) &&
				std::get<EGE::ResourceReferenceValue>(
					meshAssetValue).resourceType ==
					static_cast<int>(Resource::mesh) &&
				materialAssetProperty &&
				materialAssetProperty->kind ==
					EGE::PropertyKind::ResourceReference &&
				materialAssetProperty->Read(
					reflected.object, materialAssetValue) &&
				std::get<EGE::ResourceReferenceValue>(
					materialAssetValue).resourceType ==
					static_cast<int>(Resource::material) &&
				HasResourceProperty(
					reflected,
					"textureAsset",
					Resource::texture) &&
				HasResourceProperty(
					reflected,
					"audioAsset",
					Resource::audio) &&
				HasResourceProperty(
					reflected,
					"animationAsset",
					Resource::animation) &&
				HasResourceProperty(
					reflected,
					"modelAsset",
					Resource::model) &&
				HasResourceProperty(
					reflected,
					"stateMachineAsset",
					Resource::state_machine) &&
				runtimeMaterial &&
				runtimeMaterial->GetDoubleSided() &&
				std::abs(
					runtimeMaterial->GetMetallicRoughData()
						.metalness -
					0.35f) < 0.0001f &&
				std::abs(
					runtimeMaterial->GetMetallicRoughData()
						.roughness -
					0.65f) < 0.0001f &&
				std::abs(
					runtimeMaterial->GetUVTiling().x -
					2.0f) < 0.0001f &&
				std::abs(
					runtimeMaterial->GetUVTiling().y -
					3.0f) < 0.0001f &&
				collisionEnter == 1 &&
				collisionStay >= 1 &&
				collisionExit == 1 &&
				triggerEnter == 1 &&
				triggerStay >= 1 &&
				triggerExit == 1 &&
				referencesValid &&
				rendererApiValid &&
				queryApiValid &&
				scriptLookupValid;
			if (passed)
			{
				const uint observerId = observer->GetUID();
				EGE::ScriptGameObjectReference* persistentReference =
					EGE::MakeGameObjectReference(observerId);
				GameObject* runtimePrefab =
					App->level->InstantiatePrefab(
						prefabPath, nullptr, &prefabError);
				const bool runtimePrefabValid =
					runtimePrefab &&
					runtimePrefab->FindFirstComponent(
						Component::Script) != nullptr;
				std::string invalidSceneError;
				const bool invalidRejected =
					!App->level->RequestLoad(
						"../Outside.eduscene",
						&invalidSceneError) &&
					!App->level->HasPendingSceneChange() &&
					!invalidSceneError.empty();
				std::string reloadError;
				const bool reloadQueued =
					App->level->RequestReload(&reloadError) &&
					App->level->HasPendingSceneChange() &&
					App->level->Find(observerId) == observer;
				const bool reloadProcessed =
					reloadQueued &&
					App->level->ProcessPendingSceneChange() &&
					!App->level->HasPendingSceneChange() &&
					App->level->HasScenePath() &&
					App->level->Find(observerId) != nullptr &&
					persistentReference &&
					persistentReference->Resolve() ==
						App->level->Find(observerId);
				std::fprintf(
					stderr,
					"[PhysicsSceneScriptingE2E] prefab=%d "
					"invalid=%d queued=%d reload=%d error=%s\n",
					runtimePrefabValid,
					invalidRejected,
					reloadQueued,
					reloadProcessed,
					reloadError.c_str());
				passed =
					runtimePrefabValid &&
					invalidRejected &&
					reloadProcessed;
				if (persistentReference)
					persistentReference->Release();
			}
			Trace(passed ? "assertions passed" : "assertions failed");
		}
		while (false);

		Trace("cleanup begin");
		if (playStarted)
		{
			App->physics3D->ReceiveEvent(Event(Event::stop));
			App->scripting->ReceiveEvent(Event(Event::stop));
			App->level->ReceiveEvent(Event(Event::stop));
		}
		if (levelInitialized)
			App->level->CleanUp();
		Trace("level cleaned");
		if (scriptingInitialized)
			App->scripting->CleanUp();
		Trace("scripting cleaned");
		if (physicsStarted)
			App->physics3D->CleanUp();
		Trace("physics cleaned");
		if (fileSystemMounted)
			App->fs->ClearProjectRoot();

		application.reset();
		App = nullptr;
		Trace("application destroyed");
		return passed;
	}
}
