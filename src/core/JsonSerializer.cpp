#include "JsonSerializer.h"

#include "CanonicalType.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace bb {

namespace {

constexpr QLatin1StringView kKeyFormatVersion    {"formatVersion"};
constexpr QLatin1StringView kKeyModel            {"model"};
constexpr QLatin1StringView kKeyName             {"name"};
constexpr QLatin1StringView kKeyPreferredDialect {"preferredDialect"};
constexpr QLatin1StringView kKeyEntities         {"entities"};
constexpr QLatin1StringView kKeyRelationships    {"relationships"};
constexpr QLatin1StringView kKeyDiagrams         {"diagrams"};
constexpr QLatin1StringView kKeyId               {"id"};
constexpr QLatin1StringView kKeySchema           {"schema"};
constexpr QLatin1StringView kKeyComment          {"comment"};
constexpr QLatin1StringView kKeyColumns          {"columns"};
constexpr QLatin1StringView kKeyIndexes          {"indexes"};
constexpr QLatin1StringView kKeyType             {"type"};
constexpr QLatin1StringView kKeyKind             {"kind"};
constexpr QLatin1StringView kKeyLength           {"length"};
constexpr QLatin1StringView kKeyPrecision        {"precision"};
constexpr QLatin1StringView kKeyScale            {"scale"};
constexpr QLatin1StringView kKeyEnumValues       {"enumValues"};
constexpr QLatin1StringView kKeyNullable         {"nullable"};
constexpr QLatin1StringView kKeyPrimaryKey       {"primaryKey"};
constexpr QLatin1StringView kKeyUnique           {"unique"};
constexpr QLatin1StringView kKeyAutoIncrement    {"autoIncrement"};
constexpr QLatin1StringView kKeyDefaultValue     {"defaultValue"};
constexpr QLatin1StringView kKeyDialectOverrides {"dialectOverrides"};
constexpr QLatin1StringView kKeyNativeType       {"nativeType"};
constexpr QLatin1StringView kKeyRawType          {"rawType"};
constexpr QLatin1StringView kKeyEngine           {"engine"};
constexpr QLatin1StringView kKeyColumnIds        {"columnIds"};
constexpr QLatin1StringView kKeyChildEntity      {"childEntity"};
constexpr QLatin1StringView kKeyChildColumns     {"childColumns"};
constexpr QLatin1StringView kKeyParentEntity     {"parentEntity"};
constexpr QLatin1StringView kKeyParentColumns    {"parentColumns"};
constexpr QLatin1StringView kKeyIdentifying      {"identifying"};
constexpr QLatin1StringView kKeyOnDelete         {"onDelete"};
constexpr QLatin1StringView kKeyOnUpdate         {"onUpdate"};
constexpr QLatin1StringView kKeyNodes            {"nodes"};
constexpr QLatin1StringView kKeyEntity           {"entity"};
constexpr QLatin1StringView kKeyPos              {"pos"};
constexpr QLatin1StringView kKeyZoom             {"zoom"};
constexpr QLatin1StringView kKeyPagesX           {"pagesX"};
constexpr QLatin1StringView kKeyPagesY           {"pagesY"};
constexpr QLatin1StringView kKeyOriginX          {"originX"};
constexpr QLatin1StringView kKeyOriginY          {"originY"};

// ---- Serialization ---------------------------------------------------------

QJsonObject typeToJson(const CanonicalType &type)
{
    QJsonObject obj;
    obj.insert(kKeyKind, CanonicalType::kindToString(type.kind));
    if (type.length > 0)
        obj.insert(kKeyLength, type.length);
    if (type.precision > 0)
        obj.insert(kKeyPrecision, type.precision);
    if (type.scale > 0)
        obj.insert(kKeyScale, type.scale);
    if (!type.enumValues.isEmpty()) {
        QJsonArray values;
        for (const QString &value : type.enumValues)
            values.append(value);
        obj.insert(kKeyEnumValues, values);
    }
    return obj;
}

QJsonArray idListToJson(const QList<Id> &ids)
{
    QJsonArray arr;
    for (const Id &id : ids)
        arr.append(id.toString());
    return arr;
}

QJsonObject columnToJson(const Column &column)
{
    QJsonObject obj;
    obj.insert(kKeyId, column.id.toString());
    obj.insert(kKeyName, column.name);
    obj.insert(kKeyType, typeToJson(column.type));
    obj.insert(kKeyNullable, column.nullable);
    obj.insert(kKeyPrimaryKey, column.primaryKey);
    obj.insert(kKeyUnique, column.unique);
    obj.insert(kKeyAutoIncrement, column.autoIncrement);
    if (!column.defaultValue.isEmpty())
        obj.insert(kKeyDefaultValue, column.defaultValue);
    if (!column.comment.isEmpty())
        obj.insert(kKeyComment, column.comment);
    if (!column.dialectOverrides.isEmpty()) {
        QJsonObject overrides;
        const QList<QString> keys = column.dialectOverrides.keys();
        for (const QString &key : keys)
            overrides.insert(key, column.dialectOverrides.value(key));
        obj.insert(kKeyDialectOverrides, overrides);
    }
    if (!column.nativeType.isEmpty()) {
        QJsonObject native;
        native.insert(kKeyRawType, column.nativeType.rawType);
        native.insert(kKeyEngine, column.nativeType.engine);
        obj.insert(kKeyNativeType, native);
    }
    return obj;
}

QJsonObject indexToJson(const Index &index)
{
    QJsonObject obj;
    obj.insert(kKeyId, index.id.toString());
    obj.insert(kKeyName, index.name);
    obj.insert(kKeyColumnIds, idListToJson(index.columnIds));
    obj.insert(kKeyUnique, index.unique);
    return obj;
}

QJsonObject entityToJson(const Entity &entity)
{
    QJsonObject obj;
    obj.insert(kKeyId, entity.id.toString());
    obj.insert(kKeyName, entity.name);
    obj.insert(kKeySchema, entity.schema);
    obj.insert(kKeyComment, entity.comment);

    QJsonArray columns;
    for (const Column &column : entity.columns)
        columns.append(columnToJson(column));
    obj.insert(kKeyColumns, columns);

    QJsonArray indexes;
    for (const Index &index : entity.indexes)
        indexes.append(indexToJson(index));
    obj.insert(kKeyIndexes, indexes);

    return obj;
}

QJsonObject relationshipToJson(const Relationship &relationship)
{
    QJsonObject obj;
    obj.insert(kKeyId, relationship.id.toString());
    obj.insert(kKeyName, relationship.name);
    obj.insert(kKeyChildEntity, relationship.childEntity.toString());
    obj.insert(kKeyChildColumns, idListToJson(relationship.childColumns));
    obj.insert(kKeyParentEntity, relationship.parentEntity.toString());
    obj.insert(kKeyParentColumns, idListToJson(relationship.parentColumns));
    obj.insert(kKeyIdentifying, relationship.identifying);
    if (!relationship.onDelete.isEmpty())
        obj.insert(kKeyOnDelete, relationship.onDelete);
    if (!relationship.onUpdate.isEmpty())
        obj.insert(kKeyOnUpdate, relationship.onUpdate);
    return obj;
}

QJsonObject diagramToJson(const Diagram &diagram)
{
    QJsonObject obj;
    obj.insert(kKeyId, diagram.id.toString());
    obj.insert(kKeyName, diagram.name);

    QJsonArray nodes;
    for (const DiagramNode &node : diagram.nodes) {
        QJsonObject nodeObj;
        nodeObj.insert(kKeyEntity, node.entity.toString());
        QJsonArray pos;
        pos.append(node.pos.x());
        pos.append(node.pos.y());
        nodeObj.insert(kKeyPos, pos);
        nodes.append(nodeObj);
    }
    obj.insert(kKeyNodes, nodes);

    obj.insert(kKeyZoom, diagram.zoom);
    // Page grid (MySQL Workbench style).
    if (diagram.pagesX != 1)
        obj.insert(kKeyPagesX, diagram.pagesX);
    if (diagram.pagesY != 1)
        obj.insert(kKeyPagesY, diagram.pagesY);
    if (diagram.originX != 0)
        obj.insert(kKeyOriginX, diagram.originX);
    if (diagram.originY != 0)
        obj.insert(kKeyOriginY, diagram.originY);
    return obj;
}

// ---- Parsing ---------------------------------------------------------------

bool parseString(const QJsonObject &obj, QLatin1StringView key, QString *out, QString *err,
                 const QString &at, bool required)
{
    const QJsonValue value = obj.value(key);
    if (value.isUndefined() || value.isNull()) {
        if (required) {
            *err = at + QStringLiteral(": missing '") + QString::fromUtf8(key.data(), key.size()) + "'";
            return false;
        }
        out->clear();
        return true;
    }
    if (!value.isString()) {
        *err = at + QStringLiteral(": '") + QString::fromUtf8(key.data(), key.size()) + "' must be a string";
        return false;
    }
    *out = value.toString();
    return true;
}

bool parseBool(const QJsonObject &obj, QLatin1StringView key, bool *out, bool def, QString *err,
               const QString &at)
{
    const QJsonValue value = obj.value(key);
    if (value.isUndefined() || value.isNull()) {
        *out = def;
        return true;
    }
    if (!value.isBool()) {
        *err = at + QStringLiteral(": '") + QString::fromUtf8(key.data(), key.size()) + "' must be a boolean";
        return false;
    }
    *out = value.toBool();
    return true;
}

bool parseId(const QJsonObject &obj, QLatin1StringView key, Id *out, QString *err, const QString &at)
{
    const QJsonValue value = obj.value(key);
    if (!value.isString()) {
        *err = at + QStringLiteral(": missing or non-string '") + QString::fromUtf8(key.data(), key.size()) + "'";
        return false;
    }
    bool idOk = false;
    *out = Id::fromString(value.toString(), &idOk);
    if (!idOk) {
        *err = at + QStringLiteral(": invalid id '") + value.toString() + "' for '" + QString::fromUtf8(key.data(), key.size()) + "'";
        return false;
    }
    return true;
}

bool parseIdList(const QJsonValue &value, QLatin1StringView key, QList<Id> *out, QString *err,
                 const QString &at)
{
    out->clear();
    if (!value.isArray()) {
        *err = at + QStringLiteral(": '") + QString::fromUtf8(key.data(), key.size()) + "' must be an array";
        return false;
    }
    const QJsonArray arr = value.toArray();
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonValue entry = arr.at(i);
        if (!entry.isString()) {
            *err = at + QStringLiteral(": '") + QString::fromUtf8(key.data(), key.size()) + "' entries must be strings";
            return false;
        }
        bool idOk = false;
        Id id = Id::fromString(entry.toString(), &idOk);
        if (!idOk) {
            *err = at + QStringLiteral(": invalid id in '") + QString::fromUtf8(key.data(), key.size()) + "'";
            return false;
        }
        out->append(id);
    }
    return true;
}

