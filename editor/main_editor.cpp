#include "EditorApp.h"
#include "../engine/core/Log.h"
#include "../engine/project/ProjectManager.h"
#include <filesystem>
#include <string>
#include <iostream>

int main(int argc, char** argv)
{
    Hargon::GraphicsBackend backend = Hargon::GraphicsBackend::Vulkan;
    bool enableValidation = true;
    bool autoTest = false;
    std::string initialProject;
    int autoCreateTemplate = -1;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--backend" && i + 1 < argc)
        {
            std::string b = argv[++i];
            if (b == "d3d11" || b == "dx11")
                backend = Hargon::GraphicsBackend::D3D11;
            else
                backend = Hargon::GraphicsBackend::Vulkan;
        }
        else if (arg == "--d3d11")
        {
            backend = Hargon::GraphicsBackend::D3D11;
        }
        else if (arg == "--no-validation")
        {
            enableValidation = false;
        }
        else if (arg == "--autotest")
        {
            autoTest = true;
        }
        else if (arg == "--project" && i + 1 < argc)
        {
            initialProject = argv[++i];
        }
        else if (arg == "--create-project" && i + 2 < argc)
        {
            initialProject = argv[++i];
            autoCreateTemplate = std::stoi(argv[++i]);
        }
        else if (!arg.empty() && arg[0] != '-' && initialProject.empty())
        {
            initialProject = arg;
        }
    }

    if (!initialProject.empty() && autoCreateTemplate < 0)
    {
        std::error_code ec;
        std::filesystem::path ip(initialProject);
        if (std::filesystem::exists(ip, ec))
            initialProject = std::filesystem::weakly_canonical(ip, ec).generic_string();
    }

    Hargon::ProjectManager::InitializeWorkingDirectory(argc > 0 ? argv[0] : nullptr);

    Hargon::EditorApp app;
    if (!app.Init(backend, enableValidation, autoTest, initialProject, autoCreateTemplate))
    {
        std::cerr << "Failed to initialize Hargon EditorApp!\n";
        return 1;
    }

    app.Run();
    app.Shutdown();

    if (autoTest && Hargon::Log::GetErrorCount() > 0)
    {
        std::cerr << "AutoTest detected " << Hargon::Log::GetErrorCount() << " errors!\n";
        return 1;
    }

    return 0;
}
