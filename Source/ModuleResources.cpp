#include "Globals.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleFileSystem.h"
#include "ModuleAudio.h"
#include "Event.h"
#include "ResourceTexture.h"
#include "ResourceMaterial.h"
#include "ResourceMesh.h"
#include "ResourceAudio.h"
#include "ResourceModel.h"
#include "ResourceAnimation.h"
#include "ResourceStateMachine.h"
#include "Config.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>

#include "OpenGL.h"

#define LAST_UID_FILE "LAST_UID"

using namespace std;

namespace
{
	std::string NormalizeResourceSourcePath(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		std::transform(
			path.begin(),
			path.end(),
			path.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		while (path.starts_with("./"))
			path.erase(0, 2);
		return path;
	}

	bool ResolveNativeImportSource(
		const char* sourceFile,
		std::string& nativePath)
	{
		if (!sourceFile || sourceFile[0] == '\0')
			return false;

		std::filesystem::path source =
			std::filesystem::path(sourceFile).lexically_normal();
		if (!source.is_absolute())
		{
			if (!App->GetActiveProject())
				return false;
			source = App->fs->GetProjectRoot() / source;
		}

		std::error_code error;
		source = std::filesystem::absolute(source, error).lexically_normal();
		if (error || !std::filesystem::is_regular_file(source, error))
			return false;

		nativePath = source.string();
		return true;
	}

	bool PrepareSourceAssetPath(
		const char* sourceFile,
		std::string& normalizedPath,
		std::string& error)
	{
		if (!sourceFile || sourceFile[0] == '\0')
		{
			error = "The source asset path is empty.";
			return false;
		}

		const std::filesystem::path path =
			std::filesystem::path(sourceFile).lexically_normal();
		const std::string genericPath = path.generic_string();
		if (path.is_absolute() ||
			genericPath == "Assets" ||
			!genericPath.starts_with("Assets/") ||
			genericPath.find("..") != std::string::npos)
		{
			error =
				"Assets can only be created inside the project's Assets folder.";
			return false;
		}

		normalizedPath = genericPath;
		if (App->fs->Exists(normalizedPath.c_str()))
		{
			error = "An asset with this name already exists.";
			return false;
		}
		return true;
	}

	const char* GetMeshShapeName(
		ModuleResources::ProceduralMeshShape shape)
	{
		switch (shape)
		{
		case ModuleResources::ProceduralMeshShape::Plane: return "Plane";
		case ModuleResources::ProceduralMeshShape::Cube: return "Cube";
		case ModuleResources::ProceduralMeshShape::Sphere: return "Sphere";
		case ModuleResources::ProceduralMeshShape::Cylinder: return "Cylinder";
		case ModuleResources::ProceduralMeshShape::Cone: return "Cone";
		case ModuleResources::ProceduralMeshShape::Torus: return "Torus";
		}
		return "Unknown";
	}

	bool TryGetMeshShape(
		const char* name,
		ModuleResources::ProceduralMeshShape& shape)
	{
		if (!name)
			return false;
		if (_stricmp(name, "Plane") == 0)
			shape = ModuleResources::ProceduralMeshShape::Plane;
		else if (_stricmp(name, "Cube") == 0)
			shape = ModuleResources::ProceduralMeshShape::Cube;
		else if (_stricmp(name, "Sphere") == 0)
			shape = ModuleResources::ProceduralMeshShape::Sphere;
		else if (_stricmp(name, "Cylinder") == 0)
			shape = ModuleResources::ProceduralMeshShape::Cylinder;
		else if (_stricmp(name, "Cone") == 0)
			shape = ModuleResources::ProceduralMeshShape::Cone;
		else if (_stricmp(name, "Torus") == 0)
			shape = ModuleResources::ProceduralMeshShape::Torus;
		else
			return false;
		return true;
	}

	bool ValidateMeshSettings(
		const ModuleResources::ProceduralMeshSettings& settings,
		std::string& error)
	{
		if (settings.width <= 0.0f ||
			settings.height <= 0.0f ||
			settings.radius <= 0.0f ||
			settings.innerRadius <= 0.0f ||
			settings.outerRadius <= 0.0f ||
			settings.slices < 3 ||
			settings.stacks < 1)
		{
			error =
				"Mesh dimensions must be positive and use at least 3 slices.";
			return false;
		}
		return true;
	}

	void BuildProceduralMeshDocument(
		Config& document,
		UID uid,
		const char* name,
		const ModuleResources::ProceduralMeshSettings& settings)
	{
		Config asset = document.AddSection("Asset");
		asset.AddUInt("Version", 1);
		asset.AddString("Type", "ProceduralMesh");
		asset.AddUID("UID", uid);
		asset.AddString("Name", name);
		asset.AddString("Shape", GetMeshShapeName(settings.shape));
		asset.AddFloat("Width", settings.width);
		asset.AddFloat("Height", settings.height);
		asset.AddFloat("Radius", settings.radius);
		asset.AddFloat("InnerRadius", settings.innerRadius);
		asset.AddFloat("OuterRadius", settings.outerRadius);
		asset.AddUInt("Slices", settings.slices);
		asset.AddUInt("Stacks", settings.stacks);
	}
}

ModuleResources::ModuleResources(bool start_enabled) : Module("Resource Manager", start_enabled), asset_folder(ASSETS_FOLDER)
{
}

// Destructor
ModuleResources::~ModuleResources()
{
}

// Called before render is available
bool ModuleResources::Init(Config* config)
{
	LOG("Loading Resource Manager");
	bool ret = true;

	if (App->GetActiveProject())
	{
		LoadUID();
		LoadResources();
	}
    
	return ret;
}

bool ModuleResources::Start(Config * config)
{
	// Load preset for checkers texture
	checkers = (ResourceTexture*) CreateNewResource(Resource::Type::texture, 2);
	checkers->LoadCheckers();
	checkers->loaded = 1;

	white_fallback = new ResourceTexture(0); 
    black_fallback = new ResourceTexture(1);

    white_fallback->LoadFallback(white_fallback, float3(1.0f));
    black_fallback->LoadFallback(black_fallback, float3(0.0f));

    LoadDefaultLoopNoise();
    LoadDefaultBlueNoise();
	LoadDefaultSkybox();
    LoadDefaultSphere();
	LoadDefaultBox();
	LoadDefaultPlane();
    LoadDefaultCylinder();
    LoadDefaultCone();
    LoadDefaultLUT();
    //LoadDefaultRedImage();

	//LoadDefaultBox();

	return true;
}

// Called before quitting
bool ModuleResources::CleanUp()
{
	LOG("Unloading Resource Manager");

	SaveResources();

	for (map<UID, Resource*>::iterator it = resources.begin(); it != resources.end(); ++it)
		RELEASE(it->second);

	for (vector<Resource*>::iterator it = removed.begin(); it != removed.end(); ++it)
		RELEASE(*it);

	resources.clear();
	removed.clear();

	delete white_fallback;
    delete black_fallback;
	white_fallback = nullptr;
	black_fallback = nullptr;
	checkers = nullptr;
	skybox = nullptr;
	blueNoise = nullptr;
	loopNoise = nullptr;
	cube = nullptr;
	sphere = nullptr;
	plane = nullptr;
	cylinder = nullptr;
	cone = nullptr;
	lut.reset();

	return true;
}

void ModuleResources::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
		case Event::file_dropped:
			LOG("File dropped: %s", event.string.ptr);
			ImportFileOutsideVFM(event.string.ptr);
		break;
	}
}