bool parseType(const QJsonValue &value, CanonicalType *out, QString *err, const QString &at)
{
    if (!value.isObject()) {
        *err = at + QStringLiteral(": 'type' must be an object");
        return false;
    }
    const QJsonObject obj = value.toObject();

    const QJsonValue kindValue = obj.value(kKeyKind);
    if (!kindValue.isString()) {
        *err = at + QStringLiteral(": 'type.kind' must be a string");
        return false;
    }
    bool kindOk = false;
    out->kind = CanonicalType::stringToKind(kindValue.toString(), &kindOk);
    if (!kindOk) {
        *err = at + QStringLiteral(": unknown type kind '") + kindValue.toString() + "'";
        return false;
    }

    out->length = obj.value(kKeyLength).toInt(0);
    out->precision = obj.value(kKeyPrecision).toInt(0);
    out->scale = obj.value(kKeyScale).toInt(0);
    out->enumValues.clear();

    const QJsonValue enumValue = obj.value(kKeyEnumValues);
    if (enumValue.isArray()) {
        const QJsonArray arr = enumValue.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            const QJsonValue entry = arr.at(i);
            if (!entry.isString()) {
                *err = at + QStringLiteral(": 'type.enumValues' entries must be strings");
                return false;
            }
            out->enumValues.append(entry.toString());
        }
    }
    return true;
}

