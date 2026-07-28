#include "Globals.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleResources.h"
#include <physfs.h>
#include <assimp/cfileio.h>
#include <assimp/types.h>
#include <miniaudio.h>
#include <filesystem>

// miniaudio pulls in <windows.h> on Windows, which #defines CreateDirectory to
// CreateDirectoryA/W and breaks ModuleFileSystem::CreateDirectory()'s own definition below
#ifdef CreateDirectory
#undef CreateDirectory
#endif

#include "Leaks.h"

using namespace std;

struct PhysFSVFS
{
	ma_vfs_callbacks cb;
};

ModuleFileSystem::ModuleFileSystem(const char* game_path) : Module("File System", true)
{
	// needs to be created before Init so other modules can use it
	char* base_path = SDL_GetBasePath();
	PHYSFS_init(base_path);
	SDL_free(base_path);

	std::error_code path_error;
	engine_root = std::filesystem::absolute(
		std::filesystem::current_path(), path_error).lexically_normal();
	if (path_error)
		engine_root = std::filesystem::current_path().lexically_normal();
	project_root.clear();

	MountEngineDirectory("Settings", "Engine/Settings");
	MountEngineDirectory("Assets/Shaders", "Engine/Shaders");
	MountEngineDirectory("Assets/Textures", "Engine/Textures");
	MountEngineDirectory("Assets/LUTs", "Engine/LUTs");

	if(0&&game_path != nullptr)
		AddPath(game_path);

	// Dump list of paths
	LOG("FileSystem Operations base is [%s] plus:", GetBasePath());
	LOG(GetReadPaths());

	// Generate IO interfaces
	CreateAssimpIO();
	CreateAudioVFS();
}

bool ModuleFileSystem::CreateStandardDirectories()
{
	const char* dirs[] = {
		SETTINGS_FOLDER, ASSETS_FOLDER, LIBRARY_FOLDER,
		LIBRARY_AUDIO_FOLDER, LIBRARY_MESH_FOLDER,
		LIBRARY_MATERIAL_FOLDER, LIBRARY_SCENE_FOLDER, LIBRARY_MODEL_FOLDER, 
		LIBRARY_TEXTURES_FOLDER, LIBRARY_ANIMATION_FOLDER, LIBRARY_STATE_MACHINE_FOLDER,
	};

	for (uint i = 0; i < sizeof(dirs)/sizeof(const char*); ++i)
	{
		if (PHYSFS_exists(dirs[i]) == 0)
		{
			if (PHYSFS_mkdir(dirs[i]) == 0)
			{
				LOG("File System error while creating directory [%s]: %s",
					dirs[i], PHYSFS_getLastError());
				return false;
			}
		}
	}

	return true;
}

// Destructor
ModuleFileSystem::~ModuleFileSystem()
{
	RELEASE(AssimpIO);
	RELEASE(AudioVFS);
	PHYSFS_deinit();
}

// Called before render is available
bool ModuleFileSystem::Init(Config* config)
{
	LOG("Loading File System");
	bool ret = true;

	// Ask SDL for a write dir
	char* write_path = SDL_GetPrefPath(App->GetOrganizationName(), App->GetAppName());

	// Trun this on while in game mode
	//if(PHYSFS_setWriteDir(write_path) == 0)
		//LOG("File System error while creating write dir: %s\n", PHYSFS_getLastError());


	SDL_free(write_path);

	return ret;
}

// Called before quitting
bool ModuleFileSystem::CleanUp()
{
	//LOG("Freeing File System subsystem");

	return true;
}

// Add a new zip file or folder
bool ModuleFileSystem::AddPath(const char* path_or_zip)
{
	bool ret = false;

	if(PHYSFS_mount(path_or_zip, nullptr, 1) == 0)
		LOG("File System error while adding a path or zip: %s\n", PHYSFS_getLastError());
	else
		ret = true;

	return ret;
}

