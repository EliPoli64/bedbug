#include "ColumnEditorDialog.h"
#include "DocumentController.h"
#include "EntityEditorPanel.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QtTest>

using namespace bb;

namespace {

CanonicalType bigintType()
{
    CanonicalType t;
    t.kind = CanonicalType::Kind::BigInt;
    return t;
}

} // namespace

class EntityEditorTest : public QObject
{
    Q_OBJECT

private slots:
    void panelReflectsModelChanges();
    void addColumnViaButton_showsInList();
    void removeColumnViaButton_removesSelected();
    void primaryKeySelectsAutoIncrementAndUnique();
    void enumRowTogglesWithKind();
    void dialogRejectsDuplicateNames();
    void dialogRejectsEmptyName();
};

void EntityEditorTest::panelReflectsModelChanges()
{
    DocumentController controller;
    EntityEditorPanel panel;
    panel.setDocument(&controller);

    const Id entityId = controller.addEntity(QStringLiteral("customer"))->id;
    panel.showEntity(entityId);

    QVERIFY(panel.isEnabled());
    QCOMPARE(panel.columnCount(), 0);

    controller.addColumn(entityId, QStringLiteral("id"), bigintType(), false, true, false, true);
    QCOMPARE(panel.columnCount(), 1);
    const Id idColumnId = panel.columnIdAt(0);
    QVERIFY(!idColumnId.isNull());

    controller.addColumn(entityId, QStringLiteral("email"), bigintType(), false, false, true, false);
    QCOMPARE(panel.columnCount(), 2);

    controller.removeColumn(entityId, idColumnId);
    QCOMPARE(panel.columnCount(), 1);

    panel.clear();
    QVERIFY(!panel.isEnabled());
    QCOMPARE(panel.columnCount(), 0);
}

void EntityEditorTest::addColumnViaButton_showsInList()
{
    DocumentController controller;
    EntityEditorPanel panel;
    panel.setDocument(&controller);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));

    const Id entityId = controller.addEntity(QStringLiteral("customer"))->id;
    panel.showEntity(entityId);

    QTimer::singleShot(0, [&panel] {
        auto *dialog = panel.findChild<ColumnEditorDialog *>();
        QVERIFY(dialog);
        dialog->findChild<QLineEdit *>(QStringLiteral("nameEdit"))->setText(QStringLiteral("id"));
        dialog->accept();
    });

    auto *addButton = panel.findChild<QPushButton *>(QStringLiteral("addButton"));
    QVERIFY(addButton);
    QTest::mouseClick(addButton, Qt::LeftButton);

    QCOMPARE(panel.columnCount(), 1);
    QVERIFY(!panel.columnIdAt(0).isNull());

    panel.close();
}

void EntityEditorTest::removeColumnViaButton_removesSelected()
{
    DocumentController controller;
    EntityEditorPanel panel;
    panel.setDocument(&controller);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));

    const Id entityId = controller.addEntity(QStringLiteral("customer"))->id;
    controller.addColumn(entityId, QStringLiteral("id"), bigintType(), false, true, false, true);
    controller.addColumn(entityId, QStringLiteral("email"), bigintType(), false, false, true, false);
    panel.showEntity(entityId);
    QCOMPARE(panel.columnCount(), 2);

    auto *list = panel.findChild<QListWidget *>(QStringLiteral("columnList"));
    QVERIFY(list);
    list->setCurrentRow(0);

    auto answerBox = [](QMessageBox::StandardButton answer) {
        QTimer::singleShot(0, [answer] {
            const QList<QWidget *> topLevels = QApplication::topLevelWidgets();
            for (QWidget *widget : topLevels) {
                if (auto *box = qobject_cast<QMessageBox *>(widget)) {
                    for (QAbstractButton *button : box->buttons()) {
                        if (box->standardButton(button) == answer) {
                            button->click();
                            return;
                        }
                    }
                }
            }
        });
    };

    // Confirm removal with Yes: the armed column goes away.
    answerBox(QMessageBox::Yes);
    auto *removeButton = panel.findChild<QPushButton *>(QStringLiteral("removeButton"));
    QVERIFY(removeButton);
    QTest::mouseClick(removeButton, Qt::LeftButton);
    QCOMPARE(panel.columnCount(), 1);

    // Cancel with No: nothing is removed even though a column stays armed.
    const int before = controller.model()->findEntity(entityId)->columns.size();
    answerBox(QMessageBox::No);
    QTest::mouseClick(removeButton, Qt::LeftButton);
    QCOMPARE(controller.model()->findEntity(entityId)->columns.size(), before);
    QCOMPARE(panel.columnCount(), before);

    panel.close();
}

void EntityEditorTest::primaryKeySelectsAutoIncrementAndUnique()
{
    ColumnEditorDialog dialog;
    auto *pk = dialog.findChild<QCheckBox *>(QStringLiteral("primaryKeyCheck"));
    auto *ai = dialog.findChild<QCheckBox *>(QStringLiteral("autoIncrementCheck"));
    auto *unique = dialog.findChild<QCheckBox *>(QStringLiteral("uniqueCheck"));
    QVERIFY(pk && ai && unique);

    QVERIFY(!ai->isChecked());
    pk->setChecked(true);
    QVERIFY(ai->isChecked());
    QVERIFY(unique->isChecked());
}

void EntityEditorTest::enumRowTogglesWithKind()
{
    ColumnEditorDialog dialog;
    auto *kind = dialog.findChild<QComboBox *>(QStringLiteral("kindCombo"));
    auto *enumValues = dialog.findChild<QLineEdit *>(QStringLiteral("enumValuesEdit"));
    QVERIFY(kind && enumValues);

    // Default kind is Varchar: enum values row is hidden.
    QVERIFY(enumValues->isHidden());

    kind->setCurrentText(QStringLiteral("Enum"));
    QVERIFY(!enumValues->isHidden());
    QVERIFY(enumValues->isEnabled());

    kind->setCurrentText(QStringLiteral("BigInt"));
    QVERIFY(enumValues->isHidden());
}

void EntityEditorTest::dialogRejectsDuplicateNames()
{
    ColumnEditorDialog dialog;
    dialog.setExistingNames({QStringLiteral("id"), QStringLiteral("email")});
    auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("nameEdit"));

    name->setText(QStringLiteral("ID"));   // case-insensitive duplicate
    QVERIFY(!dialog.validationError({QStringLiteral("id"), QStringLiteral("email")}).isEmpty());

    name->setText(QStringLiteral("fresh"));
    QVERIFY(dialog.validationError({QStringLiteral("id"), QStringLiteral("email")}).isEmpty());
}

void EntityEditorTest::dialogRejectsEmptyName()
{
    ColumnEditorDialog dialog;
    dialog.findChild<QLineEdit *>(QStringLiteral("nameEdit"))->setText(QStringLiteral("   "));
    QVERIFY(!dialog.validationError({}).isEmpty());
}

QTEST_MAIN(EntityEditorTest)

#include "tst_editor.moc"