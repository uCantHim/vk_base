#include "PipelineDefinitionLanguage.h"

#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <utility>  // for std::as_const used in argparse
#include <vector>

#include <argparse/argparse.hpp>

#ifdef HAS_SPIRV_COMPILER
#include <spirv/CompileSpirv.h>
#include <spirv/FileIncluder.h>
#endif

#include "CMakeDepfileWriter.h"
#include "Compiler.h"
#include "Exceptions.h"
#include "Importer.h"
#include "Parser.h"
#include "Scanner.h"
#include "ShaderDatabase.h"
#include "TorchCppWriter.h"
#include "TypeChecker.h"
#include "TypeParser.h"
#include "shader_tools/ShaderDocument.h"



auto loadStdlib(ErrorReporter& errorReporter) -> std::vector<Stmt>
{
    std::vector<Stmt> statements;

    for (auto& entry : fs::directory_iterator(STDLIB_DIR))
    {
        if (!entry.is_regular_file()) continue;

        std::ifstream file(entry.path());
        if (!file.is_open()) {
            throw std::runtime_error("Unable to open standard library file " + entry.path().string());
        }
        std::stringstream ss;
        ss << file.rdbuf();

        // Scan
        Scanner scanner(ss.str(), errorReporter);
        auto tokens = scanner.scanTokens();

        // Parse
        Parser parser(std::move(tokens), errorReporter);
        auto parseResult = parser.parseTokens();

        statements.insert(statements.begin(), parseResult.begin(), parseResult.end());
    }

    return statements;
}



constexpr int USAGE{ 64 };

#ifdef HAS_SPIRV_COMPILER
shaderc::CompileOptions PipelineDefinitionLanguage::spirvOpts{};
#endif

void PipelineDefinitionLanguage::run(int argc, char** argv)
{
    argparse::ArgumentParser program("compiler", "0.1");
    program.add_argument("file")
        .help("Input file");
    program.add_argument("-o", "--output")
        .help("Output directory. Generated files are stored here. Use the --shader-output option "
              "to declare a separate output directory for generated shader files.");
    program.add_argument("-I", "--include")
        .action([](const std::string& arg) { includeDirs.emplace_back(arg); })
        .help("Specify additional include directories.");
    program.add_argument("--shader-input")
        .help("Input directory. Shader file paths are interpreted relative to this path.");
    program.add_argument("--shader-output")
        .action([](const std::string& arg){
            fs::create_directories(arg);
            shaderOutputDir = arg;
        })
        .help("Output directory for generated shader files.");
    program.add_argument("--depfile")
        .help("Generate a dependency file for the output files.");
    program.add_argument("--shader-db")
        .help("Generate a database of shader permutations that are generated during compilation."
              " This information can be used to replay generation without invoking the entire"
              " pipeline compiler.");
    program.add_argument("--shader-db-append")
        .default_value(false)
        .implicit_value(true)
        .help("If present, if the --shader-db option is specified, append the generated shader"
              " database to the file instead of overwriting it. This allows information from"
              " multiple compilations to be merged together into a single database file.");

#ifdef HAS_SPIRV_COMPILER
    program.add_argument("--spv")
        .action([](auto&&){ defaultShaderOutputType = ShaderOutputType::eSpirv; })
        .help("Compile generated shader files to SPIRV.")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--spv-version")
        .default_value(std::string("1.5"))
        .required()
        .action([](const std::string& arg)
        {
            std::unordered_map<std::string, shaderc_spirv_version> allowedVals{
                { "1.0", shaderc_spirv_version_1_0 },
                { "1.1", shaderc_spirv_version_1_1 },
                { "1.2", shaderc_spirv_version_1_2 },
                { "1.3", shaderc_spirv_version_1_3 },
                { "1.4", shaderc_spirv_version_1_4 },
                { "1.5", shaderc_spirv_version_1_5 },
                { "1.6", shaderc_spirv_version_1_6 },
            };
            if (!allowedVals.contains(arg)) {
                throw std::runtime_error(
                    "Value " + arg + " is not allowed for argument '--spv-version'."
                );
            }
            spirvVersion = allowedVals.at(arg);

            return arg;
        })
        .help("SPIRV version. Must be one of 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6.");
    program.add_argument("--spv-target-env")
        .default_value("vulkan1.2")
        .required()
        .action([](const std::string& arg)
        {
            std::unordered_map<std::string, std::pair<shaderc_target_env, shaderc_env_version>> allowedVals{
                { "vulkan1.0",     { shaderc_target_env_vulkan,        shaderc_env_version_vulkan_1_0 } },
                { "vulkan1.1",     { shaderc_target_env_vulkan,        shaderc_env_version_vulkan_1_1 } },
                { "vulkan1.2",     { shaderc_target_env_vulkan,        shaderc_env_version_vulkan_1_2 } },
                { "vulkan1.3",     { shaderc_target_env_vulkan,        shaderc_env_version_vulkan_1_3 } },
                { "opengl",        { shaderc_target_env_opengl,        shaderc_env_version_opengl_4_5 } },
                { "opengl-compat", { shaderc_target_env_opengl_compat, shaderc_env_version_opengl_4_5 } },
            };
            if (!allowedVals.contains(arg)) {
                throw std::runtime_error(
                    "Value " + arg + " is not allowed for argument '--spv-version'."
                );
            }
            auto [env, ver] = allowedVals.at(arg);
            targetEnv = env;
            targetEnvVersion = ver;

            return arg;
        })
        .help("Target semantics for SPIRV compilation. "
              "Must be one of vulkan1.0, vulkan1.1, vulkan1.2, vulkan1.3, opengl, opengl-compat."
        );
    program.add_argument("--shader-macro")
        .action([](const std::string& str){ shaderCompileDefinitions.emplace_back(str); })
        .help("Additional macro definition passed to the SPIRV compiler.");
#else
    program.add_description(
        "Compile with the CMake variable $PIPELINE_COMPILER_ENABLE_SPIRV_FEATURES set to TRUE to "
        "enable the option to compile generated shader files to SPIRV."
    );
#endif

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << "\n" << program;
        exit(USAGE);
    }

    const fs::path filename = program.get<std::string>("file");
    if (!fs::is_regular_file(filename))
    {
        std::cerr << filename << " is not a regular file. Exiting.";
        exit(USAGE);
    }

    if (auto val = program.present("-o"))
    {
        outputDir = *val;
        if (!program.is_used("--shader-output")) {
            shaderOutputDir = outputDir;
        }
        outputFileName = filename.filename();
        fs::create_directories(outputDir);
    }

    if (auto val = program.present("--shader-input"))
    {
        shaderInputDir = *val;
        if (!fs::is_directory(shaderInputDir))
        {
            std::cerr << shaderInputDir << " is not a directory. Exiting.";
            exit(USAGE);
        }
    }

    depfilePath = program.present("--depfile");
    shaderDatabasePath = program.present("--shader-db");
    appendToShaderDatabase = program.is_used("--shader-db-append");

    // Init
    errorReporter = std::make_unique<DefaultErrorReporter>(std::cout);
    try {
        const auto result = compile(filename);
        if (!result) exit(1);

        writeOutput(filename, result.value());
    }
    catch (const UsageError& err) {
        std::cout << "Usage Error: " << err.message << "\nExiting.\n";
        exit(USAGE);
    }
    catch (const IOError& err) {
        std::cout << "\nI/O Error: " << err.message << "\n";
        exit(1);
    }
    catch (const InternalLogicError& err) {
        std::cout << "\n[INTERNAL COMPILER ERROR]: " << err.message << "\n";
        exit(1);
    }
    catch (const std::runtime_error& err) {
        std::cout << "An unexpected error occured: " << err.what() << "\n" << "Exiting.";
        exit(1);
    }
}