bool parseColumn(const QJsonObject &obj, Column *out, QString *err, const QString &at)
{
    if (!parseId(obj, kKeyId, &out->id, err, at))
        return false;
    if (!parseString(obj, kKeyName, &out->name, err, at, true))
        return false;
    if (!parseType(obj.value(kKeyType), &out->type, err, at))
        return false;
    if (!parseBool(obj, kKeyNullable, &out->nullable, true, err, at))
        return false;
    if (!parseBool(obj, kKeyPrimaryKey, &out->primaryKey, false, err, at))
        return false;
    if (!parseBool(obj, kKeyUnique, &out->unique, false, err, at))
        return false;
    if (!parseBool(obj, kKeyAutoIncrement, &out->autoIncrement, false, err, at))
        return false;
    if (!parseString(obj, kKeyDefaultValue, &out->defaultValue, err, at, false))
        return false;
    if (!parseString(obj, kKeyComment, &out->comment, err, at, false))
        return false;

    out->dialectOverrides.clear();
    const QJsonValue overridesValue = obj.value(kKeyDialectOverrides);
    if (!overridesValue.isUndefined() && !overridesValue.isNull()) {
        if (!overridesValue.isObject()) {
            *err = at + QStringLiteral(": 'dialectOverrides' must be an object");
            return false;
        }
        const QJsonObject overrides = overridesValue.toObject();
        for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
            if (!it.value().isString()) {
                *err = at + QStringLiteral(": 'dialectOverrides.") + it.key() + "' must be a string";
                return false;
            }
            out->dialectOverrides.insert(it.key(), it.value().toString());
        }
    }

    out->nativeType = DialectHint{};
    const QJsonValue nativeValue = obj.value(kKeyNativeType);
    if (!nativeValue.isUndefined() && !nativeValue.isNull()) {
        if (!nativeValue.isObject()) {
            *err = at + QStringLiteral(": 'nativeType' must be an object");
            return false;
        }
        const QJsonObject native = nativeValue.toObject();
        if (!parseString(native, kKeyRawType, &out->nativeType.rawType, err, at, true))
            return false;
        if (!parseString(native, kKeyEngine, &out->nativeType.engine, err, at, true))
            return false;
    }
    return true;
}

