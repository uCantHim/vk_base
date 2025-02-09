#include "ObjectConverter.h"

#include "Exceptions.h"
#include "Util.h"



namespace compiler
{

ObjectConverter::ObjectConverter(std::vector<Stmt> _statements, ErrorReporter& errorReporter)
    :
    errorReporter(&errorReporter),
    statements(std::move(_statements)),
    flagTable(FlagTypeCollector{}.collect(statements)),
    identifierTable(IdentifierCollector{ errorReporter }.collect(statements)),
    resolver(flagTable, identifierTable)
{
}

auto ObjectConverter::convert() -> Object
{
    for (const auto& stmt : statements)
    {
        std::visit(*this, stmt);
    }

    return std::move(globalObject);
}

auto ObjectConverter::getFlagTable() const -> const FlagTable&
{
    return flagTable;
}

auto ObjectConverter::getIdentifierTable() const -> const IdentifierTable&
{
    return identifierTable;
}

void ObjectConverter::operator()(const TypeDef& def)
{
    std::visit(*this, def);
}

void ObjectConverter::operator()(const EnumTypeDef&)
{
    // Nothing - enum types are handled by FlagTypeCollector
}

void ObjectConverter::operator()(const FieldDefinition& def)
{
    const auto& [name, value] = def;
    auto variants = resolver.resolve(*value);
    if (variants.size() > 1)
    {
        Variated variated;
        for (auto& var : variants)
        {
            variated.variants.emplace_back(
                var.setFlags,
                std::visit(*this, var.value)
            );
        }

        auto val = std::make_shared<Value>(variated);
        std::visit([this, val](auto& name){ setValue(name, val); }, name);
    }
    else {
        auto val = std::visit(*this, variants.front().value);
        std::visit([this, val](auto& name){ setValue(name, val); }, name);
    }
}

auto ObjectConverter::operator()(const LiteralValue& val) -> std::shared_ptr<Value>
{
    return std::make_shared<Value>(
        std::visit(VariantVisitor{
            [](const StringLiteral& val){ return Literal{ .value=val.value }; },
            [](const NumberLiteral& val){
                return std::visit([](auto&& val){ return Literal{ .value=val }; }, val.value);
            },
        }, val)
    );
}

auto ObjectConverter::operator()(const Identifier& id) -> std::shared_ptr<Value>
{
    return std::make_shared<Value>(
        std::visit(VariantVisitor{
            [&id](const ValueReference&) -> Value {
                return Reference{ id.name };
            },
            [this, &id](const TypeName&) -> Value {
                error(id.token, "Type name as identifier");
                return {};
            },
            [](const DataConstructor& ctor) -> Value {
                return Literal{ .value=ctor.dataName };
            },
        }, identifierTable.get(id))
    );

}

auto ObjectConverter::operator()(const ListDeclaration& list) -> std::shared_ptr<Value>
{
    List result;
    for (const auto& item : list.items) {
        result.values.emplace_back(std::visit(*this, item));
    }

    return std::make_shared<Value>(std::move(result));
}

auto ObjectConverter::operator()(const ObjectDeclaration& obj) -> std::shared_ptr<Value>
{
    Object object;
    Object* old = current;
    current = &object;

    for (const auto& field : obj.fields)
    {
        (*this)(field);
    }

    current = old;
    return std::make_shared<Value>(std::move(object));
}

auto ObjectConverter::operator()(const MatchExpression&) -> std::shared_ptr<Value>
{
    throw InternalLogicError("AST contains a match expression after VariantResolver has been"
                             " called on it. This should not be possible.");
}

void ObjectConverter::error(const Token& token, std::string message)
{
    errorReporter->error(Error{ token.location, message });
    throw InternalLogicError(std::move(message));
}

void ObjectConverter::setValue(
    const TypelessFieldName& fieldName,
    std::shared_ptr<Value> value)
{
    auto [_, success] = current->fields.try_emplace(
        fieldName.name.name,
        SingleValue{ std::move(value) }
    );

    if (!success) {
        error(fieldName.name.token, "Duplicate object property \"" + fieldName.name.name + "\".");
    }
}

void ObjectConverter::setValue(
    const TypedFieldName& name,
    std::shared_ptr<Value> value)
{
    const auto& [fieldName, mapName] = name;
    auto [it, _0] = current->fields.try_emplace(fieldName.name, MapValue{});
    auto& map = std::get<MapValue>(it->second);

    auto [_1, success] = map.values.try_emplace(mapName.name, value);
    if (!success) {
        error(mapName.token,
              "Duplicate entry \"" + mapName.name + "\" in field \"" + fieldName.name + "\".");
    }
}

} // namespace compiler
