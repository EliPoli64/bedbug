#include "DialectSelector.h"

#include "DialectRegistry.h"
#include "SqlDialect.h"

namespace bb {

DialectSelector::DialectSelector(QWidget *parent)
    : QComboBox(parent)
{
    setObjectName(QStringLiteral("dialectSelector"));
    setToolTip(tr("Target SQL engine"));
    setAccessibleName(tr("Target SQL engine"));

    for (const SqlDialect *dialect : DialectRegistry::all()) {
        addItem(dialect->displayName(), dialect->id());
        setItemData(count() - 1, tr("Generate SQL and show type labels for %1")
                                     .arg(dialect->displayName()),
                    Qt::ToolTipRole);
    }

    if (const SqlDialect *fallback = DialectRegistry::fallback())
        setCurrentDialectId(fallback->id());

    connect(this, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString id = itemData(index).toString();
        if (id.isEmpty() || id == m_currentId)
            return;
        m_currentId = id;
        emit dialectSelected(id);
    });
}

QString DialectSelector::currentDialectId() const
{
    return m_currentId;
}

void DialectSelector::setCurrentDialectId(const QString &id)
{
    const int index = findData(id);
    if (index < 0)
        return;

    if (currentIndex() == index) {
        if (m_currentId != id) {
            m_currentId = id;
            emit dialectSelected(id);
        }
        return;
    }

    // currentIndexChanged updates m_currentId and emits dialectSelected().
    setCurrentIndex(index);
}

} // namespace bb
