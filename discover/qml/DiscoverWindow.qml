import QtQuick 2.1
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.1
import "navigation.js" as Navigation

ConditionalLoader
{
    id: window
    readonly property Component applicationListComp: Qt.createComponent("qrc:/qml/ApplicationsListPage.qml")
    readonly property Component applicationComp: Qt.createComponent("qrc:/qml/ApplicationPage.qml")
    readonly property Component categoryComp: Qt.createComponent("qrc:/qml/ApplicationsListPage.qml")
    readonly property Component reviewsComp: Qt.createComponent("qrc:/qml/ReviewsPage.qml")

    //toplevels
    readonly property Component topBrowsingComp: Qt.createComponent("qrc:/qml/BrowsingPage.qml")
    readonly property Component topInstalledComp: Qt.createComponent("qrc:/qml/InstalledPage.qml")
    readonly property Component topUpdateComp: Qt.createComponent("qrc:/qml/UpdatesPage.qml")
    readonly property Component topSourcesComp: Qt.createComponent("qrc:/qml/SourcesPage.qml")
    property Component currentTopLevel: defaultStartup ? topBrowsingComp : loadingComponent
    property bool defaultStartup: true
    property bool navigationEnabled: true
    readonly property QtObject stack: item.stack

    objectName: "DiscoverMainWindow"

    visible: true

    function clearSearch() {
//         if (toolbar.search)
//             toolbar.search.text=""
    }

    Component {
        id: loadingComponent
        Item {
            Label {
                text: i18n("Loading...")
                font.pointSize: 52
                anchors.centerIn: parent
            }
        }
    }

    ExclusiveGroup { id: appTabs }

    property list<Action> awesome: [
        TopLevelPageData {
            iconName: "tools-wizard"
            text: i18n("Discover")
            component: topBrowsingComp
            objectName: "discover"
            shortcut: "Alt+D"
        },
        TopLevelPageData {
            iconName: "applications-other"
            text: TransactionModel.count == 0 ? i18n("Installed") : i18n("Installing...")
            component: topInstalledComp
            objectName: "installed"
            shortcut: "Alt+I"
        },
        TopLevelPageData {
            iconName: "system-software-update"
            text: ResourcesModel.updatesCount==0 ? i18n("No Updates") : i18n("Update (%1)", ResourcesModel.updatesCount)
            enabled: ResourcesModel.updatesCount>0
            component: topUpdateComp
            objectName: "update"
            shortcut: "Alt+U"
        }
    ]

    Connections {
        target: app
        onOpenApplicationInternal: Navigation.openApplication(app)
        onListMimeInternal: Navigation.openApplicationMime(mime)
        onListCategoryInternal: Navigation.openCategoryByName(name)
        onPreventedClose: closePreventedInfo.enabled = true
    }

    Menu {
        id: moreMenu
        MenuItem { action: ActionBridge { action: app.action("configure_sources"); } }
        MenuSeparator {}
        Menu {
            id: advancedMenu
            title: i18n("Advanced...")

            Instantiator {
                model: ResourcesModel.messageActions
                delegate: MenuItem { action: ActionBridge { action: modelData} }

                onObjectAdded: advancedMenu.insertItem(index, object)
                onObjectRemoved: advancedMenu.removeItem(object)
            }
        }
        MenuItem { action: ActionBridge { action: app.action("options_configure_keybinding"); } }
        MenuItem { action: ActionBridge { action: app.action("help_about_app"); } }
        MenuItem { action: ActionBridge { action: app.action("help_report_bug"); } }
    }

    condition: app.isCompact
    componentTrue: Main {
        id: main
        property alias stack: main.stack
        currentTopLevel: window.currentTopLevel

        Loader {
            anchors.fill: parent
            source: "qrc:/qml/DiscoverWindow_PlasmaPhone.qml"
        }
    }

    componentFalse: ColumnLayout {
        property alias stack: main.stack
        spacing: 0
        MuonToolbar {
            id: toolbar
            Layout.fillWidth: true
            visible: !app.isCompact
        }

        Main {
            id: main
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentTopLevel: window.currentTopLevel
        }
    }
}