bool ModuleFileSystem::SetProjectRoot(
	const std::filesystem::path& requested_root)
{
	std::error_code error;
	std::filesystem::path normalized_root =
		std::filesystem::absolute(requested_root, error).lexically_normal();
	if (error || !std::filesystem::is_directory(normalized_root, error))
	{
		LOG("Cannot use project directory [%s]",
			requested_root.string().c_str());
		return false;
	}

	if (normalized_root == project_root)
		return true;

	const std::string new_path = normalized_root.string();
	if (PHYSFS_mount(new_path.c_str(), nullptr, 1) == 0)
	{
		LOG("Cannot mount project [%s]: %s",
			new_path.c_str(), PHYSFS_getLastError());
		return false;
	}

	if (PHYSFS_setWriteDir(new_path.c_str()) == 0)
	{
		PHYSFS_unmount(new_path.c_str());
		LOG("Cannot set project write directory [%s]: %s",
			new_path.c_str(), PHYSFS_getLastError());
		return false;
	}

	if (!mounted_project_path.empty() &&
		PHYSFS_unmount(mounted_project_path.c_str()) == 0)
	{
		PHYSFS_setWriteDir(project_root.string().c_str());
		PHYSFS_unmount(new_path.c_str());
		LOG("Cannot unmount previous project [%s]: %s",
			mounted_project_path.c_str(), PHYSFS_getLastError());
		return false;
	}

	project_root = std::move(normalized_root);
	mounted_project_path = new_path;
	return CreateStandardDirectories();
}

bool ModuleFileSystem::ClearProjectRoot()
{
	if (mounted_project_path.empty())
	{
		project_root.clear();
		return true;
	}

	if (PHYSFS_unmount(mounted_project_path.c_str()) == 0)
	{
		LOG("Cannot unmount project [%s]: %s",
			mounted_project_path.c_str(), PHYSFS_getLastError());
		return false;
	}

	PHYSFS_setWriteDir(nullptr);
	mounted_project_path.clear();
	project_root.clear();
	return true;
}

const std::filesystem::path& ModuleFileSystem::GetProjectRoot() const
{
	return project_root;
}

const std::filesystem::path& ModuleFileSystem::GetEngineRoot() const
{
	return engine_root;
}

bool ModuleFileSystem::MountEngineDirectory(
	const std::filesystem::path& source,
	const char* virtualPath)
{
	const std::filesystem::path absoluteSource = engine_root / source;
	std::error_code error;
	if (!std::filesystem::is_directory(absoluteSource, error))
	{
		LOG("Engine directory is unavailable [%s]",
			absoluteSource.string().c_str());
		return false;
	}

	if (PHYSFS_mount(
			absoluteSource.string().c_str(), virtualPath, 1) == 0)
	{
		LOG("Cannot mount engine directory [%s] at [%s]: %s",
			absoluteSource.string().c_str(),
			virtualPath,
			PHYSFS_getLastError());
		return false;
	}
	return true;
}

// Check if a file exists
bool ModuleFileSystem::Exists(const char* file) const
{
	return PHYSFS_exists(file) != 0;
}

// Check if a file is a directory
bool ModuleFileSystem::IsDirectory(const char* file) const
{
	return PHYSFS_isDirectory(file) != 0;
}

void ModuleFileSystem::CreateDirectory(const char* directory)
{
    PHYSFS_mkdir(directory);
}

void ModuleFileSystem::DiscoverFiles(const char* directory, vector<string> & file_list, vector<string> & dir_list) const
{
	char **rc = PHYSFS_enumerateFiles(directory);
	char **i;

	string dir(directory);

	for (i = rc; *i != nullptr; i++)
	{
		if(PHYSFS_isDirectory((dir+*i).c_str()))
			dir_list.push_back(*i);
		else
			file_list.push_back(*i);
	}

	PHYSFS_freeList(rc);
}