void ModuleResources::SaveTypedResources(Resource::Type type)
{
    for (map<UID, Resource*>::const_iterator it = resources.begin(); it != resources.end(); )
    {
        if (it->first > RESERVED_RESOURCES && it->second->GetType() == type)
        {
            if(type == Resource::texture && !static_cast<ResourceTexture*>(it->second)->Save())
            {
                it = resources.erase(it);
            }
            else if(type == Resource::model && !static_cast<ResourceModel*>(it->second)->Save())
            {
                it = resources.erase(it);
            }
            else if(type == Resource::mesh)
            {
                ResourceMesh* mesh = static_cast<ResourceMesh*>(it->second);
                mesh->LoadInMemory();
                if(!mesh->Save())
                {
                    it = resources.erase(it);
                }
				else
				{
					++it;
				}
                mesh->Release();
            }
            else
            {
                ++it;
            }
        }
        else
        {
            ++it;
        }
    }

    SaveResources();
}

void ModuleResources::SaveResources() const
{
	if (!App->GetActiveProject())
		return;

	Config save;

	// Add header info
	Config desc(save.AddSection("Header"));

	// Serialize GameObjects recursively
	save.AddArray("Resources");

	for (map<UID, Resource*>::const_iterator it = resources.begin(); it != resources.end(); ++it)
	{
		if (it->first > RESERVED_RESOURCES)
		{
			Config resource;
			it->second->Save(resource);

			save.AddArrayEntry(resource);
		}
	}

	// Finally save to file
	char* buf = nullptr;
	uint size = uint(save.Save(&buf, "Resources setup from the EDU Engine"));
	App->fs->Save(SETTINGS_FOLDER "resources.json", buf, size);
	RELEASE_ARRAY(buf);
}

void ModuleResources::SaveResourcesTo(const char* path)
{
	bool ret = true;

	// Make sure standard paths exist
	const char* dirs[] = {
		LIBRARY_FOLDER, SETTINGS_FOLDER, 
		LIBRARY_AUDIO_FOLDER, LIBRARY_MESH_FOLDER,
		LIBRARY_MATERIAL_FOLDER, LIBRARY_SCENE_FOLDER, LIBRARY_MODEL_FOLDER, 
		LIBRARY_TEXTURES_FOLDER, LIBRARY_ANIMATION_FOLDER, LIBRARY_STATE_MACHINE_FOLDER,
	};

    char tmp[256];
    char tmp2[256];

	for (uint i = 0; i < sizeof(dirs)/sizeof(const char*); ++i)
	{
		sprintf_s(tmp, 255, "/%s%s", path, dirs[i]);
		if (App->fs->Exists(tmp) == 0)
			App->fs->CreateDirectory(tmp);
	}

	Config save;

	// Add header info
	Config desc(save.AddSection("Header"));

	// Serialize GameObjects recursively
	save.AddArray("Resources");

	for (map<UID, Resource*>::const_iterator it = resources.begin(); it != resources.end(); ++it)
	{
		if (it->first > RESERVED_RESOURCES)
		{
			Config resource;
			it->second->Save(resource);

            sprintf_s(tmp, 255, "%s%s", GetDirByType(it->second->GetType()), it->second->GetExportedFile());
            sprintf_s(tmp2, 255, "/%s%s%s", path, GetDirByType(it->second->GetType()), it->second->GetExportedFile());
            App->fs->Copy(tmp, tmp2);

			save.AddArrayEntry(resource);
		}
	}

	// Finally save to file
	char* buf = nullptr;
	uint size = uint(save.Save(&buf, "Resources setup from the EDU Engine"));

    sprintf_s(tmp, 255, "/%s%s%s", path, SETTINGS_FOLDER, "resources.json");
    App->fs->Save(tmp, buf, size);
	RELEASE_ARRAY(buf);
}

const char* ModuleResources::GetDirByType(Resource::Type type) const
{
    static_assert(Resource::Type::unknown == 7, "String list needs update");

	static const char* dirs_by_type[] = {
		LIBRARY_MODEL_FOLDER, LIBRARY_MATERIAL_FOLDER, LIBRARY_TEXTURES_FOLDER, 
        LIBRARY_MESH_FOLDER, LIBRARY_AUDIO_FOLDER, LIBRARY_ANIMATION_FOLDER, 
        LIBRARY_STATE_MACHINE_FOLDER, LIBRARY_SCENE_FOLDER
	};

    return dirs_by_type[type];
}