auto PipelineDefinitionLanguage::compile(const fs::path& filename) -> std::optional<CompileResult>
{
    std::ifstream file(filename);
    std::stringstream ss;
    ss << file.rdbuf();

    // Scan
    Scanner scanner(ss.str(), *errorReporter);
    auto tokens = scanner.scanTokens();
    if (errorReporter->hadError()) return std::nullopt;

    // Parse
    Parser parser(std::move(tokens), *errorReporter);
    auto parseResult = parser.parseTokens();

    // Resolve import statements
    auto includePaths = includeDirs;
    includePaths.emplace_back(filename.parent_path());
    auto imports = Importer{ includePaths, *errorReporter }.parseImports(parseResult);
    std::move(imports.begin(), imports.end(), std::back_inserter(parseResult));

    // Load standard library
    auto stdlib = loadStdlib(*errorReporter);
    if (errorReporter->hadError()) return std::nullopt;
    std::move(stdlib.begin(), stdlib.end(), std::back_inserter(parseResult));

    // Load additional types defined in the input file
    auto typeConfig = makeDefaultTypeConfig();
    TypeParser typeParser(typeConfig, *errorReporter);
    typeParser.parse(parseResult);

    // Check types
    TypeChecker typeChecker(std::move(typeConfig), *errorReporter);
    typeChecker.check(parseResult);

    // Don't try to compile if errors have occured thus far
    if (errorReporter->hadError()) return std::nullopt;

    // Compile
    Compiler compiler(std::move(parseResult), *errorReporter);
    CompileResult compileResult;
    try {
        compileResult = compiler.compile();
    } catch (const CompilerError&) {}

    // Certainly don't output anything if errors have occured
    if (errorReporter->hadError()) return std::nullopt;
    return compileResult;
}

