#include "KeyConfig.h"

#include "App.h"
#include "command/CameraCommands.h"
#include "command/ObjectRotateCommand.h"
#include "command/ObjectScaleCommand.h"
#include "command/ObjectTranslateCommand.h"
#include "gui/ContextMenu.h"



void openContextMenu()
{
    App::get().getScene().openContextMenu();
}

void selectHoveredObject()
{
    App::get().getScene().selectHoveredObject();
}

auto makeKeyMap(App& app, const KeyConfig& conf) -> KeyMap
{
    KeyMap map;

    map.set(conf.closeApp,            makeInputCommand([&]{ app.end(); }));
    map.set(conf.openContext,         makeInputCommand(openContextMenu));
    map.set(conf.selectHoveredObject, makeInputCommand(selectHoveredObject));

    map.set(conf.cameraRotate, std::make_unique<CameraRotateCommand>(app));
    map.set(conf.cameraMove,   std::make_unique<CameraMoveCommand>(app));

    map.set(conf.translateObject, std::make_unique<ObjectTranslateCommand>(app));
    map.set(conf.scaleObject,     std::make_unique<ObjectScaleCommand>());
    map.set(conf.rotateObject,    std::make_unique<ObjectRotateCommand>());

    // General stuff that I don't know how to deal with
    trc::on<trc::MouseClickEvent>([](auto&) { gui::ContextMenu::close(); });

    trc::on<trc::ScrollEvent>([&app](const auto& e)
    {
        static i32 scrollLevel{ 0 };
        scrollLevel += static_cast<i32>(glm::sign(e.yOffset));

        trc::Camera& camera = app.getScene().getCamera();
        if (scrollLevel == 0) {
            camera.setScale(1.0f);
        }
        else if (scrollLevel < 0) {
            camera.setScale(1.0f / -(scrollLevel - 1));
        }
        else {
            camera.setScale(scrollLevel + 1);
        }
    });

    return map;
}
