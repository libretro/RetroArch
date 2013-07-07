import bb.cascades 1.0

Page
{
    titleBar: TitleBar {
        id: players
        kind: TitleBarKind.Segmented
        options: [
            Option {
                id: p1
                text: "Player 1"
                value: 0
                selected: true
            },
            Option {
                id: p2
                text: "Player 2"
                value: 1
            },
            Option {
                id: p3
                text: "Player 3"
                value: 2
            },
            Option {
                id: p4
                text: "Player 4"
                value: 3
            }
        ]
        
        onSelectedValueChanged: 
        {
            ButtonMap.refreshButtonMap(selectedValue)
        }
    }
    
    actions: [
        ActionItem {
            title: "Rescan"
            ActionBar.placement: ActionBarPlacement.OnBar
            imageSource: "asset:///images/search.png"
            onTriggered: {
                RetroArch.discoverController(players.selectedValue);
            }
        }
    ]
    
    Container
    {     
        Container
        {
            preferredWidth: 650
            horizontalAlignment: HorizontalAlignment.Center

            DropDown 
            {
                objectName: "dropdown_devices"
                title: "Device"
                
                onSelectedValueChanged:
                {
                    ButtonMap.mapDevice(selectedValue, players.selectedValue);
                }
            }

            ListView
            {
                id: buttonMapList
                objectName: "buttonMapList"

                listItemComponents: [
                    ListItemComponent
                    {
                        type: "item"
    
                        Container
                        {
                            id: itemRoot
                            horizontalAlignment: HorizontalAlignment.Center
                            rightPadding: 20

                            Divider {}
                            
                            Container
                            {
                                horizontalAlignment: HorizontalAlignment.Fill
                                topPadding: 10
                                bottomPadding: 10
                                

                                layout: DockLayout {
                                }

                                Label
                                {
                                    horizontalAlignment: HorizontalAlignment.Left
                                    verticalAlignment: VerticalAlignment.Center
                                    text: ListItemData.label
                                    textStyle
                                    {
                                        base: SystemDefaults.TextStyles.PrimaryText
                                    }
                                }
                                
                                Label
                                {
                                    horizontalAlignment: HorizontalAlignment.Right
                                    verticalAlignment: VerticalAlignment.Center
                                    text: ListItemData.button
                                    textStyle
                                    {
                                        base: SystemDefaults.TextStyles.PrimaryText
                                    }
                                }
                            }
                            
                            Divider {}
                        }
                    }
                ]

                onTriggered:
                {
                    var sym, data;
                    data = dataModel.data(indexPath);
                    sym = ButtonMap.mapButton(0, players.selectedValue, data["index"]);
                    data["button"] = ButtonMap.buttonToString(players.selectedValue, sym);
                    dataModel.replace(indexPath, data);
                }
    
                function itemType(data, indexPath)
                {
                    return "item";
                }
            }
        }
    }
}