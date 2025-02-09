#include "TorchCppWriter.h"

#include <algorithm>
#include <ranges>
#include <sstream>
#include <tuple>

#include "Util.h"
#include "StringUtil.h"
#include "PipelineDataWriter.h"



template<typename T>
auto TorchCppWriter::makeGroupInfo(const VariantGroup<T>& group) -> VariantGroupRepr
{
    std::string flagTypeName = makeFlagsType(group);

    return {
        .combinedFlagType=std::move(flagTypeName),
        .storageName=group.baseName + "Storage",
    };
}

template<typename T>
auto TorchCppWriter::makeGroupFlagUsingDecl(const VariantGroup<T>& group) -> std::string
{
    std::stringstream ss;
    ss << "using " << makeFlagsType(group) << " = FlagCombination<";
    ++nl;
    for (size_t type : group.flagTypes) {
        ss << nl << makeFlagBitsType(flagTable->getFlagType(type)) << ",";
    }
    ss.seekp(-1, std::ios_base::end);  // Remove trailing ',' character
    ss << --nl << ">;";

    return ss.str();
}

template<typename T>
auto TorchCppWriter::makeFlagsType(const VariantGroup<T>& group) -> std::string
{
    return capitalize(group.baseName) + "TypeFlags";
}

template<typename T>
void TorchCppWriter::writeSingle(const std::string& name, const T& value, std::ostream& os)
{
    // Write storage variable
    os << makeStoredType<T>() << " " << name << " = " << makeValue(value) << ";";

    // Write getter function
    os << nl;
    writeGetterFunctionHead<T>(name, os);
    os << nl << "{"
       << ++nl << "return " << name << ";"
       << --nl << "}";
}

template<typename T>
void TorchCppWriter::writeGroup(const VariantGroup<T>& group, std::ostream& os)
{
    auto groupInfo = makeGroupInfo(group);

    // Write storage array
    os << "std::array<" << makeStoredType<T>() << ", " << groupInfo.combinedFlagType << "::size()> "
       << groupInfo.storageName;

    // Write data initialization
    os << "{";
    ++nl;

    // Sort variants by index
    std::vector<std::tuple<uint32_t, const UniqueName*, const T*>> variantsAtIndex;
    for (const auto& [name, variant] : group.variants) {
        variantsAtIndex.emplace_back(name.calcFlagIndex(*flagTable), &name, &variant);
    }
    std::ranges::sort(variantsAtIndex,
                      [](auto& a, auto& b){ return std::get<0>(a) < std::get<0>(b); });

    // Write the sorted list of variants in an inline-initializer-list constructor
    for (const auto& [_, name, variant] : variantsAtIndex) {
        os << nl << makeValue(*variant) << ",";
    }
    os << --nl << "};" << nl;

    // Write getter function
    writeGetterFunction(group, os);
}

template<typename T>
void TorchCppWriter::writeGetterFunctionHead(const std::string& name, std::ostream& os)
{
    os << "auto " << makeGetterFunctionName(name) << "() -> const " << makeStoredType<T>() << "&";
}

template<typename T>
void TorchCppWriter::writeGetterFunctionHead(const VariantGroup<T>& group, std::ostream& os)
{
    VariantGroupRepr groupInfo = makeGroupInfo(group);

    os << "auto " << makeGetterFunctionName(group.baseName) << "("
       << "const " << groupInfo.combinedFlagType << "& flags"
       << ") -> const " << makeStoredType<T>() << "&";
}

template<typename T>
void TorchCppWriter::writeGetterFunction(const VariantGroup<T>& group, std::ostream& os)
{
    VariantGroupRepr groupInfo = makeGroupInfo(group);

    writeGetterFunctionHead(group, os);
    os << nl << "{" << ++nl
       << "return " << groupInfo.storageName << "[flags.toIndex()];"
       << --nl << "}" << nl;
}

template<typename T>
auto TorchCppWriter::makeValue(const ObjectReference<T>& ref) -> std::string
{
    return std::visit(VariantVisitor{
        [&](const UniqueName& name) { return makeReferenceCall(name); },
        [&](const T& value) { return makeValue(value); },
    }, ref);
}



//////////////////////////
//  Shader type writer  //
//////////////////////////

