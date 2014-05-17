import bb.cascades 1.0
import bb.cascades.pickers 1.0

Page {
    actions: [
        ActionItem {
            title: "Play"
            ActionBar.placement: ActionBarPlacement.OnBar
            imageSource: "asset:///images/open.png"
            onTriggered: {
                if(RetroArch.content == "" || RetroArch.core == "")
                {
                    //Do something to focus on select content box
                }
                else 
                    RetroArch.start();
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
                //Handled in C++
            }

            Container {
                horizontalAlignment: HorizontalAlignment.Center
                preferredWidth: 680

                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }

                //I like the look as a textbox
                DropDown
                {
                    id: contentName
                    verticalAlignment: VerticalAlignment.Center
                    horizontalAlignment: HorizontalAlignment.Center
                    preferredWidth: 600
                    enabled: false
                    title: if(picker.selectedFile)
                               picker.selectedFile
                           else
                               "Content Selection"
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

            title: "Content Selector"
            filter: { RetroArch.contentExtensions.split("|") }
            type: FileType.Other
            directories: ["/accounts/1000/shared/documents/content"]

            onFileSelected: {
                RetroArch.content = selectedFiles[0];
                selectedFile = RetroArch.content.substr(RetroArch.content.lastIndexOf('/')+1);
                picker.directories = [RetroArch.content.substr(0, RetroArch.content.lastIndexOf('/'))];
            }
        }
    ]
}