bool parseIndex(const QJsonObject &obj, Index *out, QString *err, const QString &at)
{
    if (!parseId(obj, kKeyId, &out->id, err, at))
        return false;
    if (!parseString(obj, kKeyName, &out->name, err, at, true))
        return false;
    if (!parseIdList(obj.value(kKeyColumnIds), kKeyColumnIds, &out->columnIds, err, at))
        return false;
    if (!parseBool(obj, kKeyUnique, &out->unique, false, err, at))
        return false;
    return true;
}

bool parseEntity(const QJsonObject &obj, Entity *out, QString *err, const QString &at)
{
    if (!parseId(obj, kKeyId, &out->id, err, at))
        return false;
    if (!parseString(obj, kKeyName, &out->name, err, at, true))
        return false;
    if (!parseString(obj, kKeySchema, &out->schema, err, at, false))
        return false;
    if (!parseString(obj, kKeyComment, &out->comment, err, at, false))
        return false;

    out->columns.clear();
    const QJsonValue columnsValue = obj.value(kKeyColumns);
    if (!columnsValue.isArray()) {
        *err = at + QStringLiteral(": 'columns' must be an array");
        return false;
    }
    const QJsonArray columnsArr = columnsValue.toArray();
    for (int i = 0; i < columnsArr.size(); ++i) {
        const QJsonValue entry = columnsArr.at(i);
        if (!entry.isObject()) {
            *err = at + QStringLiteral(": 'columns[%1]' must be an object").arg(i);
            return false;
        }
        Column column;
        if (!parseColumn(entry.toObject(), &column, err, at + QStringLiteral(".columns[%1]").arg(i)))
            return false;
        out->columns.append(column);
    }

    out->indexes.clear();
    const QJsonValue indexesValue = obj.value(kKeyIndexes);
    if (!indexesValue.isArray()) {
        *err = at + QStringLiteral(": 'indexes' must be an array");
        return false;
    }
    const QJsonArray indexesArr = indexesValue.toArray();
    for (int i = 0; i < indexesArr.size(); ++i) {
        const QJsonValue entry = indexesArr.at(i);
        if (!entry.isObject()) {
            *err = at + QStringLiteral(": 'indexes[%1]' must be an object").arg(i);
            return false;
        }
        Index index;
        if (!parseIndex(entry.toObject(), &index, err, at + QStringLiteral(".indexes[%1]").arg(i)))
            return false;
        out->indexes.append(index);
    }
    return true;
}