void ModuleResources::LoadResources()
{
	char* buffer = nullptr;
	uint size = App->fs->Load(SETTINGS_FOLDER "resources.json", &buffer);

	if (buffer != nullptr && size > 0)
	{
		Config config(buffer);

		// Load level description
		Config desc(config.GetSection("Header"));

		int count = config.GetArrayCount("Resources");
		for (int i = 0; i < count; ++i)
		{
			Config resource(config.GetArray("Resources", i));
			Resource::Type type = (Resource::Type) resource.GetInt("Type");
			UID uid = resource.GetUID("UID");

			if (Get(uid) != nullptr)
			{
				LOG("Skipping suplicated resource id %llu", uid);
				continue;
			}

			Resource* res = CreateNewResource(type, uid);
			res->Load(config.GetArray("Resources", i));
		}
		RELEASE_ARRAY(buffer); 
	}
}

void ModuleResources::UnloadProjectResources()
{
	for (auto iterator = resources.begin(); iterator != resources.end();)
	{
		if (iterator->first > RESERVED_RESOURCES)
		{
			RELEASE(iterator->second);
			iterator = resources.erase(iterator);
		}
		else
		{
			++iterator;
		}
	}

	for (Resource*& resource : removed)
		RELEASE(resource);
	removed.clear();
	last_uid = RESERVED_RESOURCES + 1;
}

void ModuleResources::LoadProjectResources()
{
	last_uid = RESERVED_RESOURCES + 1;

	const std::filesystem::path settings_directory =
		App->fs->GetProjectRoot() / "Settings";
	const std::filesystem::path uid_file =
		settings_directory / LAST_UID_FILE;
	const std::filesystem::path registry_file =
		settings_directory / "resources.json";

	if (std::filesystem::is_regular_file(uid_file))
		LoadUID();
	else
		SaveUID();

	if (!std::filesystem::is_regular_file(registry_file))
		SaveResources();

	LoadResources();
}

Resource::Type ModuleResources::TypeFromExtension(const char * extension) const
{
	Resource::Type ret = Resource::unknown;

	if (extension != nullptr)
	{
		if (_stricmp(extension, "wav") == 0)
			ret = Resource::audio;
		else if (_stricmp(extension, "ogg") == 0)
			ret = Resource::audio;
		else if (_stricmp(extension, "dds") == 0)
			ret = Resource::texture;
		else if (_stricmp(extension, "png") == 0)
			ret = Resource::texture;
		else if (_stricmp(extension, "jpg") == 0)
			ret = Resource::texture;
		else if (_stricmp(extension, "tga") == 0)
			ret = Resource::texture;
		else if (_stricmp(extension, "tif") == 0)
			ret = Resource::texture;
		else if (_stricmp(extension, "fbx") == 0)
			ret = Resource::model;
		else if (_stricmp(extension, "dae") == 0)
			ret = Resource::model;
	}

	return ret;
}

UID ModuleResources::Find(const char * file_in_assets) const
{
	string file(file_in_assets);
	App->fs->NormalizePath(file);

	for (map<UID, Resource*>::const_iterator it = resources.begin(); it != resources.end(); ++it)
	{
		if (it->second->file.compare(file) == 0)
			return it->first;
	}
	return 0;
}


UID ModuleResources::ImportFileOutsideVFM(const char * full_path)
{
	UID ret = 0;

	string final_path;

	App->fs->SplitFilePath(full_path, nullptr, &final_path);
	final_path = asset_folder + final_path;

	if (App->fs->CopyFromOutsideFS(full_path, final_path.c_str()) == true)
	{
		std::string extension;
		App->fs->SplitFilePath(full_path, nullptr, nullptr, &extension);
		Resource::Type type = TypeFromExtension(extension.c_str());
		ret = ImportFile(final_path.c_str(), type);
	}

	return ret;
}


UID ModuleResources::ImportFile(const char * new_file_in_assets, Resource::Type type, bool force)
{
	UID ret = 0;

	// Check is that file has been already exported
	if (force == true)
	{
		ret = Find(new_file_in_assets);

		if (ret != 0)
			return ret;
	}

	bool import_ok = false;
	string written_file;

	switch (type)
	{
		case Resource::texture:
			import_ok = ResourceTexture::Import(new_file_in_assets, written_file, false, false);
		break;
		case Resource::audio:
			import_ok = App->audio->Import(new_file_in_assets, written_file);
		break;
		case Resource::model:
			import_ok = ResourceModel::Import(new_file_in_assets, 1.0, written_file);
		break;
		//case Resource::animation:
			//import_ok = ResourceAnimation::Import(new_file_in_assets, 0, UINT_MAX, 1.0, written_file);
        break;
	}

	// If export was successfull, create a new resource
	if (import_ok == true)
	{
        std::string user_name;
        App->fs->SplitFilePath(new_file_in_assets, nullptr, &user_name);
        ret = ImportSuccess(type, new_file_in_assets, user_name.c_str(), written_file);
	}
	else
		LOG("Importing of [%s] FAILED", new_file_in_assets);

	return ret;
}

UID ModuleResources::ImportTexture(const char* file_name, bool mipmaps, bool srgb, bool toCubemap)
{
	EGE::TextureImportOptions options;
	options.generateMipmaps = mipmaps;
	options.sRgb = srgb;
	options.convertToCubemap = toCubemap;
	return ImportTexture(file_name, options);
}

