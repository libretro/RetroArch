#include <QString>

/* %1 is a placeholder for palette(highlight) or the equivalent chosen by the user */
static const QString qt_theme_default_stylesheet = QStringLiteral(R"(
   QPushButton[flat="true"] {
      min-height:20px;
      min-width:80px;
      padding:1px 3px 1px 3px;
      background-color: transparent;
      border: 1px solid #ddd;
   }
   ThumbnailWidget#thumbnailWidget, ThumbnailLabel#thumbnailGridLabel, QLabel#thumbnailQLabel {
      background-color:#d4d4d4;
   }
   QLabel#dropIndicator {
      font-size: 9pt;
      color: darkgrey;
      border: 2px dashed lightgrey;
      border-radius: 5px;
      margin: 20px;
   }
   ThumbnailWidget#thumbnailWidgetSelected {
      background-color:#d4d4d4;
      border:3px solid %1;
   }
   QFrame#playlistWidget, QFrame#browserWidget, QFrame#logWidget {
      padding: 8px;
   }
   QListWidget {
      icon-size: 32px;
   }
   /* color of the icons on the settings dialog */
   /* QLabel#iconColor {
      color: black;
   } */
)");

static const QString qt_theme_dark_stylesheet = QStringLiteral(R"(
   QWidget {
      color:white;
      background-color:rgb(53,53,53);
      selection-background-color:%1;
   }
   QWidget:disabled {
      color:rgb(127,127,127);
   }
   QFrame#playlistWidget, QFrame#browserWidget, QStackedWidget#centralWidget, QFrame#logWidget {
      padding: 8px;
      background-color:rgb(66,66,66);
      border-top:1px solid rgba(175,175,175,50%);
      border-left:1px solid rgba(125,125,125,50%);
      border-right:1px solid rgba(125,125,125,50%);
      border-bottom:1px solid rgba(25,25,25,75%);
   }
   QListWidget {
      icon-size: 32px;
   }
   QLabel#dropIndicator {
      font-size: 9pt;
      color: #575757;
      border: 2px dashed #575757;
      border-radius: 5px;
      margin: 20px;
   }
   QTextEdit, LogTextEdit {
      background-color:rgb(25,25,25);
   }
   QSpinBox, QDoubleSpinBox, QCheckBox, QRadioButton {
      background-color:rgb(25,25,25);
   }
   QCheckBox:checked, QCheckBox:unchecked, QRadioButton:checked, QRadioButton:unchecked {
      background-color:transparent;
   }
   /* Groupboxes for the settings window, can be restricted later with ViewOptionsDialog QGroupBox */
   QGroupBox {
      background-color:rgba(80,80,80,50%);
      margin-top:27px;
      border:1px solid rgba(25,25,25,127);
      border-top-left-radius:0px;
      border-top-right-radius:4px;
   }
   QGroupBox::title {
      min-height:28px;
      subcontrol-origin:margin;
      subcontrol-position:left top;
      padding:4px 6px 5px 6px;
      margin-left:0px;
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgb(65,65,65),stop: 0.4 rgb(70,70,70),stop:1 rgb(90,90,90));
      border:1px solid rgba(25,25,25,127);
      border-bottom:1px solid rgb(65,65,65);
      border-top-left-radius:4px;
      border-top-right-radius:4px;
   }
   QGroupBox::indicator:checked {
      background-color:%1;
      border:4px solid rgb(45,45,45);
   }
   QGroupBox::indicator:unchecked {
      background-color:rgba(25,25,25,50%);
   }
   QGroupBox::indicator {
      width:16px;
      height:16px;
   }
   QWidget#shaderParamsWidget {
      background-color:rgb(25,25,25);
   }
   QDialog#shaderParamsDialog QGroupBox {
      background-color:rgb(53,53,53);
      border-top-left-radius:0px;
   }
   QDialog#shaderParamsDialog QGroupBox::title {
      margin-left:0px;
      min-height:28px;
      padding:4px 10px;
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgb(53,53,53),stop:1 rgba(125,125,125,127));
      border:1px solid rgba(25,25,25,75);
      border-top:1px solid rgba(175,175,175,50%);
      border-bottom:none transparent;
   }
   QToolTip {
      color:white;
      background-color:rgb(53,53,53);
      border:1px solid rgb(80,80,80);
      border-radius:4px;
   }
   QMenuBar {
      background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-bottom:2px solid rgba(25,25,25,75);
   }
   QMenuBar::item {
      spacing:2px;
      padding:3px 4px;
      background-color:transparent;
   }
   QMenuBar::item:selected {
      background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(106,106,106,255),stop:1 rgba(106,106,106,75));
      border:1px solid %1;
   }
   QMenuBar::item:pressed {
      background-color:%1;
      border-left:1px solid rgba(25,25,25,127);
      border-right:1px solid rgba(25,25,25,127);
   }
   QMenu {
      background-color:rgb(45,45,45);
      border:1px solid palette(shadow);
   }
   QMenu::item {
      padding:3px 25px 3px 25px;
      border:1px solid transparent;
   }
   QMenu::item:disabled {
      color:rgb(127,127,127);
   }
   QMenu::item:selected {
      border-color:rgba(200,200,200,127);
      background-color:%1;
   }
   QMenu::icon:checked {
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border:1px solid %1;
      border-radius:2px;
   }
   QMenu::separator {
      height:1px;
      background-color:rgb(100,100,100);
      margin-left:5px;
      margin-right:5px;
   }
   QMenu::indicator {
      width:18px;
      height:18px;
   }
   QToolBar::top {
      background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-bottom:3px solid qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QToolBar::bottom {
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-top:3px solid qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QToolBar::left {
      background-color:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-right:3px solid qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QToolBar::right {
      background-color:qlineargradient(x1:1,y1:0,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-left:3px solid qlineargradient(x1:1,y1:0,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QMainWindow {
      background-color:rgb(53,53,53);
   }
   QMainWindow::separator {
      width:6px;
      height:5px;
      padding:2px;
      background-color:rgba(25,25,25,50%);
   }
   QLineEdit {
      color:white;
      background-color:rgb(25,25,25);
   }
   QLineEdit::focus {
      border:1px solid %1;
      border-radius:3px;
      color:white;
      background-color:rgb(25,25,25);
   }
   QSplitter::handle:horizontal {
      width:10px;
   }
   QSplitter::handle:vertical {
      height:10px;
   }
   QMainWindow::separator:hover, QSplitter::handle:hover {
   }
   QDockWidget {
      font-family:"Segoe UI";
      font-size:9pt;
   }
   QDockWidget::title {
      padding:3px 4px;
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,175),stop:1 rgba(53,53,53,75));
      border:1px solid rgba(25,25,25,75);
      border-top:1px solid rgba(175,175,175,50%);
      border-bottom:1px solid rgba(25,25,25,127);
   }
   QDockWidget::close-button, QDockWidget::float-button {
      subcontrol-position:top right;
      subcontrol-origin:margin;
      position:absolute;
      top:3px;
      bottom:0px;
      width:20px;
      height:20px;
   }
   QDockWidget::close-button:hover, QDockWidget::float-button:hover {
      border:1px solid %1;
      border-radius:4px;
   }
   QDockWidget::close-button {
      right:3px;
   }
   QDockWidget::float-button {
      right:25px;
   }
   QTabWidget::pane {
      background-color:rgba(66,66,66,50%);
   }
   QTabWidget::tab-bar {
   }
   QTabBar {
      background-color:transparent;
      qproperty-drawBase:0;
      border-bottom:1px solid rgba(25,25,25,50%);
   }
   QTabBar::tab {
      padding:4px 6px;
      background-color:rgba(25,25,25,127);
      border:1px solid rgba(25,25,25,75);
   }
   QTabBar::tab:selected {
      background-color:rgb(66,66,66);
      border-bottom-color:rgba(66,66,66,75%);
   }
   QTabBar::tab:!selected {
      color:rgb(175,175,175);
   }
   QComboBox {
      min-height:20px;
      padding:1px 6px 1px 6px;
   }
   QComboBox::focus {
      background:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,50), stop: 1 rgba(100,100,100,25));
      border:1px solid %1;
      border-radius:4px;
   }
   QComboBox::hover {
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,50), stop: 1 rgba(127,127,127,50));
      border:1px solid %1;
      border-radius:4px;
   }
   QComboBox::drop-down {
      background-color:transparent;
      width:0px;
   }
   QComboBox::selected:on, QComboBox::selected:off {
      background-color:%1;
   }
   QTabBar::tab:hover {
      color:white;
      background-color:%1;
   }
   QComboBox::separator {
      background-color:rgb(100,100,100);
      height:1px;
      margin-left:4px;
      margin-right:4px;
   }
   QCheckBox::indicator {
      width:18px;
      height:18px;
   }
   QPushButton {
      min-height:20px;
      min-width:80px;
      padding:1px 3px 1px 3px;
      outline:none;
   }
   QPushButton::disabled, QToolButton::disabled {
      color:grey;
      background-color:rgb(25,25,25);
   }
   QPushButton::focus, QToolButton::focus {
      background:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,50), stop: 1 rgba(100,100,100,25));
      border:1px solid %1;
      border-radius:4px;
   }
   QPushButton::hover, QToolButton::hover {
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,75), stop: 1 rgba(100,100,100,50));
      border:1px solid %1;
      border-radius:4px;
   }
   QPushButton::pressed, QToolButton::pressed {
      background-color:transparent;
      border:1px solid %1;
      border-radius:4px;
   }
   QPushButton[flat="true"] {
      background-color:transparent;
   }
   QPushButton[flat="true"]::menu-indicator {
      position:relative;
      bottom:4px;
      right:4px;
   }
   QRadioButton::indicator {
      width:18px;
      height:18px;
   }
   QListWidget::item:selected, QTreeView::item:selected, QTableView::item:selected {
      color:white;
      background-color:%1;
   }
   QTreeView {
      background-color:rgb(25,25,25);
      selection-background-color:%1;
   }
   QTreeView::branch:selected {
      background-color:%1;
   }
   QTreeView::item:selected:disabled, QTableView::item:selected:disabled {
      background-color:rgb(80,80,80);
   }
   QTreeView::branch:open, QTreeView::branch:closed {
      background-color:solid;
   }
   QTableView, QListWidget {
      background-color:rgb(25,25,25);
   }
   QTreeView QHeaderView::section, QTableView QHeaderView::section {
      /*height:24px;*/
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-style:none;
      border-bottom:1px solid rgb(65,65,65);
      padding-left:5px;
      padding-right:5px;
   }
   QTableView {
      background-color:rgb(25,25,25);
      alternate-background-color:rgb(40,40,40);
   }
   QScrollBar:vertical, QScrollBar:horizontal {
      background-color:rgb(35,35,35);
   }
   QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
      background-color:rgb(65,65,65);
      border-right:1px solid rgba(175,175,175,50%);
      border-top:1px solid rgba(175,175,175,50%);
      border-bottom:1px solid rgba(25,25,25,75);
      border-radius:2px;
   }
   QScrollBar::handle:horizontal:hover, QScrollBar::handle:vertical:hover {
      border:1px solid %1;
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,75), stop: 1 rgba(127,127,127,75));
   }
   QScrollBar:vertical {
      border-top-right-radius:2px;
      border-bottom-right-radius:2px;
      width:16px;
      margin:0px;
   }
   QScrollBar::handle:vertical {
      min-height:20px;
      margin:2px 4px 2px 4px;
   }
   QScrollBar::add-line:vertical {
      background:none;
      height:0px;
      subcontrol-position:right;
      subcontrol-origin:margin;
   }
   QScrollBar::sub-line:vertical {
      background:none;
      height:0px;
      subcontrol-position:left;
      subcontrol-origin:margin;
   }
   QScrollBar:horizontal {
      height:16px;
      margin:0px;
   }
   QScrollBar::handle:horizontal {
      min-width:20px;
      margin:4px 2px 4px 2px;
   }
   QScrollBar::add-line:horizontal {
      background:none;
      width:0px;
      subcontrol-position:bottom;
      subcontrol-origin:margin;
   }
   QScrollBar::sub-line:horizontal {
      background:none;
      width:0px;
      subcontrol-position:top;
      subcontrol-origin:margin;
   }
   QSlider {
      background:transparent;
   }
   QSlider::sub-page {
      background:%1;
   }
   QSlider::groove:vertical {
      width:3px;
      background:rgb(25,25,25);
   }
   QSlider::handle:vertical {
      background:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgb(175,175,175), stop: 1 rgb(75,75,75));
      border:1px solid rgb(35,35,35);
      border-radius:2px;
      height:16px;
      margin:0 -4px;
   }
   QSlider::handle:vertical:hover {
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgb(200,200,200), stop: 1 rgba(100,100,100));
      border:1px solid %1;
      border-radius:2px;
      height:16px;
      margin:0 -4px;
   }
   QSlider::groove:horizontal {
      height:3px;
      background:rgb(25,25,25);
   }
   QSlider::handle:horizontal {
      background:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgb(175,175,175), stop: 1 rgb(75,75,75));
      border:1px solid rgb(35,35,35);
      border-radius:2px;
      width:16px;
      margin:-4px 0;
   }
   QSlider::handle:horizontal:hover {
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgb(200,200,200), stop: 1 rgba(100,100,100));
      border:1px solid %1;
      border-radius:2px;
      width:16px;
      margin:-4px 0;
   }
   QStatusBar {
      color:white;
      background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QStatusBar QLabel {
      background-color:transparent;
   }
   QLabel {
      background-color:transparent;
   }
   QSizeGrip {
      background-color:solid;
   }
   GridView::item {
      background-color:rgb(40,40,40);
   }
   GridView::item:selected {
      border:3px solid %1;
   }
   GridView {
      background-color:rgb(25,25,25);
      selection-color: white;
      qproperty-layout: "fixed";
   }
   GridItem {
      qproperty-thumbnailvalign: "center";
   }
   QLabel#itemsCountLabel {
      padding-left: 5px;
   }
)");