bool ModuleFileSystem::CopyFromOutsideFS(const char * full_path, const char * destination)
{
	// Only place we acces non virtual filesystem
 	bool ret = false;

    char buf[8192];
    size_t size;

	FILE* source = nullptr;
	fopen_s(&source,full_path, "rb");
	PHYSFS_file* dest = PHYSFS_openWrite(destination);

	if (source && dest)
	{
		while (size = fread_s(buf, 8192, 1, 8192, source))
			PHYSFS_write(dest, buf, 1, int(size));

		fclose(source);
		PHYSFS_close(dest);
		ret = true;

		LOG("File System copied file [%s] to [%s]", full_path, destination);
	}
	else
		LOG("File System error while copy from [%s] to [%s]", full_path, destination);

	return ret;
}

bool ModuleFileSystem::Copy(const char * source, const char * destination)
{
 	bool ret = false;

    char buf[8192];

	PHYSFS_file* src = PHYSFS_openRead(source);
	PHYSFS_file* dst = PHYSFS_openWrite(destination);

	PHYSFS_sint32 size;
	if (src && dst)
	{
		while (size = (PHYSFS_sint32) PHYSFS_read(src, buf, 1, 8192))
			PHYSFS_write(dst, buf, 1, size);

		PHYSFS_close(src);
		PHYSFS_close(dst);
		ret = true;

		LOG("File System copied file [%s] to [%s]", source, destination);
	}
	else
		LOG("File System error while copy from [%s] to [%s]", source, destination);

	return ret;
}

void ModuleFileSystem::SplitFilePath(const char * full_path, std::string * path, std::string * file, std::string * extension) const
{
	if (full_path != nullptr)
	{
		string full(full_path);
		NormalizePath(full);
		size_t pos_separator = full.find_last_of("\\/");
		size_t pos_dot = full.find_last_of(".");

		if (path != nullptr)
		{
			if (pos_separator < full.length())
				*path = full.substr(0, pos_separator + 1);
			else
				path->clear();
		}

		if (file != nullptr)
		{
			if (pos_separator < full.length())
				*file = full.substr(pos_separator + 1);
			else
				*file = full;
		}

		if (extension != nullptr)
		{
			if (pos_dot < full.length())
				*extension = full.substr(pos_dot + 1);
			else
				extension->clear();
		}
	}
}

// Flatten filenames to always use lowercase and / as folder separator
char normalize_char(char c)
{
	if (c == '\\')
		return '/';
	return tolower(c);
}

void ModuleFileSystem::NormalizePath(char * full_path) const
{
	int len = int(strlen(full_path));
	for (int i = 0; i < len; ++i)
	{
		if (full_path[i] == '\\')
			full_path[i] = '/';
		else
			full_path[i] = tolower(full_path[i]);
	}
}

void ModuleFileSystem::NormalizePath(std::string & full_path) const
{
	for (string::iterator it = full_path.begin(); it != full_path.end(); ++it)
	{
		if (*it == '\\')
			*it = '/';
		else
			*it = tolower(*it);
	}
}

unsigned int ModuleFileSystem::Load(const char * path, const char * file, char ** buffer) const
{
	string full_path(path);
	full_path += file;
	return Load(full_path.c_str(), buffer);
}

// Read a whole file and put it in a new buffer
uint ModuleFileSystem::Load(const char* file, char** buffer) const
{
	uint ret = 0;

    const char* sep = PHYSFS_getDirSeparator();
	PHYSFS_file* fs_file = PHYSFS_openRead(file);

	if(fs_file != nullptr)
	{
		PHYSFS_sint32 size = (PHYSFS_sint32) PHYSFS_fileLength(fs_file);

		if(size > 0)
		{
			*buffer = new char[size + 1];
			uint readed = (uint) PHYSFS_read(fs_file, *buffer, 1, size);
			if(readed != size)
			{
				LOG("File System error while reading from file %s: %s\n", file, PHYSFS_getLastError());
				RELEASE(buffer);
			}
			else
			{
				(*buffer)[size] = '\0';
				ret = readed;
			}
		}

		if(PHYSFS_close(fs_file) == 0)
			LOG("File System error while closing file %s: %s\n", file, PHYSFS_getLastError());
	}
	else
		LOG("File System error while opening file %s: %s\n", file, PHYSFS_getLastError());

	return ret;
}