template<typename T>
auto TorchCppWriter::makeStoredType() -> std::string
{
    if constexpr (std::same_as<T, ShaderDesc>) {
        return "trc::ShaderPath";
    }
    else if constexpr (std::same_as<T, ProgramDesc>) {
        return "trc::ProgramDefinitionData";
    }
    else if constexpr (std::same_as<T, LayoutDesc>) {
        return "trc::PipelineLayout::ID";
    }
    else if constexpr (std::same_as<T, PipelineDesc> || std::same_as<T, ComputePipelineDesc>) {
        return "trc::Pipeline::ID";
    }
}

template<>
inline auto TorchCppWriter::makeValue(const ShaderDesc& shader) -> std::string
{
    return makeStoredType<ShaderDesc>() + "(\"" + shader.target + "\")";
}



///////////////////////////
//  Program type writer  //
///////////////////////////

template<>
inline auto TorchCppWriter::makeValue(const ProgramDesc& program) -> std::string
{
    std::stringstream ss;
    auto writeStage = [this, &ss](const char* stage, const ObjectReference<ShaderDesc>& ref) {
        ss << nl << "{ vk::ShaderStageFlagBits::e" << stage << ", { shaderLoader.load(";
        std::visit(VariantVisitor{
            [&](const UniqueName& name) {
                ss << makeReferenceCall(name);
            },
            [&](const ShaderDesc& shader) {
                ss << makeValue(shader);
            },
        }, ref);
        ss << "), {} } },";
    };

    ss << "trc::ProgramDefinitionData{" << ++nl
       << ".stages={";
    ++nl;
    if (program.vert.has_value()) {
        writeStage("Vertex", program.vert.value());
    }
    if (program.tesc.has_value()) {
        writeStage("TessellationControl", program.tesc.value());
    }
    if (program.tese.has_value()) {
        writeStage("TessellationEvaluation", program.tese.value());
    }
    if (program.geom.has_value()) {
        writeStage("Geometry", program.geom.value());
    }
    if (program.frag.has_value()) {
        writeStage("Fragment", program.frag.value());
    }
    ss << --nl << "}" << --nl << "}";

    return ss.str();
}



///////////////////////////////////
//  Pipeline Layout type writer  //
///////////////////////////////////

template<>
inline auto TorchCppWriter::makeValue(const LayoutDesc& layout) -> std::string
{
    std::stringstream ss;
    ss << "trc::PipelineRegistry::registerPipelineLayout("
       << "trc::PipelineLayoutTemplate{";

    // Descriptors
    ss << (++nl)++ << "{" << std::boolalpha;
    for (const auto& desc : layout.descriptors) {
        ss << nl << "{ { \"" << desc.name << "\" }, " << desc.isStatic << " },";
    }
    ss << --nl << "},";

    // Push constants
    ss << nl++ << "{";
    for (const auto& [stage, pcs] : layout.pushConstantsPerStage)
    {
        auto stageBit = "vk::ShaderStageFlagBits::e" + capitalize(stage);
        for (const auto& pc : pcs)
        {
            ss << nl << "{ vk::PushConstantRange(" << stageBit << ", "
               << pc.offset << ", " << pc.size << "), ";
            if (pc.defaultValueName.has_value()) {
                ss << pc.defaultValueName.value() << "{}";
            }
            else {
                ss << "std::nullopt";
            }
            ss << " },";
        }
    }
    ss << --nl << "}";

    // End
    ss << --nl << "})";
    return ss.str();
}



////////////////////////////
//  Pipeline type writer  //
////////////////////////////

template<>
inline auto TorchCppWriter::makeValue(const PipelineDesc& pipeline) -> std::string
{
    auto& prog = pipeline.program;

    std::stringstream ss;
    ss << "trc::PipelineRegistry::registerPipeline("
       << ++nl << "trc::PipelineTemplate{"
       << ++nl << (prog ? makeValue(*prog) : "{}") << ","
       <<   nl << makePipelineDefinitionDataInit(pipeline, nl)
       << --nl << "},"
       <<   nl << makeValue(pipeline.layout) << ","
       <<   nl << "trc::RenderPassName{ \"" << pipeline.renderPassName.value_or("") << "\" }"
       << --nl << ")";

    return ss.str();
}



////////////////////////////
//  Pipeline type writer  //
////////////////////////////

template<>
inline auto TorchCppWriter::makeValue(const ComputePipelineDesc& pipeline) -> std::string
{
    std::stringstream ss;
    ss << "trc::PipelineRegistry::registerPipeline("
       << ++nl << "trc::ComputePipelineTemplate(" << makeValue(pipeline.shader) << "),"
       << nl << makeValue(pipeline.layout)
       << --nl << ")";
    return ss.str();
}