UID ModuleResources::ImportTexture(
	const char* fileName,
	const EGE::TextureImportOptions& options)
{
	UID ret = 0;
    bool import_ok = false;
    string written_file;

    import_ok = ResourceTexture::Import(
		fileName, written_file, options);

	// If export was successfull, create a new resource
	if (import_ok == true)
	{
        ret = ImportSuccess(Resource::texture, fileName, "", written_file);
        ResourceTexture* texture = static_cast<ResourceTexture*>(Get(ret));
        texture->SetColorSpace(
			options.sRgb ? ColorSpace_gamma : ColorSpace_linear);
		texture->SetImportOptions(options);
	}
	else
    {
		LOG("Importing of [%s] FAILED", fileName);
    }

	SaveResources();

	return ret;
}

UID ModuleResources::ImportAudio(
	const char* fileName,
	const EGE::AudioImportOptions& options)
{
	std::string writtenFile;
	if (!App->audio->Import(fileName, writtenFile))
		return 0;

	const UID uid = ImportSuccess(
		Resource::audio,
		fileName,
		options.assetName.c_str(),
		writtenFile);
	ResourceAudio* audio =
		static_cast<ResourceAudio*>(Get(uid));
	if (!audio)
		return 0;

	switch (options.mode)
	{
	case EGE::AudioImportMode::Sample:
		audio->format = ResourceAudio::sample;
		break;
	case EGE::AudioImportMode::Stream:
		audio->format = ResourceAudio::stream;
		break;
	default:
		audio->format = ResourceAudio::unknown;
		break;
	}
	audio->loop = options.loop;
	audio->volume = options.volume;
	audio->pitch = options.pitch;
	audio->spatial = options.spatial;
	audio->minimumDistance = options.distanceRange.x;
	audio->maximumDistance = options.distanceRange.y;
	SaveResources();
	return uid;
}

UID ModuleResources::ImportAnimation(const char* file_name, uint first, uint last, const char* user_name, float scale)
{
	EGE::AnimationImportOptions options;
	options.scale = float3(scale);
	return ImportAnimation(
		file_name, first, last, user_name, options);
}

UID ModuleResources::ImportAnimation(
	const char* fileName,
	uint first,
	uint last,
	const char* userName,
	const EGE::AnimationImportOptions& options)
{
	UID ret = 0;
    bool import_ok = false;
    vector<string> written_files;
	std::string nativePath;
	if (!ResolveNativeImportSource(fileName, nativePath))
	{
		LOG("Animation import source [%s] is not available",
			fileName ? fileName : "");
		return 0;
	}

    import_ok = ResourceAnimation::Import(
		nativePath.c_str(), first, last, options, written_files);

	// If export was successfull, create a new resource
	if (import_ok == true)
	{
        for (const string& written_file : written_files)
        {
            ret = ImportSuccess(
				Resource::animation, fileName, userName, written_file);
			if (ResourceAnimation* animation =
					static_cast<ResourceAnimation*>(Get(ret)))
			{
				animation->SetImportOptions(options, first, last);
			}
        }
	}
    else
    {
		LOG("Importing of [%s] FAILED", fileName);
    }

	if (ret != 0)
	{
		MakeResourceSourcePathsPortable();
		SaveResources();
	}
	return ret;
}

UID ModuleResources::ImportModel(const char* file_name, float scale, const char* user_name)
{
	EGE::ModelImportOptions options;
	options.assetName = user_name ? user_name : "";
	options.scale = float3(scale);
	return ImportModel(file_name, options);
}

UID ModuleResources::ImportModel(
	const char* fileName,
	const EGE::ModelImportOptions& options)
{
	UID ret = 0;
    bool import_ok = false;
    string written_file;
	std::string nativePath;
	if (!ResolveNativeImportSource(fileName, nativePath))
	{
		LOG("Model import source [%s] is not available",
			fileName ? fileName : "");
		return 0;
	}

    import_ok = ResourceModel::Import(
		nativePath.c_str(), options, written_file);

	if (import_ok == true)
	{
        ret = ImportSuccess(
			Resource::model,
			fileName,
			options.assetName.c_str(),
			written_file);
		if (ResourceModel* model =
				static_cast<ResourceModel*>(Get(ret)))
		{
			model->SetImportOptions(options);
		}
    }
    else
    {
		LOG("Importing of [%s] FAILED", fileName);
    }

	if (ret != 0)
	{
		MakeResourceSourcePathsPortable();
		SaveResources();
	}
	return ret;
}

ModuleResources::AssetCreationResult ModuleResources::CreateMaterialAsset(
	const char* sourceFile,
	const char* name,
	MaterialAssetWorkflow workflow)
{
	AssetCreationResult result;
	std::string sourcePath;
	if (!name || name[0] == '\0')
	{
		result.error = "Enter a material name.";
		return result;
	}
	if (!PrepareSourceAssetPath(sourceFile, sourcePath, result.error))
		return result;

	ResourceMaterial* material = static_cast<ResourceMaterial*>(
		CreateNewResource(Resource::material));
	material->file = sourcePath;
	material->user_name = name;
	if (workflow == MaterialAssetWorkflow::SpecularGlossiness)
		material->SetSpecularGlossData(SpecularGlossData{});
	else
		material->SetMetallicRoughData(MetallicRoughData{});

	if (!material->Save())
	{
		result.error = "The material library file could not be written.";
		RemoveResource(material->GetUID());
		return result;
	}

	Config document;
	Config asset = document.AddSection("Asset");
	asset.AddUInt("Version", 1);
	asset.AddString("Type", "Material");
	asset.AddUID("UID", material->GetUID());
	asset.AddString("Name", name);
	asset.AddString(
		"Workflow",
		workflow == MaterialAssetWorkflow::SpecularGlossiness
			? "SpecularGlossiness"
			: "MetallicRoughness");
	if (!SaveSourceAsset(sourcePath.c_str(), document))
	{
		result.error = "The material source file could not be written.";
		RemoveResource(material->GetUID());
		return result;
	}

	if (App->GetActiveProject())
		SaveResources();
	result.uid = material->GetUID();
	result.sourcePath = sourcePath;
	return result;
}

