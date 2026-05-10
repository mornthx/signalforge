import QtQuick

// ADR-010: minimal QML host scene for QQuickWidget. The C++
// side calls `chart->setParentItem(hostWidget->rootObject())`
// with this Item as the rootObject — without a loaded source,
// QQuickWidget's rootObject() is null and Chart orphans itself.
//
// Sized to the QQuickWidget via SizeRootObjectToView resize mode
// (set on the C++ side); anchors here are belt-and-suspenders.
Item {
    anchors.fill: parent
    objectName: "chartHost"
}
