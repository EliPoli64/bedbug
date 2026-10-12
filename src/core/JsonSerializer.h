#pragma once

#include "Model.h"

#include <QByteArray>
#include <QJsonObject>

namespace bb {

// Serializes a Model to/from the document format described in PLAN.md §5.
class JsonSerializer
{
public:
    static QJsonObject toJson(const Model &model);
    static Model fromJson(const QJsonObject &root, QString *errorMessage = nullptr);

    static QByteArray toBytes(const Model &model, bool pretty = true);
};

} // namespace bb