ModuleResources::AssetCreationResult
ModuleResources::CreateStateMachineAsset(
	const char* sourceFile,
	const char* name)
{
	AssetCreationResult result;
	std::string sourcePath;
	if (!name || name[0] == '\0')
	{
		result.error = "Enter a state machine name.";
		return result;
	}
	if (!PrepareSourceAssetPath(sourceFile, sourcePath, result.error))
		return result;

	ResourceStateMachine* stateMachine =
		static_cast<ResourceStateMachine*>(
			CreateNewResource(Resource::state_machine));
	stateMachine->file = sourcePath;
	stateMachine->user_name = name;
	if (!stateMachine->Save())
	{
		result.error = "The state machine library file could not be written.";
		RemoveResource(stateMachine->GetUID());
		return result;
	}

	Config document;
	Config asset = document.AddSection("Asset");
	asset.AddUInt("Version", 1);
	asset.AddString("Type", "AnimationStateMachine");
	asset.AddUID("UID", stateMachine->GetUID());
	asset.AddString("Name", name);
	asset.AddUInt("DefaultNode", 0);
	if (!SaveSourceAsset(sourcePath.c_str(), document))
	{
		result.error = "The state machine source file could not be written.";
		RemoveResource(stateMachine->GetUID());
		return result;
	}

	SaveResources();
	result.uid = stateMachine->GetUID();
	result.sourcePath = sourcePath;
	return result;
}

ModuleResources::AssetCreationResult
ModuleResources::CreateProceduralMeshAsset(
	const char* sourceFile,
	const char* name,
	const ProceduralMeshSettings& settings)
{
	AssetCreationResult result;
	std::string sourcePath;
	if (!name || name[0] == '\0')
	{
		result.error = "Enter a mesh name.";
		return result;
	}
	if (!PrepareSourceAssetPath(sourceFile, sourcePath, result.error))
		return result;
	if (!ValidateMeshSettings(settings, result.error))
		return result;

	const UID requestedUid = GenerateNewUID();
	UID uid = 0;
	switch (settings.shape)
	{
	case ProceduralMeshShape::Plane:
		uid = ResourceMesh::LoadPlane(
			name,
			settings.width,
			settings.height,
			settings.slices,
			settings.stacks,
			requestedUid);
		break;
	case ProceduralMeshShape::Cube:
		uid = ResourceMesh::LoadCube(
			name,
			settings.width,
			requestedUid);
		break;
	case ProceduralMeshShape::Sphere:
		uid = ResourceMesh::LoadSphere(
			name,
			settings.radius,
			settings.slices,
			settings.stacks,
			requestedUid);
		break;
	case ProceduralMeshShape::Cylinder:
		uid = ResourceMesh::LoadCylinder(
			name,
			settings.height,
			settings.radius,
			settings.slices,
			settings.stacks,
			requestedUid);
		break;
	case ProceduralMeshShape::Cone:
		uid = ResourceMesh::LoadCone(
			name,
			settings.height,
			settings.radius,
			settings.slices,
			settings.stacks,
			requestedUid);
		break;
	case ProceduralMeshShape::Torus:
		uid = ResourceMesh::LoadTorus(
			name,
			settings.innerRadius,
			settings.outerRadius,
			settings.slices,
			settings.stacks,
			requestedUid);
		break;
	}

	ResourceMesh* mesh = static_cast<ResourceMesh*>(Get(uid));
	if (!mesh)
	{
		result.error = "The procedural mesh could not be generated.";
		return result;
	}
	mesh->file = sourcePath;
	mesh->user_name = name;

	Config document;
	BuildProceduralMeshDocument(
		document, mesh->GetUID(), name, settings);
	if (!SaveSourceAsset(sourcePath.c_str(), document))
	{
		result.error = "The procedural mesh source file could not be written.";
		RemoveResource(mesh->GetUID());
		return result;
	}

	SaveResources();
	result.uid = mesh->GetUID();
	result.sourcePath = sourcePath;
	return result;
}

bool ModuleResources::LoadProceduralMeshSettings(
	const char* sourceFile,
	ProceduralMeshSettings& settings,
	std::string& name,
	std::string& error) const
{
	char* buffer = nullptr;
	if (!sourceFile || App->fs->Load(sourceFile, &buffer) == 0 || !buffer)
	{
		error = "The procedural mesh source file could not be read.";
		RELEASE_ARRAY(buffer);
		return false;
	}

	Config document(buffer);
	RELEASE_ARRAY(buffer);
	if (!document.IsValid())
	{
		error = "The procedural mesh source file contains invalid JSON.";
		return false;
	}

	Config asset = document.GetSection("Asset");
	if (!asset.IsValid() ||
		_stricmp(asset.GetString("Type", ""), "ProceduralMesh") != 0 ||
		!TryGetMeshShape(asset.GetString("Shape", ""), settings.shape))
	{
		error = "This source file is not a supported procedural mesh asset.";
		return false;
	}

	name = asset.GetString("Name", "Procedural Mesh");
	settings.width = asset.GetFloat("Width", 1.0f);
	settings.height = asset.GetFloat("Height", 1.0f);
	settings.radius = asset.GetFloat("Radius", 0.5f);
	settings.innerRadius = asset.GetFloat("InnerRadius", 0.35f);
	settings.outerRadius = asset.GetFloat("OuterRadius", 1.0f);
	settings.slices = asset.GetUInt("Slices", 24);
	settings.stacks = asset.GetUInt("Stacks", 12);
	return ValidateMeshSettings(settings, error);
}

