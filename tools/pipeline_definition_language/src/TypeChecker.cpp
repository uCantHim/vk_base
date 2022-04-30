#include "TypeChecker.h"

#include <cassert>

#include "Exceptions.h"
#include "ErrorReporter.h"
#include "Util.h"



auto makeDefaultTypeConfig() -> TypeConfiguration
{
    return TypeConfiguration({
        // Fundamental string type
        { stringTypeName, StringType{} },

        // Type of the "global" object, which has both arbitrary field names
        // and some pre-defined fields (e.g. 'Meta')
        { globalObjectTypeName, ObjectType{
            .typeName=globalObjectTypeName,
            .fields={
                { "Meta", { "CompilerMetaData" } },
            }
        }},
        { "CompilerMetaData", ObjectType{
            .typeName="CompilerMetaData",
            .fields={
                { "BaseDir", { stringTypeName } },
            }
        }},

        // The following are custom (non-built-in) types
        { "Variable", StringType{} },
        { "Shader", ObjectType{
            .typeName="Shader",
            .fields={
                { "Source",   { stringTypeName } },
                { "Variable", { "Variable", FieldType::eMap } },
            }
        }},
        { "Program", ObjectType{
            .typeName="Program",
            .fields={
                { "VertexShader", { "Shader" } },
                { "TessControlShader", { "Shader" } },
                { "TessEvalShader", { "Shader" } },
                { "GeometryShader", { "Shader" } },
                { "FragmentShader", { "Shader" } },
            }
        }},
        { "Pipeline", ObjectType{
            .typeName="Pipeline",
            .fields={
                { "Base",    { "Pipeline" } },
                { "Program", { "Program" } },
            }
        }}
    });
}



TypeChecker::TypeChecker(TypeConfiguration config, ErrorReporter& errorReporter)
    :
    config(std::move(config)),
    errorReporter(&errorReporter)
{
}

bool TypeChecker::check(const std::vector<Stmt>& statements)
{
    bool hadError{ false };
    for (const auto& stmt : statements)
    {
        try {
            std::visit(*this, stmt);
        }
        catch (const TypeError& err)
        {
            hadError = true;
            errorReporter->error({
                .location=err.token.location,
                .message="At token " + to_string(err.token.type) + ": " + err.message
            });
        }
    }

    return hadError;
}

void TypeChecker::operator()(const TypeDef& def)
{
    std::visit(*this, def);
}

void TypeChecker::operator()(const EnumTypeDef& def)
{
    auto [it, success] = config.types.try_emplace(def.name, EnumType{ def.name, def.options });
    if (!success) {
        error({}, "Duplicate type definition.");
    }
}

void TypeChecker::operator()(const FieldDefinition& def)
{
    const auto& globalObj = std::get<ObjectType>(this->config.types.at(globalObjectTypeName));
    checkFieldDefinition(globalObj, def, true);  // Allow arbitrary fields on the global object

    // Statement is valid, add defined variable to lookup table
    if (std::holds_alternative<TypedFieldName>(def.name))
    {
        const auto& field = std::get<TypedFieldName>(def.name);
        identifierValues.try_emplace(field.mappedName, def.value.get());
    }
}

void TypeChecker::checkFieldDefinition(
    const ObjectType& parent,
    const FieldDefinition& def,
    const bool allowArbitraryFields)
{
    TypeName expected = std::visit(VariantVisitor{
        [&, this](const TypedFieldName& name){
            if (!(allowArbitraryFields || parent.fields.contains(name.name.name))) {
                error(name.name.token, "Invalid field name \"" + name.name.name + "\".");
            }
            return name.name.name;
        },
        [this, &parent](const TypelessFieldName& name){
            if (!parent.fields.contains(name.name.name)) {
                error(name.name.token, "Invalid field name \"" + name.name.name + "\".");
            }
            return parent.fields.at(name.name.name).storedType;
        },
    }, def.name);

    if (!config.types.contains(expected)) {
        error(getToken(*def.value), "Expected undefined type \"" + expected + "\".");
    }

    if (!std::visit(CheckValueType{ config.types.at(expected), *this }, *def.value)) {
        error({}, "Encountered value of unexpected type.");
    }
}