bool parseRelationship(const QJsonObject &obj, Relationship *out, QString *err, const QString &at)
{
    if (!parseId(obj, kKeyId, &out->id, err, at))
        return false;
    if (!parseString(obj, kKeyName, &out->name, err, at, true))
        return false;
    if (!parseId(obj, kKeyChildEntity, &out->childEntity, err, at))
        return false;
    if (!parseIdList(obj.value(kKeyChildColumns), kKeyChildColumns, &out->childColumns, err, at))
        return false;
    if (!parseId(obj, kKeyParentEntity, &out->parentEntity, err, at))
        return false;
    if (!parseIdList(obj.value(kKeyParentColumns), kKeyParentColumns, &out->parentColumns, err, at))
        return false;
    if (!parseBool(obj, kKeyIdentifying, &out->identifying, false, err, at))
        return false;
    if (!parseString(obj, kKeyOnDelete, &out->onDelete, err, at, false))
        return false;
    if (!parseString(obj, kKeyOnUpdate, &out->onUpdate, err, at, false))
        return false;
    return true;
}

bool parseDiagram(const QJsonObject &obj, Diagram *out, QString *err, const QString &at)
{
    if (!parseId(obj, kKeyId, &out->id, err, at))
        return false;
    if (!parseString(obj, kKeyName, &out->name, err, at, true))
        return false;

    out->nodes.clear();
    const QJsonValue nodesValue = obj.value(kKeyNodes);
    if (!nodesValue.isUndefined() && !nodesValue.isNull()) {
        if (!nodesValue.isArray()) {
            *err = at + QStringLiteral(": 'nodes' must be an array");
            return false;
        }
        const QJsonArray nodesArr = nodesValue.toArray();
        for (int i = 0; i < nodesArr.size(); ++i) {
            const QJsonValue entry = nodesArr.at(i);
            if (!entry.isObject()) {
                *err = at + QStringLiteral(": 'nodes[%1]' must be an object").arg(i);
                return false;
            }
            const QJsonObject nodeObj = entry.toObject();
            DiagramNode node;
            if (!parseId(nodeObj, kKeyEntity, &node.entity, err, at))
                return false;

            const QJsonValue posValue = nodeObj.value(kKeyPos);
            if (!posValue.isArray() || posValue.toArray().size() != 2) {
                *err = at + QStringLiteral(": 'pos' must be an [x, y] array");
                return false;
            }
            node.pos = QPointF(posValue.toArray().at(0).toDouble(0.0),
                               posValue.toArray().at(1).toDouble(0.0));
            out->nodes.append(node);
        }
    }

    out->zoom = obj.value(kKeyZoom).toDouble(1.0);

    const QJsonValue pagesXValue = obj.value(kKeyPagesX);
    if (pagesXValue.isDouble())
        out->pagesX = qMax(1, static_cast<int>(pagesXValue.toDouble()));
    const QJsonValue pagesYValue = obj.value(kKeyPagesY);
    if (pagesYValue.isDouble())
        out->pagesY = qMax(1, static_cast<int>(pagesYValue.toDouble()));
    const QJsonValue originXValue = obj.value(kKeyOriginX);
    if (originXValue.isDouble())
        out->originX = static_cast<int>(originXValue.toDouble());
    const QJsonValue originYValue = obj.value(kKeyOriginY);
    if (originYValue.isDouble())
        out->originY = static_cast<int>(originYValue.toDouble());

    return true;
}

} // namespace

// ---- Public API -------------------------------------------------------------

QJsonObject JsonSerializer::toJson(const Model &model)
{
    QJsonObject root;
    root.insert(kKeyFormatVersion, model.formatVersion);

    QJsonObject modelObj;
    modelObj.insert(kKeyName, model.name);
    modelObj.insert(kKeyPreferredDialect, model.preferredDialect);
    root.insert(kKeyModel, modelObj);

    QJsonArray entities;
    for (const Entity &entity : model.entities)
        entities.append(entityToJson(entity));
    root.insert(kKeyEntities, entities);

    QJsonArray relationships;
    for (const Relationship &relationship : model.relationships)
        relationships.append(relationshipToJson(relationship));
    root.insert(kKeyRelationships, relationships);

    QJsonArray diagrams;
    for (const Diagram &diagram : model.diagrams)
        diagrams.append(diagramToJson(diagram));
    root.insert(kKeyDiagrams, diagrams);

    return root;
}

