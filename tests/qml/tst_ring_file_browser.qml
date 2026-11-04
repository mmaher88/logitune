import QtQuick
import QtTest
import "../../src/app/qml/components/actionsring"

TestCase {
    name: "RingFileBrowser"
    when: windowShown

    Component { id: factory; RingFileBrowser {} }

    // The device folder ships PNG renders, so it always has images to list.
    readonly property string imageFolder: Qt.resolvedUrl("../../devices/mx-master-4").toString().replace(/^file:\/\//, "")

    function test_listsImagesAndChoosesOne() {
        let browser = createTemporaryObject(factory, this)
        browser.folderPath = imageFolder
        browser.open()
        tryVerify(() => browser.opened)

        // The grid must get real space and list the folder's images; a squeezed layout once left it
        // zero pixels wide.
        let grid = findChild(browser.contentItem, "fileGrid")
        verify(grid !== null)
        tryVerify(() => grid.count > 0)
        verify(grid.width > 300)

        let chosenPath = ""
        browser.chosen.connect(path => chosenPath = path)
        browser.selectedPath = imageFolder + "/back.png"
        browser.confirm()
        compare(chosenPath, imageFolder + "/back.png")
        verify(!browser.opened)
    }

    function test_folderModeChoosesTheShownFolder() {
        let browser = createTemporaryObject(factory, this)
        browser.mode = "folder"
        browser.folderPath = imageFolder
        browser.open()
        tryVerify(() => browser.opened)

        let chosenPath = ""
        browser.chosen.connect(path => chosenPath = path)
        browser.confirm()
        compare(chosenPath, imageFolder)
    }

    function test_goUpStopsAtRoot() {
        let browser = createTemporaryObject(factory, this)
        browser.folderPath = "/usr"
        browser.goUp()
        compare(browser.folderPath, "/")
        browser.goUp()
        compare(browser.folderPath, "/")
    }
}
