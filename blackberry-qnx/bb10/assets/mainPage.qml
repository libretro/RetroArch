import bb.cascades 1.0
import bb.cascades.pickers 1.0

TabbedPane {
    Tab {
        Page {
            actions: [
                ActionItem {
                    title: "Play"
                    ActionBar.placement: ActionBarPlacement.OnBar
                    imageSource: "asset:///images/open.png"
                    onTriggered: {
                        if(RetroArch.rom == "" || RetroArch.core == ""){
                            //Do something to focus on select rom box
                        } else {
                            OrientationSupport.supportedDisplayOrientation = 
                                SupportedDisplayOrientation.DisplayLandscape;
                        }
                    }
                }
            ]  
            
            titleBar: TitleBar {
                id: titleBar
                title: "RetroArch"
            }
            
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                rightPadding: 20
                leftPadding: 20
                
                layout: DockLayout {}
                
                Container {
                    preferredWidth: 680
                    horizontalAlignment: HorizontalAlignment.Center
                    verticalAlignment: VerticalAlignment.Center
                    
                    ImageView 
                    {
                        horizontalAlignment: HorizontalAlignment.Center
                        imageSource: "asset:///images/icon.png"
                        preferredWidth: 200
                        preferredHeight: 200
                    }
  
                    DropDown 
                    {
                        id: _core
                        objectName: "dropdown_core"
                        horizontalAlignment: HorizontalAlignment.Center
                        title: "Core Selection"
                    }
                    
                    Container {
                        horizontalAlignment: HorizontalAlignment.Center
                        preferredWidth: 680
                        
                        layout: StackLayout {
                            orientation: LayoutOrientation.LeftToRight
                        }
                        
                        DropDown
                        {
                            id: romName
                            verticalAlignment: VerticalAlignment.Center
                            horizontalAlignment: HorizontalAlignment.Center
                            preferredWidth: 600
                            enabled: false
                            //hintText: "Select a ROM"
                            title: if(picker.selectedFile)
                                       picker.selectedFile
                                   else
                                       "Rom Selection"
                        }
                        
                        ImageButton {
                            horizontalAlignment: HorizontalAlignment.Right
                            defaultImageSource: "asset:///images/file.png"
                            onClicked: {
                                picker.open();
                            }
                        }
                    }
                }
            }
            attachedObjects: [
                FilePicker {
                    id: picker
        
                    property string selectedFile
        
                    title: "Rom Selector"
                    filter: []
                    type: FileType.Other
                    directories: ["/accounts/1000/shared/documents/roms"]
        
                    onFileSelected: {
                        RetroArch.rom = selectedFiles[0];
                        selectedFile = RetroArch.rom.substr(RetroArch.rom.lastIndexOf('/')+1);
                        picker.directories = [RetroArch.rom.substr(0, RetroArch.rom.lastIndexOf('/'))];
                    }
                }
            ]
        }
    }
    Tab {
        Page {
            Container {

            }
        }
    }
}