// Read a whole file and put it in a new buffer
SDL_RWops* ModuleFileSystem::Load(const char* file) const
{
	char* buffer;
	int size = Load(file, &buffer);

	if(size > 0)
	{
		SDL_RWops* r = SDL_RWFromConstMem(buffer, size);
		if(r != nullptr)
			r->close = close_sdl_rwops;

		return r;
	}
	else
		return nullptr;
}

int close_sdl_rwops(SDL_RWops *rw)
{
	RELEASE_ARRAY(rw->hidden.mem.base);
	SDL_FreeRW(rw);
	return 0;
}

unsigned int ModuleFileSystem::Save(const char* path, const char* file, const void* buffer, unsigned int size, bool append) const
{
    string full_path(path);
    full_path += file;
    return Save(full_path.c_str(), buffer, size, append);
}

// Save a whole buffer to disk
uint ModuleFileSystem::Save(const char* file, const void* buffer, unsigned int size, bool append) const
{
	unsigned int ret = 0;

	bool overwrite = PHYSFS_exists(file) != 0;
	PHYSFS_file* fs_file = (append) ? PHYSFS_openAppend(file) : PHYSFS_openWrite(file);

	if(fs_file != nullptr)
	{
		uint written = (uint) PHYSFS_write(fs_file, (const void*)buffer, 1, size);
		if(written != size)
			LOG("File System error while writing to file %s: %s", file, PHYSFS_getLastError());
		else
		{
			if(append == true)
				LOG("Added %u data to [%s%s]", size, PHYSFS_getWriteDir(), file);
			//else if(overwrite == true)
				//LOG("File [%s%s] overwritten with %u bytes", PHYSFS_getWriteDir(), file, size);
			else if(overwrite == false)
				LOG("New file created [%s%s] of %u bytes", PHYSFS_getWriteDir(), file, size);

			ret = written;
		}

		if(PHYSFS_close(fs_file) == 0)
			LOG("File System error while closing file %s: %s", file, PHYSFS_getLastError());
	}
	else
		LOG("File System error while opening file %s: %s", file, PHYSFS_getLastError());

	return ret;
}

bool ModuleFileSystem::SaveUnique(string& name, const void * buffer, uint size, const char * path, const char * prefix, const char * extension)
{
	char result[250];

	sprintf_s(result, 250, "%s%s_%llu.%s", path, prefix, App->resources->GenerateNewUID(), extension);
	NormalizePath(result);
	if (Save(result, buffer, size) > 0)
	{
		name = result;
		return true;
	}
	return false;
}

bool ModuleFileSystem::Remove(const char * file)
{
	bool ret = false;

	if (file != nullptr)
	{
		if (PHYSFS_delete(file) == 0)
		{
			LOG("File deleted: [%s]", file);
			ret = true;
		}
		else
			LOG("File System error while trying to delete [%s]: ", file, PHYSFS_getLastError());
	}

	return ret;
}

const char * ModuleFileSystem::GetBasePath() const
{
	return PHYSFS_getBaseDir();
}

const char * ModuleFileSystem::GetWritePath() const
{
	return PHYSFS_getWriteDir();
}

const char * ModuleFileSystem::GetReadPaths() const
{
	static char paths[512];

	paths[0] = '\0';

	char **path;
	for (path = PHYSFS_getSearchPath(); *path != nullptr; path++)
	{
		strcat_s(paths, 512, *path);
		strcat_s(paths, 512, "\n");
	}

	return paths;
}

// -----------------------------------------------------
// ASSIMP IO
// -----------------------------------------------------