bool ModuleResources::UpdateProceduralMeshAsset(
	UID uid,
	const char* sourceFile,
	const char* name,
	const ProceduralMeshSettings& settings,
	std::string& error)
{
	Resource* resource = Get(uid);
	if (!resource || resource->GetType() != Resource::mesh)
	{
		error = "The procedural mesh resource is no longer available.";
		return false;
	}
	if (!sourceFile || !name || name[0] == '\0')
	{
		error = "The procedural mesh needs a name and a source file.";
		return false;
	}
	if (!ValidateMeshSettings(settings, error))
		return false;

	ResourceMesh* mesh = static_cast<ResourceMesh*>(resource);
	const std::string previousName = mesh->name;
	const std::string previousUserName = mesh->user_name;
	mesh->name = name;
	mesh->user_name = name;
	bool regenerated = false;
	switch (settings.shape)
	{
	case ProceduralMeshShape::Plane:
		regenerated = mesh->RegeneratePlane(
			settings.width, settings.height,
			settings.slices, settings.stacks);
		break;
	case ProceduralMeshShape::Cube:
		regenerated = mesh->RegenerateCube(settings.width);
		break;
	case ProceduralMeshShape::Sphere:
		regenerated = mesh->RegenerateSphere(
			settings.radius, settings.slices, settings.stacks);
		break;
	case ProceduralMeshShape::Cylinder:
		regenerated = mesh->RegenerateCylinder(
			settings.height, settings.radius,
			settings.slices, settings.stacks);
		break;
	case ProceduralMeshShape::Cone:
		regenerated = mesh->RegenerateCone(
			settings.height, settings.radius,
			settings.slices, settings.stacks);
		break;
	case ProceduralMeshShape::Torus:
		regenerated = mesh->RegenerateTorus(
			settings.innerRadius, settings.outerRadius,
			settings.slices, settings.stacks);
		break;
	}
	if (!regenerated)
	{
		mesh->name = previousName;
		mesh->user_name = previousUserName;
		error = "The procedural mesh could not be regenerated.";
		return false;
	}

	mesh->file = sourceFile;
	Config document;
	BuildProceduralMeshDocument(document, uid, name, settings);
	if (!SaveSourceAsset(sourceFile, document))
	{
		error = "The procedural mesh source file could not be written.";
		return false;
	}

	SaveResources();
	return true;
}

UID ModuleResources::ImportSuccess(Resource::Type type, const char* file_name, const char* user_name, const std::string& output)
{
    Resource* res = CreateNewResource(type);
	if (file_name != nullptr)
	{
		res->file = file_name;
		App->fs->NormalizePath(res->file);
	}

    string file;
    App->fs->SplitFilePath(output.c_str(), nullptr, &file);
    res->exported_file = file.c_str();
    LOG("Imported successful from [%s] to [%s]", res->GetFile(), res->GetExportedFile());

	if (strlen(user_name) != 0 || res->file.empty())
	{
		res->user_name = user_name;
	}
	else
	{
		App->fs->SplitFilePath(res->file.c_str(), nullptr, &res->user_name, nullptr);
	}
    
    if (res->user_name.empty())
    {
        res->user_name = res->exported_file;
    }

    size_t pos_dot = res->user_name.find_last_of(".");
    if(pos_dot != std::string::npos)
    {
        res->user_name.erase(res->user_name.begin()+pos_dot, res->user_name.end());
    }

    return res->uid;
}

UID ModuleResources::ImportBuffer(const void * buffer, uint size, Resource::Type type, const char* source_file)
{
	UID ret = 0;

	bool import_ok = false;
	string output;


	switch (type)
	{
		case Resource::texture:
			import_ok = ResourceTexture::Import(buffer, size, output, false, false);
		break;
		case Resource::mesh:
			// Old school trick: if it is a Mesh, buffer will be treated as an AiMesh*
			// TODO: this can go bad in so many ways :)
			// \todo: import_ok = App->meshes->Import((aiMesh*) buffer, output);
            
		break;
		case Resource::animation:
			// import_ok = ResourceAnimation::Import( anim_loader->Import((aiAnimation*) buffer, (UID) size, output);
		break;
	}

	// If export was successfull, create a new resource
	if (import_ok  == true)
	{
		Resource* res = CreateNewResource(type);
		if (source_file != nullptr) {
			res->file = source_file;
			App->fs->NormalizePath(res->file);
		}
		string file;
		App->fs->SplitFilePath(output.c_str(), nullptr, &file);
		res->exported_file = file;
		ret = res->uid;
		LOG("Imported successful from BUFFER [%s] to [%s]", res->GetFile(), res->GetExportedFile());
	}
	else
		LOG("Importing of BUFFER [%s] FAILED", source_file);

	return ret;
}

UID ModuleResources::GenerateNewUID()
{
	++last_uid;
	SaveUID();
	return last_uid;
}

const Resource * ModuleResources::Get(UID uid) const
{			   
	if(resources.find(uid) != resources.end())
		return resources.at(uid);
	return nullptr;
}

Resource * ModuleResources::Get(UID uid) 
{			   
	std::map<UID, Resource*>::iterator it = resources.find(uid);
	if(it != resources.end())
		return it->second;
	return nullptr;
}

Resource * ModuleResources::CreateNewResource(Resource::Type type, UID force_uid)
{
	Resource* ret = nullptr;
	UID uid;

	if (force_uid != 0 && Get(force_uid) == nullptr)
		uid = force_uid;
	else
		uid = GenerateNewUID();

	switch (type)
	{
		case Resource::texture:
			ret = (Resource*) new ResourceTexture(uid);
		break;
		case Resource::mesh:
			ret = (Resource*) new ResourceMesh(uid);
		break;
		case Resource::audio:
			ret = (Resource*) new ResourceAudio(uid);
		break;
		case Resource::animation:
			ret = (Resource*) new ResourceAnimation(uid);
		break;
        case Resource::material:
            ret = new ResourceMaterial(uid);
        break;
        case Resource::model:
            ret= new ResourceModel(uid);
            break;
        case Resource::state_machine:
            ret= new ResourceStateMachine(uid);
            break;

    }

	if (ret != nullptr)
	{
		resources[uid] = ret;
	}

	return ret;
}

