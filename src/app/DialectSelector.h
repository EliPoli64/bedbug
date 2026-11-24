#pragma once

#include <QComboBox>

namespace bb {

// Toolbar control listing the registered SQL dialects (PLAN.md §7.1). Changing the
// selection is a view/export parameter only; it never mutates the model.
class DialectSelector : public QComboBox
{
    Q_OBJECT

public:
    explicit DialectSelector(QWidget *parent = nullptr);

    QString currentDialectId() const;
    void setCurrentDialectId(const QString &id);

signals:
    void dialectSelected(const QString &id);

private:
    QString m_currentId;
};

} // namespace bb