size_t AssimpWrite(aiFile* file, const char* data, size_t size, size_t chunks)
{
	PHYSFS_sint64 ret = PHYSFS_write((PHYSFS_File*)file->UserData, (void*)data, PHYSFS_uint32(size), PHYSFS_uint32(chunks));
	if(ret == -1)
		LOG("File System error while WRITE via assimp: %s", PHYSFS_getLastError());

	return (size_t) ret;
}

size_t AssimpRead(aiFile* file, char* data, size_t size, size_t chunks)
{
	PHYSFS_sint64 ret = PHYSFS_read((PHYSFS_File*)file->UserData, (void*)data, PHYSFS_uint32(size), PHYSFS_uint32(chunks));
	if(ret == -1)
		LOG("File System error while READ via assimp: %s", PHYSFS_getLastError());

	return (size_t) ret;
}

size_t AssimpTell(aiFile* file)
{
	PHYSFS_sint64 ret = PHYSFS_tell((PHYSFS_File*)file->UserData);
	if(ret == -1)
		LOG("File System error while TELL via assimp: %s", PHYSFS_getLastError());

	return (size_t) ret;
}

size_t AssimpSize(aiFile* file)
{
	PHYSFS_sint64 ret = PHYSFS_fileLength((PHYSFS_File*)file->UserData);
	if(ret == -1)
		LOG("File System error while SIZE via assimp: %s", PHYSFS_getLastError());

	return (size_t) ret;
}

void AssimpFlush(aiFile* file)
{
	if(PHYSFS_flush((PHYSFS_File*)file->UserData) == 0)
		LOG("File System error while FLUSH via assimp: %s", PHYSFS_getLastError());
}

aiReturn AssimpSeek(aiFile* file, size_t pos, aiOrigin from)
{
	int res = 0;

	switch (from)
	{
	case aiOrigin_SET:
		res = PHYSFS_seek((PHYSFS_File*)file->UserData, pos);
		break;
	case aiOrigin_CUR:
		res = PHYSFS_seek((PHYSFS_File*)file->UserData, PHYSFS_tell((PHYSFS_File*)file->UserData) + pos);
		break;
	case aiOrigin_END:
		res = PHYSFS_seek((PHYSFS_File*)file->UserData, PHYSFS_fileLength((PHYSFS_File*)file->UserData) + pos);
		break;
	}

	if(res == 0)
		LOG("File System error while SEEK via assimp: %s", PHYSFS_getLastError());

	return (res != 0) ? aiReturn_SUCCESS : aiReturn_FAILURE;
}

aiFile* AssimpOpen(aiFileIO* io, const char* name, const char* format)
{
	static aiFile file;

	file.UserData = (char*) PHYSFS_openRead(name);
	file.ReadProc = AssimpRead;
	file.WriteProc = AssimpWrite;
	file.TellProc = AssimpTell;
	file.FileSizeProc = AssimpSize;
	file.FlushProc= AssimpFlush;
	file.SeekProc = AssimpSeek;

	return &file;
}

void AssimpClose(aiFileIO* io, aiFile* file)
{
	if (PHYSFS_close((PHYSFS_File*)file->UserData) == 0)
		LOG("File System error while CLOSE via assimp: %s", PHYSFS_getLastError());
}

void ModuleFileSystem::CreateAssimpIO()
{
	RELEASE(AssimpIO);

	AssimpIO = new aiFileIO;
	AssimpIO->OpenProc = AssimpOpen;
	AssimpIO->CloseProc = AssimpClose;
}

aiFileIO * ModuleFileSystem::GetAssimpIO()
{
	return AssimpIO;
}

// -----------------------------------------------------
// Audio VFS (miniaudio file access via PhysFS)
// -----------------------------------------------------
static ma_result AudioVFS_Open(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
	(void) pVFS;

	if ((openMode & MA_OPEN_MODE_WRITE) != 0)
		return MA_NOT_IMPLEMENTED; // read-only VFS

	PHYSFS_File* file = PHYSFS_openRead(pFilePath);
	if (file == nullptr)
	{
		LOG("File System error while OPEN via miniaudio: %s", PHYSFS_getLastError());
		return MA_DOES_NOT_EXIST;
	}

	*pFile = (ma_vfs_file) file;
	return MA_SUCCESS;
}