Model JsonSerializer::fromJson(const QJsonObject &root, QString *errorMessage)
{
    QString localError;
    QString *err = errorMessage ? errorMessage : &localError;

    Model model;

    const QJsonValue versionValue = root.value(kKeyFormatVersion);
    if (versionValue.isUndefined() || versionValue.isNull()) {
        *err = QStringLiteral("missing 'formatVersion'");
        return model;
    }
    if (!versionValue.isString()) {
        *err = QStringLiteral("'formatVersion' must be a string");
        return model;
    }
    const QString version = versionValue.toString();
    if (version != Model::currentFormatVersion()) {
        *err = QStringLiteral("unsupported format version '%1' (expected '%2')")
                   .arg(version, Model::currentFormatVersion());
        return model;
    }
    model.formatVersion = version;

    const QJsonValue modelValue = root.value(kKeyModel);
    if (!modelValue.isUndefined() && !modelValue.isNull()) {
        if (!modelValue.isObject()) {
            *err = QStringLiteral("'model' must be an object");
            return model;
        }
        const QJsonObject modelObj = modelValue.toObject();
        if (!parseString(modelObj, kKeyName, &model.name, err, QStringLiteral("model"), false))
            return model;
        if (!parseString(modelObj, kKeyPreferredDialect, &model.preferredDialect, err,
                         QStringLiteral("model"), false))
            return model;
    }

    const QJsonValue entitiesValue = root.value(kKeyEntities);
    if (entitiesValue.isUndefined() || entitiesValue.isNull()) {
        *err = QStringLiteral("missing 'entities'");
        return model;
    }
    if (!entitiesValue.isArray()) {
        *err = QStringLiteral("'entities' must be an array");
        return model;
    }
    const QJsonArray entitiesArr = entitiesValue.toArray();
    for (int i = 0; i < entitiesArr.size(); ++i) {
        const QJsonValue entry = entitiesArr.at(i);
        if (!entry.isObject()) {
            *err = QStringLiteral("'entities[%1]' must be an object").arg(i);
            return model;
        }
        Entity entity;
        if (!parseEntity(entry.toObject(), &entity, err, QStringLiteral("entities[%1]").arg(i)))
            return model;
        model.entities.append(entity);
    }

    const QJsonValue relationshipsValue = root.value(kKeyRelationships);
    if (!relationshipsValue.isUndefined() && !relationshipsValue.isNull()) {
        if (!relationshipsValue.isArray()) {
            *err = QStringLiteral("'relationships' must be an array");
            return model;
        }
        const QJsonArray relsArr = relationshipsValue.toArray();
        for (int i = 0; i < relsArr.size(); ++i) {
            const QJsonValue entry = relsArr.at(i);
            if (!entry.isObject()) {
                *err = QStringLiteral("'relationships[%1]' must be an object").arg(i);
                return model;
            }
            Relationship relationship;
            if (!parseRelationship(entry.toObject(), &relationship, err,
                                   QStringLiteral("relationships[%1]").arg(i)))
                return model;
            model.relationships.append(relationship);
        }
    }

    const QJsonValue diagramsValue = root.value(kKeyDiagrams);
    if (!diagramsValue.isUndefined() && !diagramsValue.isNull()) {
        if (!diagramsValue.isArray()) {
            *err = QStringLiteral("'diagrams' must be an array");
            return model;
        }
        const QJsonArray diagramsArr = diagramsValue.toArray();
        for (int i = 0; i < diagramsArr.size(); ++i) {
            const QJsonValue entry = diagramsArr.at(i);
            if (!entry.isObject()) {
                *err = QStringLiteral("'diagrams[%1]' must be an object").arg(i);
                return model;
            }
            Diagram diagram;
            if (!parseDiagram(entry.toObject(), &diagram, err, QStringLiteral("diagrams[%1]").arg(i)))
                return model;
            model.diagrams.append(diagram);
        }
    }

    return model;
}

QByteArray JsonSerializer::toBytes(const Model &model, bool pretty)
{
    const QJsonDocument doc(toJson(model));
    return pretty ? doc.toJson(QJsonDocument::Indented) : doc.toJson(QJsonDocument::Compact);
}

} // namespace bb