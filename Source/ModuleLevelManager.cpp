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

#include "Event.h"
#include "ThreadPool.h"

#include "IBLData.h"
#include "LightManager.h"

#include "OpenGL.h"
#include "Leaks.h"

using namespace std;

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
		RecursiveFixedUpdate(root, dt);
	return UPDATE_CONTINUE;
}

update_status ModuleLevelManager::Update(float dt)
{
    if (App->IsPlay())
    {
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
		RecursiveLateUpdate(root, dt);
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
}

GameObject * ModuleLevelManager::Duplicate(const GameObject * original)
{
	GameObject* ret = nullptr;

	if (original != nullptr)
	{
		Config save;
		save.AddArray("Game Objects");
		map<uint, uint> new_uids;
		original->Save(save, &new_uids);
	
		LoadGameObjects(save);
	}

	return ret;
}


void ModuleLevelManager::LoadGameObjects(const Config & config)
{
	int count = config.GetArrayCount("Game Objects");
	map<GameObject*, uint> relations;
    std::vector<GameObject*> gos;
    gos.resize(count);

	for (int i = 0; i < count; ++i)
	{
		GameObject* go = CreateGameObject();
        Config goCfg = config.GetArray("Game Objects", i);
		go->Load(&goCfg, relations);
        gos[i] = go;
	}

    for (int i=0; i< count; ++i)
    {
        Config goCfg = config.GetArray("Game Objects", i);
        gos[i]->LoadComponents(&goCfg);
    }


	// Second pass to tide up the hierarchy
	for (map<GameObject*, uint>::iterator it = relations.begin(); it != relations.end(); ++it)
	{
		uint parent_id = it->second;
		GameObject* go = it->first;

		if (parent_id > 0)
		{
			GameObject* parent_go = Find(parent_id);
			if (parent_go != nullptr)
				go->SetNewParent(parent_go);
		}
	}

	// Reset all info about the level (this also fill in the quadtree)
	root->RecursiveCalcGlobalTransform(root->GetLocalTransform(), true);
	root->RecursiveCalcBoundingBoxes();
	
	// Third pass: call OnStart on all new GameObjects
	for (map<GameObject*, uint>::iterator it = relations.begin(); it != relations.end(); ++it)
		it->first->OnStart();
}

bool ModuleLevelManager::Load(const char * file)
{
	bool ret = false;

	if (file != nullptr)
	{
		int len = int(strlen(file));

		char* buffer = nullptr;
		uint size = App->fs->Load(file, &buffer);

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
				ret = true;
			}
        }

		RELEASE_ARRAY(buffer); 
	}

	return ret;
}

bool ModuleLevelManager::Save(const char * file)
{
	bool ret = true;

	Config save;

	// Add header info
	Config desc(save.AddSection("Description"));
	desc.AddString("Name", name.c_str());
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

	// Finally save to file
	char* buf = nullptr;
	uint size = uint(save.Save(&buf, "Level save file from EDU Engine"));
	App->fs->Save(file, buf, size);
	RELEASE_ARRAY(buf);

	return ret;
}

void ModuleLevelManager::UnloadCurrent()
{
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
	if (pointer == root)
		return root;

	for (list<GameObject*>::const_iterator it = root->childs.begin(); it != root->childs.end(); ++it)
		if (pointer == *it)
			return (GameObject *) pointer;

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

    bool ok = res->GetType() == Resource::model;
    std::vector<GameObject*> gos;

    if(ok)
    {
        ResourceModel* model = static_cast<ResourceModel*>(res);
        model->LoadToMemory();

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