void ModuleResources::GatherResourceType(std::vector<const Resource*>& resources, Resource::Type type) const
{
	for (map<UID, Resource*>::const_iterator it = this->resources.begin(); it != this->resources.end(); ++it)
	{
		if (it->second->type == type)
			resources.push_back(it->second);
	}
}

const Resource* ModuleResources::FindResourceBySourceFile(
	Resource::Type type,
	const std::string& sourceFile) const
{
	const std::string expected =
		NormalizeResourceSourcePath(sourceFile);
	for (const auto& [uid, resource] : resources)
	{
		if (resource &&
			resource->GetType() == type &&
			NormalizeResourceSourcePath(resource->GetFile()) ==
				expected)
		{
			return resource;
		}
	}
	return nullptr;
}

std::size_t ModuleResources::RemoveResourcesBySourcePath(
	const std::string& sourcePath)
{
	const std::string expected =
		NormalizeResourceSourcePath(sourcePath);
	std::vector<UID> matches;
	for (const auto& [uid, resource] : resources)
	{
		if (uid <= RESERVED_RESOURCES ||
			!resource ||
			!resource->GetFile())
		{
			continue;
		}
		if (NormalizeResourceSourcePath(resource->GetFile()) ==
			expected)
			matches.push_back(uid);
	}

	for (UID uid : matches)
		RemoveResource(uid);
	if (!matches.empty())
		SaveResources();
	return matches.size();
}

std::size_t ModuleResources::RenameResourceSourcePath(
	const std::string& oldPath,
	const std::string& newPath)
{
	const std::string expected =
		NormalizeResourceSourcePath(oldPath);
	std::size_t renamed = 0;
	for (auto& [uid, resource] : resources)
	{
		if (uid <= RESERVED_RESOURCES ||
			!resource ||
			!resource->GetFile() ||
			NormalizeResourceSourcePath(resource->GetFile()) !=
				expected)
		{
			continue;
		}
		resource->file = newPath;
		++renamed;
	}
	if (renamed > 0)
		SaveResources();
	return renamed;
}

void ModuleResources::MakeResourceSourcePathsPortable()
{
	if (!App->GetActiveProject())
		return;

	const std::filesystem::path projectRoot =
		App->fs->GetProjectRoot().lexically_normal();
	for (auto& [uid, resource] : resources)
	{
		if (!resource || resource->file.empty())
			continue;

		const std::filesystem::path source =
			std::filesystem::path(resource->file).lexically_normal();
		if (!source.is_absolute())
			continue;

		const std::filesystem::path relative =
			source.lexically_relative(projectRoot);
		const std::string relativeText = relative.generic_string();
		if (!relative.empty() &&
			!relative.is_absolute() &&
			relativeText != ".." &&
			!relativeText.starts_with("../") &&
			(relativeText == "Assets" ||
			 relativeText.starts_with("Assets/")))
		{
			resource->file = relativeText;
		}
	}
}

void ModuleResources::LoadUID()
{
	string file(SETTINGS_FOLDER);
	file += LAST_UID_FILE;

	char *buf = nullptr;
	uint size = App->fs->Load(file.c_str(), &buf);

	if (size == sizeof(last_uid))
	{
		last_uid = *((UID*)buf);
		RELEASE_ARRAY(buf);
	}
	else
	{
		LOG("WARNING! Cannot read resource UID from file [%s] - Generating a new one", file.c_str());
		SaveUID();
	}
}

void ModuleResources::SaveUID() const
{
	if (!App->GetActiveProject())
		return;

	string file(SETTINGS_FOLDER);
	file += LAST_UID_FILE;

	uint size = App->fs->Save(file.c_str(), (const char*) &last_uid, sizeof(last_uid));

	if (size != sizeof(last_uid))
		LOG("WARNING! Cannot write resource UID into file [%s]", file.c_str());
}

void ModuleResources::ReleaseFromMemory(UID uid)
{
    Resource *res = Get(uid);
    if (res != nullptr)
    {
        res->Release();
    }
}

void ModuleResources::RemoveResource(UID uid)
{
    map<UID, Resource*>::iterator it = resources.find(uid);
    if(it != resources.end())
    {
		const char* exportedFile = it->second->GetExportedFile();
		if (exportedFile && exportedFile[0] != '\0')
		{
			char tmp[256];
			sprintf_s(
				tmp,
				255,
				"%s%s",
				GetDirByType(it->second->GetType()),
				exportedFile);
			App->fs->Remove(tmp);
		}

        removed.push_back(it->second);

        resources.erase(it);
    }
}

bool ModuleResources::SaveSourceAsset(
	const char* sourceFile,
	const Config& document) const
{
	char* buffer = nullptr;
	const uint size = static_cast<uint>(
		document.Save(&buffer, "Edu Game Engine source asset"));
	const bool saved =
		buffer &&
		size > 0 &&
		App->fs->Save(sourceFile, buffer, size) == size;
	RELEASE_ARRAY(buffer);
	return saved;
}

bool ModuleResources::LoadDefaultSkybox()
{
	skybox = static_cast<ResourceTexture*>(CreateNewResource(Resource::texture, 3));

    char* buffer = nullptr;
    uint size = App->fs->Load(
		"Engine/Textures/Cubemaps/cubemap.dds", &buffer);

    if (buffer != nullptr)
    {        
        skybox->LoadFromBuffer(buffer, size);
        skybox->loaded++;
        skybox->file = "*Default Skybox*";
        skybox->exported_file = "*Default Skybox*";
        skybox->user_name = "*Default skybox*";

        delete[] buffer;

        return true;
    }

	return false;
}

