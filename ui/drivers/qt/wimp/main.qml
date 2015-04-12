import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1


Window {
    id: mainWindow
    title: "RetroArch"
    width: 1280
    height: 720
    color: "#211822"
    visible: true

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Rectangle {
            id: consoleBar;
            color: "#2b2b2b";
            height: 500;
            width: 250;
            Layout.maximumWidth: 400

            Rectangle {
                id: rightBord;
                anchors {
                    top: parent.top;
                    bottom: parent.bottom;
                    right: parent.right;
                }
                width: 1;
                color: "#1a1a1a";
            }

            Row {
                id: leftBord;
                anchors {
                    left: parent.left;
                    top: parent.top;
                    bottom: parent.bottom;
                }
            }


            Rectangle {
                id: consoleHeader;
                height: 36;
                color: parent.color;

                anchors {
                    top: parent.top;
                    //topMargin: 12;
                    left: parent.left;
                    right: parent.right;
                    rightMargin: 1;
                }
            }

            MouseArea {
                id: mouse;
                anchors.fill: parent;
                onClicked: {
                    if (listView.retractList)
                        listView.retractList = false;
                    else
                        listView.retractList = true;
                }
            }
            Row {
                anchors {
                    left: parent.left;
                    top: parent.top;
                    topMargin: 12;
                    leftMargin: 12;
                    horizontalCenter: parent.horizontalCenter;
                }
                Text {
                    renderType: Text.QtRendering;
                    text: "Platforms";
                    color: "#f1f1f1";
                    font {
                        bold: true;
                        family: "Sans";
                        pixelSize: 12;
                    }
                }
            }
            ListModel {
                id: platformsModel
                ListElement { name: "Megadrive" }
                ListElement { name: "Nintendo" }
                ListElement { name: "Playstation" }
            }
            ListView {
                id: listView;
                visible: (height !== 0);
                anchors {
                    top: consoleHeader.bottom;
                    //bottom: parent.bottom;
                    right: parent.right;
                    left: parent.left;
                    topMargin: 0;
                }

                height: retractList ? 0 : 500;

                Behavior on height {
                    PropertyAnimation {}
                }

                snapMode: ListView.SnapToItem;
                orientation: ListView.Vertical;
                interactive: true;
                highlightFollowsCurrentItem: false;

                property bool retractList: false;

                highlight: Item {
                    id: highlightItem;
                    visible: !listView.retractList;
                    height: listView.currentItem.height;
                    width: listView.width;
                    anchors.verticalCenter: listView.currentItem.verticalCenter;
                    y: listView.currentItem.y;
                    Item {
                        id: innerItem;
                        height: parent.height;
                        width: parent.width;




                        Rectangle {
                            id: mainColor;
                            anchors {
                                left: parent.left;
                                right: parent.right;
                                top: parent.top;
                                bottom: parent.bottom;
                            }
                            color: listView.currentItem ? "#171717" : "#000000FF";
                            Rectangle {
                                id: topBorder;
                                color: "#f27b77";
                                anchors {
                                    left: parent.left;
                                    leftMargin: leftBord.width;
                                    top: parent.top;
                                }
                                height: 2;
                                width: 4;
                            }

                            Row {
                                // leftAccent;
                                anchors {
                                    left: parent.left;
                                    leftMargin: leftBord.width;
                                    bottom: bottomB.top;
                                    top: topBorder.bottom;
                                }

                                Rectangle {
                                    anchors {
                                        top: parent.top;
                                        bottom: parent.bottom;
                                    }
                                    width: 1;
                                    color: "#db5753";
                                }

                                Rectangle {
                                    anchors {
                                        top: parent.top;
                                        bottom: parent.bottom;
                                    }
                                    width: 3;
                                    color: "#e8433f";
                                }

                            }

                            Column {
                                id: bottomB;
                                anchors {
                                    right: parent.right;
                                    rightMargin: rightBord.width;
                                    left: parent.left;
                                    leftMargin: leftBord.width;
                                    bottom: parent.bottom;
                                }

                                Rectangle {
                                    color: "#a22f2c";
                                    anchors {
                                        left: parent.left;
                                    }
                                    width: 4;
                                    height: 2;
                                }
                                Rectangle {
                                    color: "#474747";
                                    anchors {
                                        left: parent.left;
                                        right: parent.right;
                                    }
                                    height: 1;
                                }
                            }
                        }


                    }
                }

                anchors {
                    right: parent.right;
                    left: parent.left;
                    top: consoleLabel.bottom;
                    topMargin: 10;
                }

                model: platformsModel;

                ExclusiveGroup {
                    id: consoleGroup
                }

                delegate: Item {
                    //visible: !listView.retractList;
                    height: 22;
                    width: consoleBar.width;
                    Row {
                        id: row;
                        anchors {
                            fill: parent;
                            leftMargin: 25;
                        }
                        spacing: 7;

                        Image {
                            anchors.verticalCenter: parent.verticalCenter;
                            fillMode: Image.PreserveAspectFit;
                            sourceSize {
                                height: 24;
                                width: 24;
                            }
                            height: 20;
                            width: 20;
                        }

                        Text {
                            id: consoleItem;
                            anchors.verticalCenter: parent.verticalCenter;
                            width: 140;
                            text: modelData;
                            color: "#f1f1f1";
                            renderType: Text.QtRendering;
                            elide: Text.ElideRight;
                            font {
                                family: "Sans";
                                pixelSize: 11;
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent;
                        onClicked: {
                            listView.currentIndex = index;

                        }
                    }
                }
            }


        }

        Rectangle {
            id: centerItem
            Layout.minimumWidth: 50
            Layout.fillWidth: true
            color: "lightgray"
            Text {
                text: "Content View"
                anchors.centerIn: parent
            }

        }

    }






}
