#include <iostream>

#include <trc/Torch.h>
#include <trc/TorchResources.h>
#include <trc/ui/Window.h>
#include <trc/ui/torch/GuiIntegration.h>
using namespace trc::basic_types;
namespace ui = trc::ui;
#include <ui/elements/Quad.h>
#include <ui/elements/Text.h>

int main()
{
    {
        auto renderer = trc::init();
        auto scene = std::make_unique<trc::Scene>();
        auto camera = std::make_unique<trc::Camera>();
        const auto [width, height] = vkb::getSwapchain().getImageExtent();
        camera->lookAt(vec3(0, 2, 4), vec3(0, 0, 0), vec3(0, 1, 0));
        camera->makePerspective(float(width) / float(height), 45.0f, 0.1f, 100.0f);

        const ui32 nerdFont = ui::FontRegistry::addFont(
            "/usr/share/fonts/nerd-fonts-complete/TTF/Hack Regular Nerd Font Complete.ttf",
            18
        );

        ui::Window window{{
            std::make_unique<trc::TorchWindowBackend>(vkb::getSwapchain())
        }};

        // Notify GUI of mouse clicks
        vkb::on<vkb::MouseClickEvent>([&window](const vkb::MouseClickEvent& e) {
            if (e.action == vkb::InputAction::press)
            {
                vec2 pos = e.swapchain->getMousePosition();
                window.signalMouseClick(pos.x, pos.y);
            }
        });

        // Add gui pass and stage to render graph
        auto renderPass = trc::RenderPass::createAtNextIndex<trc::GuiRenderPass>(
            vkb::getDevice(),
            vkb::getSwapchain(),
            window
        ).first;
        auto& graph = renderer->getRenderGraph();
        graph.after(trc::RenderStageTypes::getDeferred(), trc::getGuiRenderStage());
        graph.addPass(trc::getGuiRenderStage(), renderPass);

        // Create some gui elements
        auto quad = window.create<ui::Quad>().makeUnique();
        window.getRoot().attach(*quad);
        quad->setPos({ 0.5f, 0.0f });
        quad->setSize({ 0.1f, 0.15f });

        quad->addEventListener([](const ui::event::Click& e) {
            std::cout << "Click on first quad\n";
        });

        auto child = window.create<ui::Quad>().makeUnique();
        quad->attach(*child);
        child->setPos({ 0.15f, 0.4f });
        child->addEventListener([](const ui::event::Click& e) {
            std::cout << "Click on second quad\n";
        });

        auto text = window.create<ui::Text>(
            "Hello World! and some more text…"
            "\n»this line« contains some cool special characters: “µ” · ħŋſđðſđ"
            "\nNewlines working: ∞",
            nerdFont
        ).makeUnique();
        window.getRoot().attach(*text);
        text->setPos({ 0.2f, 0.4f });
        text->addEventListener([](const ui::event::Click&) {
            std::cout << "Text clicked\n";
        });

        // Also add world-space objects
        trc::Light light = trc::makeSunLight(vec3(1.0f), vec3(0, -1, -1), 0.4f);
        scene->addLight(light);

        auto planeGeo = trc::AssetRegistry::addGeometry(trc::makePlaneGeo());
        auto planeMat = trc::AssetRegistry::addMaterial({ .color=vec4(0.3f, 0.7f, 0.2f, 1.0f) });
        trc::AssetRegistry::updateMaterials();
        trc::Drawable plane(planeGeo, planeMat);
        plane.attachToScene(*scene);
        plane.rotateX(glm::radians(30.0f));

        while (vkb::getSwapchain().isOpen())
        {
            vkb::pollEvents();
            renderer->drawFrame(*scene, *camera);
        }

        vkb::getDevice()->waitIdle();
    }

    trc::terminate();

    std::cout << "Done.\n";
    return 0;
}
