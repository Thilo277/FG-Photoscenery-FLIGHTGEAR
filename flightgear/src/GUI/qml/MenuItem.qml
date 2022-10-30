import QtQuick 2.4
import "."

BaseMenuItem {
    id: root
    
    property alias text: itemText.text
    property string shortcut: "" // for compatability with Qt 5.6 and up

    signal triggered();

    function minWidth() { 
        return itemText.width + (Style.inset * 2)
    }

    Rectangle {
        height: parent.height
        width: parent.width
        visible: mouse.containsMouse
        color: "#cfcfcf"
    }

    Text {
        id: itemText
        font.pixelSize: Style.baseFontPixelSize
        color: mouse.containsMouse ? Style.themeColor :
            (root.enabled ? Style.baseTextColor : Style.disabledTextColor);

        anchors {
            left: parent.left
            leftMargin: Style.inset
            verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouse
        enabled: root.enabled
        anchors.fill: parent
        hoverEnabled: root.enabled
        onClicked: {
            root.closeMenu();
            root.triggered();
        }
    }
}
