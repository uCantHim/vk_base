add_subdirectory(shader)

target_sources(
    torch
    PRIVATE
        FragmentShader.cpp
        MaterialProgram.cpp
        MaterialSpecialization.cpp
        TorchMaterialSettings.cpp
        VertexShader.cpp
)
