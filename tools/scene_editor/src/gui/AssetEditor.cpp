#include "AssetEditor.h"

#include "App.h"



gui::AssetEditor::AssetEditor(AssetManager& assetManager)
    :
    assets(&assetManager)
{
}

void gui::AssetEditor::drawImGui()
{
    imGuiTryBegin("Asset Editor");
    trc::imgui::WindowGuard guard;

    drawMaterialGui();
}

void gui::AssetEditor::drawMaterialGui()
{
    ig::PushItemWidth(200);
    util::textInputWithButton("Add material", matNameBuf.data(), matNameBuf.size(), [this]()
    {
        const std::string matName(matNameBuf.data());
        if (matName.empty()) return;

        try {
            trc::MaterialID matId = assets->add(trc::Material{});
            materials.emplace_back(matNameBuf.data(), matId);
            editedMaterial = matId;
            editedMaterialCopy = {}; // New material
        }
        catch (const trc::DuplicateKeyError& err) {
            // Nothing
        }
    });
    ig::PopItemWidth();

    // Show a material editor for every material that's being edited
    if (ig::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawMaterialList();
    }
    // Draw material editor
    if (editedMaterial != trc::MaterialID::NONE)
    {
        materialEditor("Material Editor", editedMaterialCopy,
            [this]() {
                // Material has been saved
                assets->get(editedMaterial) = editedMaterialCopy;
                assets->updateMaterials();
                editedMaterial = {};
            },
            [this]() {
                // Don't save; edit has been canceled
                editedMaterial = {};
            }
        );
    }
}

void gui::AssetEditor::drawMaterialList()
{
    for (auto& mat : materials)
    {
        ig::PushID(mat.matId);
        ig::Text("Material \"%s\"", mat.name.c_str());
        ig::SameLine();
        if (ig::Button("Edit")) {
            editedMaterial = mat.matId;
            editedMaterialCopy = assets->get(mat.matId);
        }
        ig::PopID();
    }
}