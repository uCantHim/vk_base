#pragma once

#include <memory>

#include <trc/Torch.h>
using namespace trc::basic_types;

#include "Scene.h"
#include "AssetManager.h"
#include "gui/MainMenu.h"
#include "input/InputState.h"

class App
{
public:
    App(int argc, char* argv[]);
    ~App();

    void run();
    void end();

    auto getTorch() -> trc::TorchStack&;
    auto getAssets() -> AssetManager&;
    auto getScene() -> Scene&;

    static auto get() -> App&;

private:
    static inline App* _app{ nullptr };

    void init();
    void tick();
    bool doEnd{ false };

    u_ptr<int, std::function<void(int*)>> trcTerminator;

    u_ptr<trc::TorchStack> torch;
    u_ptr<trc::imgui::ImguiRenderPass> imgui{ nullptr };
    AssetManager assetManager;
    Scene scene;

    gui::MainMenu mainMenu;
    InputStateMachine inputState;

    trc::Timer frameTimer;
};