void PipelineDefinitionLanguage::writeOutput(
    const fs::path& sourceFilePath,
    const CompileResult& result)
{
    // Generate shaders first
    const auto db = makeShaderDatabase(defaultShaderOutputType, result);
    for (const auto& shader : db)  // Compile all shaders once by default
    {
        std::ifstream inFile(shaderInputDir / shader.at("source").get<std::string>());
        shader_edit::ShaderDocument doc(inFile);
        for (const auto& [key, val] : shader.at("variables").items()) {
            doc.set(key, val.get<std::string>());
        }

        writeShader(ShaderInfo{
            .glslCode=doc.compile(),
            .outputFileName=shaderOutputDir / shader.at("target").get<std::string>(),
            .outputType=shader.at("outputType").get<ShaderOutputType>(),
        });
    }
    if (shaderDatabasePath) {
        writeShaderDatabase(*shaderDatabasePath, db, appendToShaderDatabase);
    }

    // Generate pipeline files
    TorchCppWriter writer(*errorReporter, TorchCppWriterCreateInfo{
        .compiledFileName=sourceFilePath.filename().replace_extension("").string(),
        .shaderInputDir=shaderInputDir,
        .shaderOutputDir=shaderOutputDir,
        .shaderDatabasePath=shaderDatabasePath,
        .defaultShaderOutput=defaultShaderOutputType
    });

    fs::path outFilePath = outputDir / outputFileName;
    if (generateHeader)
    {
        const fs::path headerName = outFilePath.replace_extension(".h");
        std::ofstream header(outputDir / headerName);
        std::ofstream source(outFilePath.replace_extension(".cpp"));

        source << "#include " << headerName << "\n\n";
        header << "#pragma once\n\n";
        writer.write(result, header, source);
    }
    else {
        std::ofstream file(outFilePath.replace_extension(".h"));
        writer.write(result, file);
    }

    // Write dependency file
    if (depfilePath)
    {
        std::ofstream depfile(*depfilePath);
        CMakeDepfileWriter depfileWriter{ shaderInputDir, outFilePath.replace_extension(".cpp") };
        depfileWriter.write(result, depfile);
    }

    // Copy helper files to the output directory
    copyHelperFiles();

#ifdef HAS_SPIRV_COMPILER
    compileSpirvShaders();
#endif
}

void PipelineDefinitionLanguage::copyHelperFiles()
{
    // the fs::copy_options::overwrite_existing does not seem to work on windows
    const auto target = outputDir / "FlagCombination.h";
    if (!fs::is_regular_file(target)) {
        fs::copy(FLAG_COMBINATION_HEADER, target, fs::copy_options::overwrite_existing);
    }
}

void PipelineDefinitionLanguage::writeShader(const ShaderInfo& shader)
{
    assert(!shader.outputFileName.string().ends_with(".spv"));

    // Always write the GLSL source code to a file, even when the shader
    // will be compiled to SPIRV later.
    writePlain(shader.glslCode, shader.outputFileName);

    if (shader.outputType == ShaderOutputType::eSpirv)
    {
#ifdef HAS_SPIRV_COMPILER
        pendingSpirvCompilations.push_back({ shader.glslCode, shader.outputFileName });
#else
        throw UsageError("Unable to compile " + shaderFileName.string() + " to SPIRV.\n"
                         "The pipeline compiler must be compiled with the SPIRV capability enabled"
                         " to output shader files as SPIRV.");
#endif
    }
}

void PipelineDefinitionLanguage::writePlain(
    const std::string& data,
    const fs::path& filename)
{
    const fs::path outPath{ shaderOutputDir / filename };
    fs::create_directories(outPath.parent_path());

    std::ofstream file{ outPath };
    if (!file.is_open()) {
        throw IOError("Unable to open file " + outPath.string() + " for writing");
    }
    file << data;
}

#ifdef HAS_SPIRV_COMPILER
void PipelineDefinitionLanguage::compileSpirvShaders()
{
    spirvOpts.SetTargetSpirv(spirvVersion);
    spirvOpts.SetTargetEnvironment(targetEnv, targetEnvVersion);

    /**
     * Use shaderOutputDir as primary include directory to consider previously
     * generated shader files first in the include order.
     */
    spirvOpts.SetIncluder(std::make_unique<spirv::FileIncluder>(
        std::vector<fs::path>{ shaderOutputDir, shaderInputDir }
    ));
    for (const auto& str : shaderCompileDefinitions)
    {
        auto pos = str.find('=');
        if (pos == std::string::npos) {
            spirvOpts.AddMacroDefinition(str);
        }
        else {
            spirvOpts.AddMacroDefinition(str.substr(0, pos), str.substr(pos + 1));
        }
    }

    std::vector<std::future<void>> futs;
    for (const auto& info : pendingSpirvCompilations)
    {
        futs.emplace_back(threadPool.async([&info]{
            try {
                compileToSpirv(info);
            }
            catch (const CompilerError&) {}
        }));
    }
    for (auto& f : futs) f.wait();
}

void PipelineDefinitionLanguage::compileToSpirv(const SpirvCompileInfo& info)
{
    const auto& [code, shaderFileName] = info;
    const fs::path outPath{ shaderOutputDir / (shaderFileName.string() + ".spv") };
    fs::create_directories(outPath.parent_path());

    auto result = spirv::generateSpirv(code, shaderFileName, spirvOpts);
    if (result.GetNumErrors() > 0)
    {
        std::cerr << "An error occured during SPIRV compilation: " << result.GetErrorMessage();
        throw CompilerError{};
    }

    std::ofstream file(outPath, std::ios::binary);
    file.write(
        reinterpret_cast<const char*>(result.begin()),
        static_cast<std::streamsize>(
            (result.end() - result.begin()) * sizeof(decltype(result)::element_type)
        )
    );
}
#endif