static ma_result AudioVFS_Close(ma_vfs* pVFS, ma_vfs_file file)
{
	(void) pVFS;

	if (PHYSFS_close((PHYSFS_File*) file) == 0)
	{
		LOG("File System error while CLOSE via miniaudio: %s", PHYSFS_getLastError());
		return MA_ERROR;
	}

	return MA_SUCCESS;
}

static ma_result AudioVFS_Read(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
{
	(void) pVFS;

	PHYSFS_sint64 ret = PHYSFS_read((PHYSFS_File*) file, pDst, 1, (PHYSFS_uint32) sizeInBytes);
	if (ret < 0)
	{
		LOG("File System error while READ via miniaudio: %s", PHYSFS_getLastError());
		if (pBytesRead != nullptr)
			*pBytesRead = 0;
		return MA_ERROR;
	}

	if (pBytesRead != nullptr)
		*pBytesRead = (size_t) ret;

	return (ret < (PHYSFS_sint64) sizeInBytes) ? MA_AT_END : MA_SUCCESS;
}

static ma_result AudioVFS_Write(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten)
{
	(void) pVFS; (void) file; (void) pSrc; (void) sizeInBytes;

	if (pBytesWritten != nullptr)
		*pBytesWritten = 0;

	return MA_NOT_IMPLEMENTED; // read-only VFS
}

static ma_result AudioVFS_Seek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
	(void) pVFS;

	PHYSFS_sint64 base = 0;
	if (origin == ma_seek_origin_current)
		base = PHYSFS_tell((PHYSFS_File*) file);
	else if (origin == ma_seek_origin_end)
		base = PHYSFS_fileLength((PHYSFS_File*) file);

	if (PHYSFS_seek((PHYSFS_File*) file, (PHYSFS_uint64) (base + offset)) == 0)
	{
		LOG("File System error while SEEK via miniaudio: %s", PHYSFS_getLastError());
		return MA_ERROR;
	}

	return MA_SUCCESS;
}

static ma_result AudioVFS_Tell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor)
{
	(void) pVFS;

	PHYSFS_sint64 pos = PHYSFS_tell((PHYSFS_File*) file);
	if (pos < 0)
	{
		LOG("File System error while TELL via miniaudio: %s", PHYSFS_getLastError());
		return MA_ERROR;
	}

	*pCursor = (ma_int64) pos;
	return MA_SUCCESS;
}

static ma_result AudioVFS_Info(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo)
{
	(void) pVFS;

	PHYSFS_sint64 len = PHYSFS_fileLength((PHYSFS_File*) file);
	if (len < 0)
	{
		LOG("File System error while INFO via miniaudio: %s", PHYSFS_getLastError());
		return MA_ERROR;
	}

	pInfo->sizeInBytes = (ma_uint64) len;
	return MA_SUCCESS;
}

void ModuleFileSystem::CreateAudioVFS()
{
	if (AudioVFS == nullptr)
		AudioVFS = new PhysFSVFS;

	AudioVFS->cb.onOpen = AudioVFS_Open;
	AudioVFS->cb.onOpenW = nullptr;
	AudioVFS->cb.onClose = AudioVFS_Close;
	AudioVFS->cb.onRead = AudioVFS_Read;
	AudioVFS->cb.onWrite = AudioVFS_Write;
	AudioVFS->cb.onSeek = AudioVFS_Seek;
	AudioVFS->cb.onTell = AudioVFS_Tell;
	AudioVFS->cb.onInfo = AudioVFS_Info;
}

void* ModuleFileSystem::GetAudioVFS()
{
	return AudioVFS;
}
