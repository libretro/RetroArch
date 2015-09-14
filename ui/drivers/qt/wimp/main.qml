import QtQuick 2.2
import Material 0.1
import Material.ListItems 0.1 as ListItem

ApplicationWindow {
    id: mainWindow
    title: "RetroArch"
    width: 1280
    height: 720
    color: "#211822"
    visible: true
    theme {
        primaryColor: Palette.colors["blue"]["500"]
        primaryDarkColor: Palette.colors["blue"]["700"]
        accentColor: Palette.colors["red"]["A200"]
        tabHighlightColor: "white"
    }

    /* temporary top level folder list */
    property var folders: [
            "C:\\", "D:\\"
    ]

    property var sections: [ collections, cores, folders ]
    property var sectionTitles: [ "Collections", "Cores", "File Browser" ]

    property string selectedItem: collections[0]


    initialPage: Page {
        id: page

        title: "RetroArch"

        tabs: navDrawer.enabled ? [] : sectionTitles

        actionBar.maxActionCount: navDrawer.enabled ? 3 : 4

        actions: [
            Action {
                iconName: "action/search"
                name: "Search"
                enabled: true
            },

            Action {
                iconName: "image/color_lens"
                name: "Colors"
                onTriggered: colorPicker.show()
            },

            Action {
                iconName: "action/settings"
                name: "Settings"
                hoverAnimation: true
            },


            Action {
                iconName: "action/language"
                name: "Language"
                enabled: true
            },

            Action {
                iconName: "action/account_circle"
                name: "Accounts"
            }
        ]

        backAction: navDrawer.action

        NavigationDrawer {
            id: navDrawer
            enabled: page.width < Units.dp(500)


            Flickable {
                anchors.fill: parent

                contentHeight: Math.max(content.implicitHeight, height)

                Column {
                    id: content
                    anchors.fill: parent

                    Repeater {
                        model: sections

                        delegate: ListItem.Standard {
                            width: parent.width
                            ListItem.Subheader {
                                text: sectionTitles[index]
                            }
                        }
                    }
                }
            }
        }

        TabView {
            id: tabView
            anchors.fill: parent
            currentIndex: page.selectedTab
            model: sections


            delegate: Item {
                width: tabView.width
                height: tabView.height
                clip: true

                Sidebar {
                    id: sidebar

                    expanded: !navDrawer.enabled

                    Column {
                        width: parent.width

                        Repeater {
                            model: modelData
                            delegate: ListItem.Standard {
                                text: modelData
                                selected: modelData == selectedItem
                                onClicked: selectedItem = modelData
                            }
                        }
                    }
                }
                Flickable {
                    id: flickable
                    anchors {
                        left: sidebar.right
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                    }
                    clip: true
                    Column {
                        width: parent.width
                    }

                }
                Scrollbar {
                    flickableItem: flickable
                }
            }
        }
    }

    Dialog {
        id: colorPicker
        title: "Pick color"

        positiveButtonText: "Done"

        MenuField {
            id: selection
            model: ["Primary color", "Accent color", "Background color"]
            width: Units.dp(160)
        }

        Grid {
            columns: 7
            spacing: Units.dp(8)

            Repeater {
                model: [
                    "red", "pink", "purple", "deepPurple", "indigo",
                    "blue", "lightBlue", "cyan", "teal", "green",
                    "lightGreen", "lime", "yellow", "amber", "orange",
                    "deepOrange", "grey", "blueGrey", "brown", "black",
                    "white"
                ]

                Rectangle {
                    width: Units.dp(30)
                    height: Units.dp(30)
                    radius: Units.dp(2)
                    color: Palette.colors[modelData]["500"]
                    border.width: modelData === "white" ? Units.dp(2) : 0
                    border.color: Theme.alpha("#000", 0.26)

                    Ink {
                        anchors.fill: parent

                        onPressed: {
                            switch(selection.selectedIndex) {
                                case 0:
                                    theme.primaryColor = parent.color
                                    break;
                                case 1:
                                    theme.accentColor = parent.color
                                    break;
                                case 2:
                                    theme.backgroundColor = parent.color
                                    break;
                            }
                        }
                    }
                }
            }
        }

        onRejected: {
            // TODO set default colors again but we currently don't know what that is
        }
    }

}