bool ModuleResources::LoadDefaultLoopNoise()
{
    loopNoise = static_cast<ResourceTexture*>(CreateNewResource(Resource::texture, 8));

    char* buffer = nullptr;
    uint size = App->fs->Load("Engine/Textures/fog.png", &buffer);


    if (buffer != nullptr)
    {
        loopNoise->LoadFromBuffer(buffer, size);
        loopNoise->loaded++;
        loopNoise->file = "*Default LoopNoise*";
        loopNoise->exported_file = "*Default LoopNoise*";
        loopNoise->user_name = "*Default loop noise*";

        loopNoise->GetTexture()->SetWrapping(GL_REPEAT, GL_REPEAT, GL_REPEAT);


        delete[] buffer;

        return true;
    }

    return false;
}

bool ModuleResources::LoadDefaultBlueNoise()
{
    blueNoise = static_cast<ResourceTexture*>(CreateNewResource(Resource::texture, 9));

    char* buffer = nullptr;
    //uint size = App->fs->Load("Assets/Textures/BlueNoise/512_512/LDR_LLL1_0.png", &buffer);
    uint size = App->fs->Load(
		"Engine/Textures/BlueNoise/512_512/LDR_RGB1_0.png",
		&buffer);


    if (buffer != nullptr)
    {
        blueNoise->LoadFromBuffer(buffer, size);
        blueNoise->loaded++;
        blueNoise->file = "*Default BlueNoise*";
        blueNoise->exported_file = "*Default BlueNoise*";
        blueNoise->user_name = "*Default blue noise*";

        blueNoise->GetTexture()->SetWrapping(GL_REPEAT, GL_REPEAT, GL_REPEAT);


        delete[] buffer;

        return true;
    }

    return false;
}


bool ModuleResources::LoadDefaultRedImage()
{
    redImage = static_cast<ResourceTexture*>(CreateNewResource(Resource::texture, 5));
    
    if (redImage->LoadRedImage(redImage, 1024, 1024))
    {
        redImage->file = "*Test redimage*";
        redImage->exported_file = "*Test redimage*";
        redImage->user_name = "*Test redimage*";
        redImage->loaded = 1;        

        return true;
    }

    return false;
}

bool ModuleResources::LoadDefaultBox()
{
	cube = static_cast<ResourceMesh*>(
		Get(ResourceMesh::LoadCube("DefaultBox", 1.0f, UID(8))));

	return cube != nullptr;
}

bool ModuleResources::LoadDefaultSphere()
{
	sphere = static_cast<ResourceMesh*>(Get(ResourceMesh::LoadSphere("DefaultSphere", 1.0f, 20, 20, UID(4))));

	return sphere != nullptr;
}

bool ModuleResources::LoadDefaultPlane()
{
	plane = static_cast<ResourceMesh*>(Get(ResourceMesh::LoadPlane("DefaultPlane", 1.0f, 1.0f, 1, 1, UID(5))));

	return plane != nullptr;
}

bool ModuleResources::LoadDefaultCylinder()
{
	cylinder = static_cast<ResourceMesh*>(Get(ResourceMesh::LoadCylinder("DefaultCylinder", 1.0f, 1.0f, 20, 20, UID(6))));

	return cylinder != nullptr;
}

bool ModuleResources::LoadDefaultCone()
{
	cone = static_cast<ResourceMesh*>(Get(ResourceMesh::LoadCone("DefaultCone", 1.0f, 0.5f, 60, 40, UID(7))));

	return cone != nullptr;
}

bool ModuleResources::LoadDefaultLUT()
{
    uint size = 0;
    float* lutData = nullptr;
    //if(LoadCubeLUT("Assets/LUTs/vibrant.CUBE", lutData, size))
    if (LoadCubeLUT(
			"Engine/LUTs/purple11-free-luts-pack/Once upon a time.CUBE",
			lutData,
			size))
    //if (LoadCubeLUT("Assets/LUTs/Shutterstock Free  LUTs/SoftBlackAndWhite.CUBE", lutData, size))        
    {
        lut = std::make_unique<Texture3D>(size, size, size, GL_RGB, GL_RGB, GL_FLOAT, lutData, false);
        lut->SetWrapping(GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);

        return true;
    }

    return false;
}

bool ModuleResources::LoadCubeLUT(const char *file_path, float*& lut_data, uint& size)
{
	char* contents = nullptr;
	const uint byteCount = App->fs->Load(file_path, &contents);
	if (!contents || byteCount == 0)
	{
		LOG("Could not open LUT file %s", file_path);
		return false;
	}

	std::istringstream stream(
		std::string(contents, static_cast<std::size_t>(byteCount)));
	RELEASE_ARRAY(contents);

	lut_data = nullptr;
	size = 0;
	std::string line;
	while (std::getline(stream, line))
	{
		std::istringstream header(line);
		std::string name;
		if (header >> name && name == "LUT_3D_SIZE" && header >> size)
			break;
	}

	if (size == 0)
	{
		LOG("LUT file %s does not define LUT_3D_SIZE", file_path);
		return false;
	}

	const std::size_t entryCount =
		static_cast<std::size_t>(size) * size * size;
	lut_data = new float[entryCount * 3];
	std::size_t row = 0;
	while (row < entryCount && std::getline(stream, line))
	{
		std::istringstream values(line);
		float red = 0.0f;
		float green = 0.0f;
		float blue = 0.0f;
		if (!(values >> red >> green >> blue))
			continue;

		lut_data[row * 3] = red;
		lut_data[row * 3 + 1] = green;
		lut_data[row * 3 + 2] = blue;
		++row;
	}

	if (row != entryCount)
	{
		LOG(
			"LUT file %s contains %zu entries, expected %zu",
			file_path,
			row,
			entryCount);
		RELEASE_ARRAY(lut_data);
		size = 0;
		return false;
	}

	return true;
}
