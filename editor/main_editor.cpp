#include "EditorApp.h"
#include "../engine/core/Log.h"
#include <string>
#include <iostream>

int main(int argc, char** argv)
{
    Hargon::GraphicsBackend backend = Hargon::GraphicsBackend::Vulkan;
    bool enableValidation = true;
    bool autoTest = false;

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
    }

    Hargon::EditorApp app;
    if (!app.Init(backend, enableValidation, autoTest))
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
