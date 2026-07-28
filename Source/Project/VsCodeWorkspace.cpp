#include "VsCodeWorkspace.h"

#include <Windows.h>
#include <Shellapi.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace EGE
{
	namespace
	{
		constexpr char ExtensionRecommendations[] = R"({
    "recommendations": [
        "sashi0034.angel-lsp"
    ]
}
)";

		constexpr char WorkspaceSettings[] = R"({
    "angelScript.builtinStringType": "string",
    "angelScript.implicitMutualInclusion": true,
    "angelScript.files.angelScript": [
        "Assets/**/*.as"
    ],
    "angelScript.files.exclude": [
        "Library/**",
        "build/**"
    ],
    "angelScript.definedSymbols": [
        "EDITOR"
    ]
}
)";

		bool WriteFileIfMissing(
			const std::filesystem::path& path,
			const char* contents,
			std::string& error)
		{
			std::error_code filesystemError;
			const bool exists = std::filesystem::exists(path, filesystemError);
			if (filesystemError)
			{
				error = "Could not inspect '" + path.string() + "': " +
					filesystemError.message();
				return false;
			}
			if (exists)
			{
				if (!std::filesystem::is_regular_file(path, filesystemError))
				{
					error = "Expected a file at '" + path.string() + "'.";
					return false;
				}
				return true;
			}

			std::ofstream stream(path, std::ios::binary | std::ios::trunc);
			if (!stream)
			{
				error = "Could not create '" + path.string() + "'.";
				return false;
			}
			stream << contents;
			if (!stream)
			{
				error = "Could not write '" + path.string() + "'.";
				return false;
			}
			return true;
		}

		std::wstring QuoteArgument(const std::wstring& value)
		{
			return L"\"" + value + L"\"";
		}

		bool LaunchVsCode(
			const std::wstring& executable,
			const std::wstring& parameters,
			const std::filesystem::path& projectDirectory)
		{
			const HINSTANCE result = ShellExecuteW(
				nullptr,
				L"open",
				executable.c_str(),
				parameters.c_str(),
				projectDirectory.c_str(),
				SW_SHOWNORMAL);
			return reinterpret_cast<INT_PTR>(result) > 32;
		}

		std::filesystem::path FindInstalledVsCode()
		{
			constexpr std::array<const char*, 3> EnvironmentVariables = {
				"LOCALAPPDATA",
				"ProgramFiles",
				"ProgramFiles(x86)"
			};
			constexpr std::array<const wchar_t*, 3> RelativePaths = {
				L"Programs\\Microsoft VS Code\\Code.exe",
				L"Microsoft VS Code\\Code.exe",
				L"Microsoft VS Code\\Code.exe"
			};

			for (std::size_t index = 0; index < EnvironmentVariables.size(); ++index)
			{
				const char* root = std::getenv(EnvironmentVariables[index]);
				if (!root || root[0] == '\0')
					continue;

				const std::filesystem::path candidate =
					std::filesystem::path(root) / RelativePaths[index];
				std::error_code error;
				if (std::filesystem::is_regular_file(candidate, error))
					return candidate;
			}

			return {};
		}

		bool OpenVsCodeImpl(
			const std::filesystem::path& projectDirectory,
			const std::filesystem::path* filePath,
			int line,
			int column,
			std::string& error)
		{
			if (!EnsureVsCodeWorkspace(projectDirectory, error))
				return false;

			std::error_code filesystemError;
			const std::filesystem::path absoluteProjectDirectory =
				std::filesystem::absolute(projectDirectory, filesystemError);
			if (filesystemError)
			{
				error = "Could not resolve the project directory: " +
					filesystemError.message();
				return false;
			}

			std::wstring parameters =
				L"--reuse-window " +
				QuoteArgument(absoluteProjectDirectory.wstring());
			if (filePath)
			{
				const std::filesystem::path absoluteFilePath =
					std::filesystem::absolute(*filePath, filesystemError);
				if (filesystemError)
				{
					error = "Could not resolve the script file: " +
						filesystemError.message();
					return false;
				}
				parameters += L" --goto " +
					QuoteArgument(
						absoluteFilePath.wstring() + L":" +
						std::to_wstring((std::max)(1, line)) + L":" +
						std::to_wstring((std::max)(1, column)));
			}

			const std::filesystem::path installedVsCode = FindInstalledVsCode();
			if (!installedVsCode.empty() &&
				LaunchVsCode(
					installedVsCode.wstring(),
					parameters,
					absoluteProjectDirectory))
			{
				return true;
			}
			if (LaunchVsCode(L"code", parameters, absoluteProjectDirectory))
				return true;

			error =
			"Could not start Visual Studio Code. Install it or enable the 'code' command-line launcher.";
			return false;
		}
	}

	bool EnsureVsCodeWorkspace(
		const std::filesystem::path& projectDirectory,
		std::string& error)
	{
		error.clear();
		if (projectDirectory.empty())
		{
			error = "The project directory is empty.";
			return false;
		}

		const std::filesystem::path vscodeDirectory =
			projectDirectory / ".vscode";
		std::error_code filesystemError;
		std::filesystem::create_directories(vscodeDirectory, filesystemError);
		if (filesystemError)
		{
			error = "Could not create '" + vscodeDirectory.string() + "': " +
				filesystemError.message();
			return false;
		}

		return WriteFileIfMissing(
			vscodeDirectory / "extensions.json",
			ExtensionRecommendations,
			error) &&
			WriteFileIfMissing(
				vscodeDirectory / "settings.json",
				WorkspaceSettings,
				error);
	}

	bool OpenVsCode(
		const std::filesystem::path& projectDirectory,
		std::string& error)
	{
		return OpenVsCodeImpl(projectDirectory, nullptr, 1, 1, error);
	}

	bool OpenVsCode(
		const std::filesystem::path& projectDirectory,
		const std::filesystem::path& filePath,
		std::string& error)
	{
		return OpenVsCodeImpl(projectDirectory, &filePath, 1, 1, error);
	}

	bool OpenVsCode(
		const std::filesystem::path& projectDirectory,
		const std::filesystem::path& filePath,
		int line,
		int column,
		std::string& error)
	{
		return OpenVsCodeImpl(
			projectDirectory, &filePath, line, column, error);
	}
}