auto TypeChecker::getToken(const FieldValue& val) -> const Token&
{
    return std::visit([](const auto& v) -> const Token& { return v.token; }, val);
}

void TypeChecker::error(const Token& token, std::string message)
{
    throw TypeError{
        token,
        std::move(message)
    };
}



TypeChecker::CheckValueType::CheckValueType(TypeType& expected, TypeChecker& self)
    :
    expectedType(&expected),
    expectedTypeName(std::visit([](auto& a){ return a.typeName; }, expected)),
    self(&self)
{
}

bool TypeChecker::CheckValueType::operator()(const LiteralValue& val) const
{
    if (!std::holds_alternative<StringType>(*expectedType)) {
        expectedTypeError(val.token, StringType::typeName);
    }
    return true;
}

bool TypeChecker::CheckValueType::operator()(const Identifier& id) const
{
    // Find identifier's type in lookup table
    auto it = self->identifierValues.find(id);
    if (it == self->identifierValues.end()) {
        self->error(id.token, "Use of undeclared identifier \"" + id.name + "\"");
    }

    // Check the referenced value
    return std::visit(CheckValueType{ *this }, *it->second);
}

bool TypeChecker::CheckValueType::operator()(const ListDeclaration& list) const
{
    /**
     * All values in the list must be of the expected type - we only have
     * homogenous lists.
     */

    bool result{ true };
    for (const auto& fieldValue : list.items) {
        result = result && std::visit(*this, fieldValue);
    }

    return result;
}

bool TypeChecker::CheckValueType::operator()(const ObjectDeclaration& obj) const
{
    if (!std::holds_alternative<ObjectType>(*expectedType)) {
        expectedTypeError(obj.token, undefinedObjectType);
    }

    auto& objectType = std::get<ObjectType>(*expectedType);
    for (const auto& field : obj.fields)
    {
        try {
            self->checkFieldDefinition(objectType, field);
        }
        catch (const TypeError& err) {
            // Replace generated error with the correct location
            self->error(err.token, err.message);
        }
    }

    return true;
}

bool TypeChecker::CheckValueType::operator()(const MatchExpression& expr) const
{
    // Test if the matched type exists
    auto it = self->config.types.find(expr.matchedType.name);
    if (it == self->config.types.end())
    {
        self->error(expr.matchedType.token,
                    "Matching on undefined type \"" + expr.matchedType.name + "\".");
    }

    // Check if the matched type is an enum type
    if (!std::holds_alternative<EnumType>(it->second))
    {
        self->error(expr.matchedType.token,
                    "Matching on non-enum type \"" + expr.matchedType.name + "\".");
    }
    auto& enumType = std::get<EnumType>(it->second);

    // Check individual cases
    std::unordered_set<Identifier> matchedCases;
    for (const auto& _case : expr.cases)
    {
        // Check the case enumerator
        auto& name = _case.caseIdentifier.name;
        if (!enumType.options.contains(name))
        {
            self->error(_case.caseIdentifier.token,
                        "No option named \"" + name + "\" in enum \"" + expr.matchedType.name + "\".");
        }

        // Check for duplicate matches
        auto [_, success] = matchedCases.emplace(_case.caseIdentifier);
        if (!success) {
            self->error(_case.caseIdentifier.token, "Duplicate match on \"" + name + "\".");
        }

        // Check the type of the case's value
        try {
            if (!std::visit(CheckValueType{ *this }, *_case.value)) {
                return false;
            }
        }
        catch (const TypeError& err) {
            // Replace generated error with the correct location
            self->error(getToken(*_case.value), err.message);
        }
    }

    return true;
}

void TypeChecker::CheckValueType::expectedTypeError(
    const Token& token,
    const TypeName& actualType) const
{
    self->error(token, "Expected value of type \"" + expectedTypeName + "\""
                       ", but got \"" + actualType + "\".");
}
