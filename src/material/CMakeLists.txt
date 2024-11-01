add_subdirectory(shader)

target_sources(
    torch
    PRIVATE
        FragmentShader.cpp
        MaterialProgram.cpp
        TorchMaterialSettings.cpp
        VertexShader.cpp
)
