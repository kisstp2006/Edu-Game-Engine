#include "Globals.h"
#include "Application.h"
#include "ModuleLevelManager.h"
#include "ModuleFileSystem.h"
#include "GameObject.h"
#include "Project/Project.h"
#include "Config.h"

#include "ModuleRenderer3D.h"
#include "ModuleEditorCamera.h"
#include "ModuleEditor.h"
#include "ModuleResources.h"
#include "ModuleHints.h"

#include "ResourceModel.h"
#include "ResourceMesh.h"

#include "ComponentCamera.h"
#include "ComponentMeshRenderer.h"
#include "ComponentScript.h"

#include "Event.h"
#include "ThreadPool.h"

#include "IBLData.h"
#include "LightManager.h"

#include "OpenGL.h"
#include "Leaks.h"

#include <algorithm>
#include <cctype>

using namespace std;

namespace
{
	bool ResolveProjectScenePath(
		const char* file,
		std::filesystem::path& relativePath,
		std::string& error)
	{
		if (!file || file[0] == '\0')
		{
			error = "No scene file was selected.";
			return false;
		}

		const std::filesystem::path projectRoot =
			App->fs->GetProjectRoot().lexically_normal();
		if (projectRoot.empty())
		{
			error = "No project is currently mounted.";
			return false;
		}

		std::filesystem::path candidate(file);
		if (candidate.is_absolute())
		{
			candidate =
				candidate.lexically_normal().lexically_relative(projectRoot);
		}
		else
		{
			candidate = candidate.lexically_normal();
		}

		if (!EGE::IsSafeProjectRelativePath(candidate))
		{
			error = "The scene must be inside the active project.";
			return false;
		}

		std::string extension = candidate.extension().string();
		std::transform(
			extension.begin(),
			extension.end(),
			extension.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		if (extension != ".eduscene" && extension != ".scene")
		{
			error = "Scene files must use the .eduscene extension.";
			return false;
		}

		relativePath = std::move(candidate);
		return true;
	}

	bool WriteSceneConfig(
		const Config& config,
		const std::filesystem::path& relativePath,
		std::string* error)
	{
		char* buffer = nullptr;
		const uint size = static_cast<uint>(
			config.Save(&buffer, "EDU Engine scene"));
		const uint written = App->fs->Save(
			relativePath.generic_string().c_str(),
			buffer,
			size);
		RELEASE_ARRAY(buffer);

		if (written == size)
			return true;

		LOG(
			"Could not write scene [%s]",
			relativePath.generic_string().c_str());
		if (error)
			*error = "The scene file could not be written.";
		return false;
	}

	bool ResolveProjectPrefabPath(
		const char* file,
		std::filesystem::path& relativePath,
		std::string& error)
	{
		if (!file || file[0] == '\0')
		{
			error = "No prefab file was selected.";
			return false;
		}

		const std::filesystem::path projectRoot =
			App->fs->GetProjectRoot().lexically_normal();
		if (projectRoot.empty())
		{
			error = "No project is currently mounted.";
			return false;
		}

		std::filesystem::path candidate(file);
		if (candidate.is_absolute())
		{
			candidate =
				candidate.lexically_normal().lexically_relative(projectRoot);
		}
		else
		{
			candidate = candidate.lexically_normal();
		}

		if (!EGE::IsSafeProjectRelativePath(candidate))
		{
			error = "The prefab must be inside the active project.";
			return false;
		}

		const auto first = candidate.begin();
		if (first == candidate.end())
		{
			error = "The prefab must be inside the project's Assets folder.";
			return false;
		}
		std::string rootName = first->string();
		std::transform(
			rootName.begin(), rootName.end(), rootName.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		if (rootName != "assets")
		{
			error = "The prefab must be inside the project's Assets folder.";
			return false;
		}

		std::string extension = candidate.extension().string();
		std::transform(
			extension.begin(), extension.end(), extension.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		if (extension != ".egeprefab")
		{
			error = "Prefab files must use the .egeprefab extension.";
			return false;
		}

		relativePath = std::move(candidate);
		return true;
	}

	bool WritePrefabConfig(
		const Config& config,
		const std::filesystem::path& relativePath,
		std::string* error)
	{
		char* buffer = nullptr;
		const uint size = static_cast<uint>(
			config.Save(&buffer, "EDU Engine prefab"));
		const uint written = App->fs->Save(
			relativePath.generic_string().c_str(),
			buffer,
			size);
		RELEASE_ARRAY(buffer);

		if (written == size)
			return true;

		if (error)
			*error = "The prefab file could not be written.";
		return false;
	}
}

ModuleLevelManager::ModuleLevelManager( bool start_enabled) : Module("LevelManager", start_enabled)
{
}

// Destructor
ModuleLevelManager::~ModuleLevelManager()
{
}

// Called before render is available
bool ModuleLevelManager::Init(Config* config)
{
	bool ret = true;
	LOG("Loading Level Manager");
	
	// create an empty game object to be the root of everything
	root = new GameObject(nullptr, "root");
	quadtree.SetBoundaries(AABB(float3(-500,0,-500), float3(500,30,500)));

    skybox = std::make_unique<IBLData>();
	lightManager = std::make_unique<LightManager>();

	return ret;
}

bool ModuleLevelManager::Start(Config * config)
{
	const std::shared_ptr<const EGE::Project> project =
		App->GetActiveProject();
	if (project && !project->GetConfig().startScene.empty())
		Load(project->GetConfig().startScene.generic_string().c_str());
	else
		CreateNewEmpty(project ? project->GetName().c_str() : "Untitled");
	
	return true;
}

update_status ModuleLevelManager::PreUpdate(float dt)
{
	if (App->IsPlay())
	{
		const EGE::TimeService& time = App->GetTime();
		for (std::uint32_t step = 0;
			step < time.GetFixedStepCount();
			++step)
		{
			RecursiveFixedUpdate(root, time.GetFixedDeltaTime());
		}
	}
	return UPDATE_CONTINUE;
}

update_status ModuleLevelManager::Update(float dt)
{
    if (App->IsPlay())
    {
		dt = App->GetTime().GetDeltaTime();
#ifdef USE_THREAD_POOL
        root->OnUpdate(dt);

        ThreadPool* pool = App->getThreadPool();
        std::vector<std::future<void> > futures;
        futures.reserve(root->childs.size());

        for (list<GameObject*>::const_iterator it = root->childs.begin(); it != root->childs.end(); ++it)
        {
            GameObject* child = *it;
            futures.push_back(pool->submitTask([=]() 
                { 
                    RecursiveUpdate(child, dt); 
                    root->RecursiveCalcGlobalTransform(float4x4::identity, false);
                    root->RecursiveCalcBoundingBoxes();
                }));
        }

        for (auto& ft : futures) ft.wait();

#else
        RecursiveUpdate(root, dt);
        // Update transformations tree for this frame
        root->RecursiveCalcGlobalTransform(float4x4::identity, false);
        root->RecursiveCalcBoundingBoxes();
#endif 
        
    }
    else
    {
        // Update transformations tree for this frame
        root->RecursiveCalcGlobalTransform(float4x4::identity, false);
        root->RecursiveCalcBoundingBoxes();
    }

    
    return UPDATE_CONTINUE;
}

update_status ModuleLevelManager::PostUpdate(float dt)
{
	if (App->IsPlay())
		RecursiveLateUpdate(root, App->GetTime().GetDeltaTime());
	return UPDATE_CONTINUE;
}

// Called before quitting
bool ModuleLevelManager::CleanUp()
{
	LOG("Freeing Level Manager");

	if (root)
		root->DestroyImmediate();
	RELEASE(root);

	return true;
}

void ModuleLevelManager::ReceiveEvent(const Event & event)
{
	RecursiveProcessEvent(root, event);
}

const GameObject * ModuleLevelManager::GetRoot() const
{
	return root;
}

GameObject * ModuleLevelManager::GetRoot()
{
	return root;
}

const GameObject * ModuleLevelManager::Find(uint uid) const
{
	if (uid > 0)
		return RecursiveFind(uid, root, false);

	return nullptr;
}

GameObject * ModuleLevelManager::Find(uint uid)
{
	if (uid > 0)
		return RecursiveFind(uid, root, false);

	return nullptr;
}

const GameObject* ModuleLevelManager::Find(const char* objectName) const
{
	return objectName && *objectName
		? RecursiveFind(objectName, root)
		: nullptr;
}

GameObject* ModuleLevelManager::Find(const char* objectName)
{
	return objectName && *objectName
		? RecursiveFind(objectName, root)
		: nullptr;
}

bool ModuleLevelManager::CreateNewEmpty(const char * name)
{
	UnloadCurrent();
	this->name = name ? name : "Untitled";
	scene_path.clear();
	return true;
}

GameObject * ModuleLevelManager::CreateGameObject(GameObject * parent, const float3 & pos, const float3 & scale, const Quat & rot, const char * name)
{
	GameObject* ret = new GameObject(parent, name, pos, scale, rot);

	return ret;
}

GameObject * ModuleLevelManager::CreateGameObject(GameObject * parent)
{
	if (parent == nullptr)
		parent = root;

	return new GameObject(parent, "Unnamed");
}

GameObject* ModuleLevelManager::CreateGameObject(
	const char* objectName,
	GameObject* parent)
{
	if (!parent || parent->IsPendingDestroy())
		parent = root;
	return new GameObject(
		parent,
		objectName && *objectName ? objectName : "GameObject");
}

void ModuleLevelManager::DestroyGameObject(GameObject* gameObject)
{
	if (!gameObject)
		return;

	if (gameObject == root)
	{
		std::vector<GameObject*> children(
			root->childs.begin(), root->childs.end());
		for (GameObject* child : children)
			DestroyGameObject(child);
		return;
	}

	if (gameObject->IsPendingDestroy() ||
		RecursiveFind(gameObject->GetUID(), root, true) != gameObject)
	{
		return;
	}

	gameObject->SetPendingDestroyRecursively(true);
	std::scoped_lock lock(pending_destructions_mutex);
	pending_destructions.push_back(gameObject->GetUID());
}

void ModuleLevelManager::FlushPendingDestructions()
{
	std::vector<uint> pending;
	{
		std::scoped_lock lock(pending_destructions_mutex);
		pending.swap(pending_destructions);
	}

	for (uint uid : pending)
	{
		GameObject* gameObject =
			RecursiveFind(uid, root, true);
		if (gameObject && gameObject != root)
			gameObject->DestroyImmediate();
	}

	RecursiveFlushPendingComponentRemovals(root);
}

GameObject * ModuleLevelManager::Duplicate(const GameObject * original)
{
	if (!original || Validate(original) != original || original == root)
		return nullptr;

	Config save;
	save.AddArray("Game Objects");
	original->SaveSubtree(save);
	return LoadGameObjects(save, true);
}


GameObject* ModuleLevelManager::LoadGameObjects(
	const Config& config,
	bool regenerateIds)
{
	int count = config.GetArrayCount("Game Objects");
	if (count <= 0)
		return nullptr;

	map<GameObject*, uint> relations;
	map<uint, uint> gameObjectIds;
	map<uint, uint> componentIds;
	std::vector<GameObject*> gos(count);

	for (int i = 0; i < count; ++i)
	{
		GameObject* go = CreateGameObject();
		const uint generatedUid = go->GetUID();
		Config goCfg = config.GetArray("Game Objects", i);
		go->Load(&goCfg, relations);
		if (regenerateIds)
		{
			gameObjectIds[go->GetUID()] = generatedUid;
			go->uid = generatedUid;
		}
		gos[i] = go;
	}

	for (int i = 0; i < count; ++i)
	{
		Config goCfg = config.GetArray("Game Objects", i);
		gos[i]->LoadComponents(
			&goCfg,
			regenerateIds ? &componentIds : nullptr);
	}

	for (map<GameObject*, uint>::iterator it = relations.begin(); it != relations.end(); ++it)
	{
		uint parent_id = it->second;
		GameObject* go = it->first;

		if (parent_id > 0)
		{
			if (regenerateIds)
			{
				const auto remapped = gameObjectIds.find(parent_id);
				parent_id = remapped == gameObjectIds.end()
					? 0
					: remapped->second;
			}
			GameObject* parent_go =
				parent_id > 0 ? Find(parent_id) : nullptr;
			if (parent_go != nullptr)
				go->SetNewParent(parent_go);
		}
	}

	if (regenerateIds)
	{
		for (GameObject* go : gos)
		{
			for (Component* component : go->components)
			{
				component->RemapSerializedReferences(
					gameObjectIds, componentIds);
			}
		}
	}

	root->RecursiveCalcGlobalTransform(root->GetLocalTransform(), true);
	root->RecursiveCalcBoundingBoxes();
	
	for (map<GameObject*, uint>::iterator it = relations.begin(); it != relations.end(); ++it)
		it->first->OnStart();

	return gos.front();
}

bool ModuleLevelManager::Load(const char * file)
{
	bool ret = false;
	std::filesystem::path resolvedPath;
	std::string pathError;

	if (ResolveProjectScenePath(file, resolvedPath, pathError))
	{
		char* buffer = nullptr;
		const std::string sourcePath = resolvedPath.generic_string();
		uint size = App->fs->Load(sourcePath.c_str(), &buffer);

		if (buffer != nullptr && size > 0)
		{
			Config config(buffer);

			if (config.IsValid())
			{
				UnloadCurrent();

				// Load level description
				Config desc(config.GetSection("Description"));
				name = desc.GetString("Name", "Unnamed level");
				//App->hints->Init(&desc);
				//App->camera->Load(&desc);

				lightManager->LoadLights(config.GetSection("Lights"));
				LoadGameObjects(config);
				skybox->Load(config.GetSection("Skybox"));
				scene_path = resolvedPath;
				ret = true;
			}
        }

		RELEASE_ARRAY(buffer); 
	}
	else
	{
		LOG("Could not load scene: %s", pathError.c_str());
	}

	return ret;
}

bool ModuleLevelManager::Save(const char * file)
{
	const char* requestedPath = file;
	const std::string currentPath = scene_path.generic_string();
	if (!requestedPath && !currentPath.empty())
		requestedPath = currentPath.c_str();

	std::filesystem::path resolvedPath;
	std::string pathError;
	if (!ResolveProjectScenePath(
			requestedPath, resolvedPath, pathError))
	{
		LOG("Could not save scene: %s", pathError.c_str());
		return false;
	}

	const std::string savedName = resolvedPath.stem().string();

	Config save;

	// Add header info
	Config desc(save.AddSection("Description"));
	desc.AddString("Name", savedName.c_str());
    //App->hints->Save(&desc);
    //App->camera->Save(&desc);

    Config lightsCfg = save.AddSection("Lights");
    lightManager->SaveLights(lightsCfg);

	// Serialize GameObjects recursively
	save.AddArray("Game Objects");

	for (list<GameObject*>::const_iterator it = root->childs.begin(); it != root->childs.end(); ++it)
		(*it)->Save(save);

    Config skyCfg = save.AddSection("Skybox");
	skybox->Save(skyCfg);

	if (!WriteSceneConfig(save, resolvedPath, nullptr))
		return false;

	name = savedName;
	scene_path = std::move(resolvedPath);
	return true;
}

bool ModuleLevelManager::CreateEmptySceneAsset(
	const char* file,
	const char* sceneName,
	std::string* error) const
{
	std::filesystem::path resolvedPath;
	std::string pathError;
	if (!ResolveProjectScenePath(file, resolvedPath, pathError))
	{
		if (error)
			*error = std::move(pathError);
		return false;
	}

	if (App->fs->Exists(resolvedPath.generic_string().c_str()))
	{
		if (error)
			*error = "A scene with this name already exists.";
		return false;
	}

	Config scene;
	Config description = scene.AddSection("Description");
	const std::string resolvedName =
		sceneName && sceneName[0] != '\0'
			? sceneName
			: resolvedPath.stem().string();
	description.AddString("Name", resolvedName.c_str());

	LightManager defaultLights;
	Config lights = scene.AddSection("Lights");
	defaultLights.SaveLights(lights);

	scene.AddArray("Game Objects");

	Config skyboxConfig = scene.AddSection("Skybox");
	skyboxConfig.AddUInt("Texture", 0);
	skyboxConfig.AddFloat("intensity", 1.0f);

	return WriteSceneConfig(scene, resolvedPath, error);
}

bool ModuleLevelManager::SavePrefab(
	const GameObject* source,
	const char* file,
	std::string* error) const
{
	if (!source || source == root ||
		Validate(source) != source ||
		source->IsPendingDestroy())
	{
		if (error)
			*error = "Select a valid GameObject to create a prefab.";
		return false;
	}

	if (!App->IsStop())
	{
		if (error)
			*error = "Prefabs can only be created while the game is stopped.";
		return false;
	}

	std::filesystem::path resolvedPath;
	std::string pathError;
	if (!ResolveProjectPrefabPath(file, resolvedPath, pathError))
	{
		if (error)
			*error = std::move(pathError);
		return false;
	}

	if (App->fs->Exists(resolvedPath.generic_string().c_str()))
	{
		if (error)
			*error = "A prefab with this name already exists.";
		return false;
	}

	Config prefab;
	Config description = prefab.AddSection("Prefab");
	description.AddUInt("FormatVersion", 1);
	description.AddString("Name", source->name.c_str());
	description.AddUInt("RootUID", source->GetUID());
	prefab.AddArray("Game Objects");
	source->SaveSubtree(prefab);

	return WritePrefabConfig(prefab, resolvedPath, error);
}

GameObject* ModuleLevelManager::InstantiatePrefab(
	const char* file,
	GameObject* parent,
	std::string* error)
{
	if (!App->IsStop())
	{
		if (error)
			*error = "Prefabs can only be instantiated while the game is stopped.";
		return nullptr;
	}

	std::filesystem::path resolvedPath;
	std::string pathError;
	if (!ResolveProjectPrefabPath(file, resolvedPath, pathError))
	{
		if (error)
			*error = std::move(pathError);
		return nullptr;
	}

	char* buffer = nullptr;
	const std::string sourcePath = resolvedPath.generic_string();
	const uint size = App->fs->Load(sourcePath.c_str(), &buffer);
	if (!buffer || size == 0)
	{
		RELEASE_ARRAY(buffer);
		if (error)
			*error = "The prefab file could not be read.";
		return nullptr;
	}

	Config prefab(buffer);
	RELEASE_ARRAY(buffer);
	if (!prefab.IsValid())
	{
		if (error)
			*error = "The prefab file is invalid.";
		return nullptr;
	}

	Config description(prefab.GetSection("Prefab"));
	if (description.GetUInt("FormatVersion", 0) != 1 ||
		prefab.GetArrayCount("Game Objects") <= 0)
	{
		if (error)
			*error = "The prefab format is not supported or contains no objects.";
		return nullptr;
	}

	if (parent && Validate(parent) != parent)
		parent = nullptr;

	GameObject* instance = LoadGameObjects(prefab, true);
	if (!instance)
	{
		if (error)
			*error = "The prefab could not be instantiated.";
		return nullptr;
	}

	if (parent)
		instance->SetNewParent(parent);
	root->RecursiveCalcGlobalTransform(root->GetLocalTransform(), true);
	root->RecursiveCalcBoundingBoxes();
	return instance;
}

bool ModuleLevelManager::HasScenePath() const
{
	return !scene_path.empty();
}

const std::filesystem::path& ModuleLevelManager::GetScenePath() const
{
	return scene_path;
}

void ModuleLevelManager::UnloadCurrent()
{
	App->GetTime().DiscardPendingFixedSteps();

	{
		std::scoped_lock lock(pending_destructions_mutex);
		pending_destructions.clear();
	}

	if (App->renderer3D && App->camera)
	{
		App->renderer3D->active_camera = App->camera->GetDummy();
		App->renderer3D->culling_camera = App->camera->GetDummy();
	}

	if (root)
		root->DestroyImmediate();

	quadtree.Clear();
	quadtree.SetBoundaries(
		AABB(float3(-500, 0, -500), float3(500, 30, 500)));
	skybox = std::make_unique<IBLData>();
	lightManager = std::make_unique<LightManager>();
	name.clear();
	scene_path.clear();
}

void ModuleLevelManager::RecursiveProcessEvent(GameObject * go, const Event & event) const
{
	switch (event.type)
	{
		case Event::EventType::play: go->OnPlay(); break;
		case Event::EventType::stop: go->OnStop(); break;
		case Event::EventType::pause: go->OnPause(); break;
		case Event::EventType::unpause: go->OnUnPause(); break;
		case Event::EventType::gameobject_destroyed: go->OnGoDestroyed(); break;
	}

	for (list<GameObject*>::const_iterator it = go->childs.begin(); it != go->childs.end(); ++it)
		RecursiveProcessEvent(*it, event);
}

void ModuleLevelManager::RecursiveUpdate(GameObject * go, float dt) const
{
	if (!go || go->IsPendingDestroy())
		return;
	go->OnUpdate(dt);

	for (list<GameObject*>::const_iterator it = go->childs.begin(); it != go->childs.end(); ++it)
		RecursiveUpdate(*it, dt);
}

void ModuleLevelManager::RecursiveFixedUpdate(GameObject* go, float dt) const
{
	if (!go || go->IsPendingDestroy())
		return;
	go->OnFixedUpdate(dt);
	for (list<GameObject*>::const_iterator it = go->childs.begin(); it != go->childs.end(); ++it)
		RecursiveFixedUpdate(*it, dt);
}

void ModuleLevelManager::RecursiveLateUpdate(GameObject* go, float dt) const
{
	if (!go || go->IsPendingDestroy())
		return;
	go->OnLateUpdate(dt);
	for (list<GameObject*>::const_iterator it = go->childs.begin(); it != go->childs.end(); ++it)
		RecursiveLateUpdate(*it, dt);
}

void ModuleLevelManager::RecursiveFlushPendingComponentRemovals(
	GameObject* go) const
{
	if (!go)
		return;

	for (auto component = go->components.begin();
		component != go->components.end();)
	{
		if (!(*component)->flag_for_removal)
		{
			++component;
			continue;
		}

		Component* removed = *component;
		if (App->renderer3D)
		{
			ComponentCamera* fallback =
				App->camera ? App->camera->GetDummy() : nullptr;
			if (App->renderer3D->active_camera == removed)
				App->renderer3D->active_camera = fallback;
			if (App->renderer3D->culling_camera == removed)
				App->renderer3D->culling_camera = fallback;
		}

		removed->OnFinish();
		delete removed;
		component = go->components.erase(component);
	}

	for (GameObject* child : go->childs)
		RecursiveFlushPendingComponentRemovals(child);
}

GameObject* ModuleLevelManager::RecursiveFind(
	uint uid,
	GameObject* go,
	bool includePending) const
{
	if (!go || (!includePending && go->IsPendingDestroy()))
		return nullptr;
	if (uid == go->GetUID())
		return go;

	GameObject* ret = nullptr;

	for (list<GameObject*>::const_iterator it = go->childs.begin(); it != go->childs.end() && ret == nullptr; ++it)
		ret = RecursiveFind(uid, *it, includePending);

	return ret;
}

GameObject* ModuleLevelManager::RecursiveFind(
	const char* objectName,
	GameObject* go) const
{
	if (!go || go->IsPendingDestroy())
		return nullptr;
	if (go != root && go->name == objectName)
		return go;

	for (GameObject* child : go->childs)
	{
		if (GameObject* found = RecursiveFind(objectName, child))
			return found;
	}
	return nullptr;
}

GameObject * ModuleLevelManager::Validate(const GameObject * pointer) const
{
	return pointer
		? RecursiveValidate(pointer, root)
		: nullptr;
}

GameObject* ModuleLevelManager::RecursiveValidate(
	const GameObject* pointer,
	GameObject* gameObject) const
{
	if (!gameObject || gameObject->IsPendingDestroy())
		return nullptr;
	if (pointer == gameObject)
		return gameObject;

	for (GameObject* child : gameObject->childs)
	{
		if (GameObject* found = RecursiveValidate(pointer, child))
			return found;
	}
	return nullptr;
}

GameObject* ModuleLevelManager::CastRay(const LineSegment& segment, float& dist) const
{
	dist = inf;
	GameObject* candidate = nullptr;
	RecursiveTestRay(segment, dist, &candidate);
	return candidate;
}

void ModuleLevelManager::RecursiveTestRayBBox(const LineSegment & segment, float & dist, float3 & normal, GameObject ** best_candidate) const
{
	map<float, GameObject*> objects;
	quadtree.CollectIntersections(objects, segment);

	for (map<float, GameObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
	{
		// Look for meshes
		GameObject* go = it->second;
		if (go->HasComponent(Component::Types::MeshRenderer) == true)
		{
			float closer = inf;
			*best_candidate = (GameObject*) go;
			dist = it->first;

			// let's find out the plane that hit the segment and fill in the normal
			for (int i = 0; i < 6; ++i)
			{
				Plane p(go->global_bbox.FacePlane(i));
				float d;
				if (p.Intersects(segment, &d))
				{
					if (d < closer)
						normal = p.normal;
				}
			}
		}
	}
}

void ModuleLevelManager::RecursiveTestRay(const LineSegment& segment, float& dist, GameObject** best_candidate) const
{
	map<float, GameObject*> objects;
	quadtree.CollectIntersections(objects, segment);

	for (map<float, GameObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
	{
		// Look for meshes, nothing else can be "picked" from the screen
		GameObject* go = it->second;
		vector<Component*> meshes;
		go->FindComponents(Component::Types::MeshRenderer, meshes);

		if (meshes.size() > 0)
		{
			const ComponentMeshRenderer* cmesh = (const ComponentMeshRenderer*)meshes[0];
			const ResourceMesh* mesh = cmesh->GetMeshRes();

			// test all triangles
			LineSegment segment_local_space(segment);
			segment_local_space.Transform(go->GetGlobalTransformation().Inverted());

			Triangle tri;
			for (uint i = 0; i < mesh->num_indices;)
			{
				tri.a = mesh->src_vertices[mesh->src_indices[i++]*3];
				tri.b = mesh->src_vertices[mesh->src_indices[i++]*3];
				tri.c = mesh->src_vertices[mesh->src_indices[i++]*3];

				float distance;
				float3 hit_point;
				if (segment_local_space.Intersects(tri, &distance, &hit_point))
				{
					if (distance < dist)
					{
						dist = distance;
						*best_candidate = (GameObject*) go;
					}
				}
			}
		}
	}
}

GameObject* ModuleLevelManager::CastRay(const Ray & ray, float& dist) const
{
	dist = inf;
	GameObject* candidate = nullptr;
	RecursiveTestRay(ray, dist, &candidate);
	return candidate;
}

GameObject * ModuleLevelManager::CastRayOnBoundingBoxes(const LineSegment & segment, float & dist, float3 & normal) const
{
	dist = inf;
	normal = float3::zero;
	GameObject* candidate = nullptr;
	RecursiveTestRayBBox(segment, dist, normal, &candidate);
	return candidate;
}

void ModuleLevelManager::RecursiveTestRay(const Ray& ray, float& dist, GameObject** best_candidate) const
{
	map<float, GameObject*> objects;
	quadtree.CollectIntersections(objects, ray);

	for (map<float, GameObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
	{
		// Look for meshes, nothing else can be "picked" from the screen
		GameObject* go = it->second;

		vector<Component*> meshes;
		go->FindComponents(Component::Types::MeshRenderer, meshes);

		if (meshes.size() > 0)
		{
			const ComponentMeshRenderer* cmesh = (const ComponentMeshRenderer*)meshes[0];
			const ResourceMesh* mesh = (ResourceMesh*) cmesh->GetMeshRes();

			// test all triangles
			Ray ray_local_space(ray);
			ray_local_space.Transform(go->GetGlobalTransformation().Inverted());
			ray_local_space.dir.Normalize();

			// Experiment using a TriangleMesh instead of raw triangles
			Triangle tri;
			for (uint i = 0; i < mesh->num_indices;)
			{
				tri.a = mesh->src_vertices[mesh->src_indices[i++]*3];
				tri.b = mesh->src_vertices[mesh->src_indices[i++]*3];
				tri.c = mesh->src_vertices[mesh->src_indices[i++]*3];
				// TODO I got a bug twice here, looks like a problem creating the triangle

				float distance;
				float3 hit_point;
				if (ray_local_space.Intersects(tri, &distance, &hit_point))
				{
					if (distance < dist)
					{
						dist = distance;
						*best_candidate = (GameObject*) go;
					}
				}
			}
		}
	}
}

void ModuleLevelManager::FindNear(const float3 & position, float radius, std::vector<GameObject*>& results) const
{
	quadtree.CollectIntersections(results, Sphere(position, radius));
}

GameObject* ModuleLevelManager::AddModel(UID id)
{
    Resource* res = App->resources->Get(id);
	if (!res || res->GetType() != Resource::model)
		return nullptr;

    bool ok = true;
    std::vector<GameObject*> gos;

    if(ok)
    {
        ResourceModel* model = static_cast<ResourceModel*>(res);
        if (!model->LoadToMemory())
			return nullptr;

        gos.reserve(model->GetNumNodes());

        for (uint i = 0, count = model->GetNumNodes(); ok && i < count; ++i)
        {
            const ResourceModel::Node& node = model->GetNode(i);

            GameObject* parent = i == node.parent ? nullptr : gos[node.parent];
            GameObject* go = CreateGameObject(parent);

            gos.push_back(go);

            go->SetLocalTransform(node.transform);
            go->name = node.name.c_str();
        }

        for (uint i = 0, count = model->GetNumNodes(); ok && i < count; ++i)
        {
            const ResourceModel::Node& node = model->GetNode(i);
            GameObject* go = gos[i];

            for(uint j=0; j < node.renderers.size(); ++j)
            {
                ComponentMeshRenderer* mesh = new ComponentMeshRenderer(go);

                if(node.renderers[j].mesh != 0)
                {
                    ok = mesh->SetMeshRes(node.renderers[j].mesh);
                }

                if(ok && node.renderers[j].material != 0)
                {
                    ok = mesh->SetMaterialRes(node.renderers[j].material);
                }

                if (ok && node.renderers[j].skin >= 0)
                {
                    ok = mesh->SetSkinInfo(model->GetSkin(node.renderers[j].skin), gos.data());
                }

                mesh->SetBatchName(HashString("default"));

                go->components.push_back(mesh);
            }
        }

        /*if (!gos.empty())
        {
            gos.front()->name = model->GetUserResName();
        }*/

        model->Release();
    }

    GameObject* ret = nullptr;

    if(!gos.empty())
    {
        if(!ok)
        {
            gos.front()->Remove();
        }
        else
        {
            ret = gos.front();
        }
    }

    return ret;
}
