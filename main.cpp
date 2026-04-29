#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QInputDialog>
#include <QDialog>
#include <QLabel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSortFilterProxyModel>
#include <QDirIterator>
#include <QFile>
#include <QHeaderView>
#include <QTextBlock>
#include <QTextCursor>
#include <QPainter>
#include <QProcess>
#include <QCloseEvent>
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <QMouseEvent>
#include <QFrame>
#include <QWindow>
#include <QTime>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <iostream>
#include <set>
#include <map>
#include <cmath>

const QString BG_MAIN = "#1e1e1e";
const QString BG_SIDEBAR = "#252526";
const QString BORDER_COLOR = "#333333";
const QString ACCENT_BLUE = "#007acc";

class JavaProjectProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    JavaProjectProxy(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        if (sourceModel()->data(index).toString().contains(filterRegularExpression().pattern(), Qt::CaseInsensitive)) return true;
        for (int i = 0; i < sourceModel()->rowCount(index); ++i) {
            if (filterAcceptsRow(i, index)) return true;
        }
        return false;
    }
};

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    CodeEditor(QWidget *parent = nullptr) : QPlainTextEdit(parent) {
        lineNumberArea = new LineNumberArea(this);
        connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
        connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
        connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
        updateLineNumberAreaWidth(0);
        setReadOnly(true);
        setLineWrapMode(QPlainTextEdit::NoWrap);
        setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: 'JetBrains Mono', 'Monospace'; font-size: 12pt; border: none;");
    }
    void lineNumberAreaPaintEvent(QPaintEvent *event) {
        QPainter painter(lineNumberArea);
        painter.fillRect(event->rect(), QColor("#1e1e1e"));
        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());
        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                QString number = QString::number(blockNumber + 1);
                painter.setPen(QColor("#858585"));
                painter.drawText(0, top, lineNumberArea->width() - 5, fontMetrics().height(), Qt::AlignRight, number);
            }
            block = block.next(); top = bottom; bottom = top + qRound(blockBoundingRect(block).height()); ++blockNumber;
        }
    }
    int lineNumberAreaWidth() {
        int digits = 1; int max = qMax(1, blockCount());
        while (max >= 10) { max /= 10; digits++; }
        return 20 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        QPlainTextEdit::resizeEvent(event);
        lineNumberArea->setGeometry(QRect(contentsRect().left(), contentsRect().top(), lineNumberAreaWidth(), contentsRect().bottom()));
    }
public slots: void highlightCurrentLine() { emit requestHighlightSync(); }
signals: void requestHighlightSync();
private slots:
    void updateLineNumberAreaWidth(int) { setViewportMargins(lineNumberAreaWidth(), 0, 0, 0); }
    void updateLineNumberArea(const QRect &rect, int dy) {
        if (dy) lineNumberArea->scroll(0, dy);
        else lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    }
private:
    class LineNumberArea : public QWidget {
    public:
        LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}
        QSize sizeHint() const override { return QSize(codeEditor->lineNumberAreaWidth(), 0); }
    protected:
        void paintEvent(QPaintEvent *event) override { codeEditor->lineNumberAreaPaintEvent(event); }
    private: CodeEditor *codeEditor;
    };
    QWidget *lineNumberArea;
};

class JavaMonitorGUI : public QMainWindow {
    Q_OBJECT
public:
    JavaMonitorGUI(QStringList paths) {
        projectPaths = paths;
        currentGroupName = "Default";
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
        resize(1300, 850);
        setWindowTitle("Hawk Java Debugger");
        setStyleSheet(QString("QMainWindow { background-color: %1; color: #ccc; }").arg(BG_MAIN));

        customTitleBar = new QFrame();
        customTitleBar->setFixedHeight(32);
        customTitleBar->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;").arg(BG_SIDEBAR).arg(BORDER_COLOR));
        titleLabel = new QLabel("  🦅  <b>HAWK PRO</b> [STABLE]");
        titleLabel->setStyleSheet("color: #999; font-weight: bold; font-size: 11px;");

        QPushButton *btnMin = new QPushButton("—");
        QPushButton *btnMax = new QPushButton("⬜");
        QPushButton *btnClose = new QPushButton("✕");
        QString tBtnStyle = "QPushButton { border:none; color: white; width: 45px; height: 32px; } QPushButton:hover { background-color: #444; }";
        btnMin->setStyleSheet(tBtnStyle); btnMax->setStyleSheet(tBtnStyle);
        btnClose->setStyleSheet("QPushButton { border:none; color: white; width: 45px; height: 32px; } QPushButton:hover { background-color: #e81123; }");

        QHBoxLayout *titleLayout = new QHBoxLayout(customTitleBar);
        titleLayout->setContentsMargins(10, 0, 0, 0); titleLayout->setSpacing(0);
        titleLayout->addWidget(titleLabel); titleLayout->addStretch();
        titleLayout->addWidget(btnMin); titleLayout->addWidget(btnMax); titleLayout->addWidget(btnClose);

        customTitleBar->installEventFilter(this);
        titleLabel->installEventFilter(this);

        treeModel = new QStandardItemModel(this);
        for (const QString &path : projectPaths) {
            QString abs = QDir(path).absolutePath();
            QStandardItem *rootItem = new QStandardItem(QFileInfo(abs).fileName());
            rootItem->setData(abs, Qt::UserRole);
            treeModel->invisibleRootItem()->appendRow(rootItem);
            buildTree(abs, rootItem);
        }
        proxyModel = new JavaProjectProxy(this);
        proxyModel->setSourceModel(treeModel);

        searchBar = new QLineEdit();
        searchBar->setPlaceholderText(" 🔍 Teleport search...");
        searchBar->setStyleSheet("background: #252526; border: none; border-bottom: 1px solid #333; color: #ccc; padding: 8px;");

        tree = new QTreeView();
        tree->setModel(proxyModel);
        tree->setHeaderHidden(true);
        tree->setStyleSheet(QString("QTreeView { background: %1; border: none; color: #bbb; outline: none; } QTreeView::item:selected { background: %2; color: white; }").arg(BG_SIDEBAR).arg(ACCENT_BLUE));
        
        viewer = new CodeEditor();
        editorSearch = new QLineEdit();
        editorSearch->setPlaceholderText(" 🔍 Search in current file...");
        editorSearch->setStyleSheet("background: #252526; border: none; border-bottom: 1px solid #333; color: #ccc; padding: 8px;");

        auto createStyledBtn = [&](QString txt, QString bgCol) {
            QPushButton *b = new QPushButton(txt);
            b->setFixedSize(75, 26);
            b->setStyleSheet(QString("QPushButton { background: %1; border: 1px solid #444; border-radius: 2px; color: white; font-weight: bold; font-size: 10px; } QPushButton:hover { background: #555; }").arg(bgCol));
            return b;
        };

        QPushButton *btnBP = createStyledBtn("🔴 BP", "#7a1a1a");
        QPushButton *btnNext = createStyledBtn("NEXT", "#1a4a7a");
        QPushButton *btnStop = createStyledBtn("RESUME", "#1a7a1a");
        QPushButton *btnStep = createStyledBtn("STEP", "#1a5a5a");
        QPushButton *btnOut = createStyledBtn("OUT", "#5a1a7a");
        QPushButton *btnLocals = createStyledBtn("LOCALS", "#4a4a1a");
        QPushButton *btnStack = createStyledBtn("STACK", "#1a4a4a");
        QPushButton *btnThreads = createStyledBtn("THREADS", "#4a1a4a");
        QPushButton *btnWhere = createStyledBtn("WHERE", "#2a2a5a");
        QPushButton *btnExit = createStyledBtn("EXIT", "#3a3a3a");
        QPushButton *btnPrint = createStyledBtn("PRINT", "#2a5a2a");
        QPushButton *btnPrintD = createStyledBtn("DUMP", "#2a5a2a");
        btnHealth = createStyledBtn("EYE: ON", "#2a5a5a");
        btnHealth->setFixedWidth(100);
        
        groupCombo = new QComboBox();
        groupCombo->addItem("Default");
        groupCombo->setFixedSize(140, 26);
        groupCombo->setItemDelegate(new QStyledItemDelegate(groupCombo));
        groupCombo->setStyleSheet("QComboBox { background: #333; border: 1px solid #555; color: #fff; padding-left: 5px; font-size: 11px; } QComboBox QAbstractItemView { background-color: #252526 !important; color: #ffffff !important; selection-background-color: #007acc; border: 1px solid #333; }");

        btnSaveGroup = createStyledBtn("💾 SAVE", "#444");
        btnSaveGroup->setFixedWidth(60);

        QSplitter *split = new QSplitter(Qt::Horizontal);
        split->setStyleSheet("QSplitter::handle { background-color: #333; }");
        QWidget *left = new QWidget();
        QVBoxLayout *lbox = new QVBoxLayout(left);
        lbox->setContentsMargins(0,0,0,0); lbox->setSpacing(0);
        lbox->addWidget(searchBar); lbox->addWidget(tree);

        QWidget *right = new QWidget();
        QVBoxLayout *rbox = new QVBoxLayout(right);
        rbox->setContentsMargins(0,0,0,0); rbox->setSpacing(0);
        QHBoxLayout *btnBar = new QHBoxLayout();
        btnBar->setContentsMargins(5,5,5,5); btnBar->setSpacing(4);
        btnBar->addWidget(btnBP); btnBar->addWidget(btnNext); btnBar->addWidget(btnStop);
        btnBar->addWidget(btnStep); btnBar->addWidget(btnOut); btnBar->addWidget(btnLocals);
        btnBar->addWidget(btnStack); btnBar->addWidget(btnThreads); btnBar->addWidget(btnWhere);
        btnBar->addWidget(btnExit); btnBar->addWidget(btnPrint); btnBar->addWidget(btnPrintD);
        btnBar->addWidget(btnHealth); btnBar->addWidget(new QLabel(" PHASE:")); btnBar->addWidget(groupCombo);
        btnBar->addWidget(btnSaveGroup);

        jdbOutput = new QTextEdit();
        jdbOutput->setReadOnly(true);
        jdbOutput->setMaximumHeight(150);
        jdbOutput->setStyleSheet("background-color: #000; color: #00ff99; font-family: 'Monospace'; font-size: 10pt; border: none; border-top: 1px solid #333;");

        rbox->addWidget(editorSearch); rbox->addWidget(viewer); rbox->addWidget(jdbOutput); rbox->addLayout(btnBar);
        split->addWidget(left); split->addWidget(right); split->setStretchFactor(1, 4);

        QWidget *central = new QWidget();
        QVBoxLayout *mainLayout = new QVBoxLayout(central);
        mainLayout->setContentsMargins(0, 0, 0, 0); mainLayout->setSpacing(0);
        mainLayout->addWidget(customTitleBar); mainLayout->addWidget(split);
        setCentralWidget(central);

        connect(btnMin, &QPushButton::clicked, this, &JavaMonitorGUI::showMinimized);
        connect(btnMax, &QPushButton::clicked, this, [this]() { if (isMaximized()) showNormal(); else showMaximized(); });
        connect(btnClose, &QPushButton::clicked, this, &JavaMonitorGUI::close);
        connect(searchBar, &QLineEdit::textChanged, this, [this](const QString &t){ proxyModel->setFilterRegularExpression(QRegularExpression(t, QRegularExpression::CaseInsensitiveOption)); if(!t.isEmpty()) tree->expandAll(); else tree->collapseAll(); });
        connect(editorSearch, &QLineEdit::textChanged, this, &JavaMonitorGUI::onEditorSearch);
        connect(tree, &QTreeView::clicked, this, &JavaMonitorGUI::onFileClicked);
        connect(btnBP, &QPushButton::clicked, this, &JavaMonitorGUI::onToggleBreakpoint);
        connect(btnNext, &QPushButton::clicked, this, [this](){ jdbProcess->write("next\n"); });
        connect(btnStop, &QPushButton::clicked, this, [this](){ jdbProcess->write("cont\n"); hitLine = -1; applyVisualBreakpoints(); });
        connect(btnStep, &QPushButton::clicked, this, [this](){ jdbProcess->write("step\n"); });
        connect(btnOut, &QPushButton::clicked, this, [this](){ jdbProcess->write("step up\n"); });
        connect(btnLocals, &QPushButton::clicked, this, [this](){ jdbProcess->write("locals\n"); });
        connect(btnStack, &QPushButton::clicked, this, [this](){ jdbProcess->write("where\n"); });
        connect(btnThreads, &QPushButton::clicked, this, [this](){ jdbProcess->write("threads\n"); });
        connect(btnWhere, &QPushButton::clicked, this, [this](){ jdbProcess->write("where all\n"); });
        connect(btnExit, &QPushButton::clicked, this, [this](){ jdbProcess->write("exit\n"); });
        connect(btnPrint, &QPushButton::clicked, this, [this](){ bool ok; QString v = QInputDialog::getText(this, "Print", "Var:", QLineEdit::Normal, "", &ok); if(ok) jdbProcess->write(QString("print %1\n").arg(v).toUtf8()); });
        connect(btnPrintD, &QPushButton::clicked, this, [this](){ bool ok; QString v = QInputDialog::getText(this, "Dump", "Var:", QLineEdit::Normal, "", &ok); if(ok) jdbProcess->write(QString("dump %1\n").arg(v).toUtf8()); });
        connect(btnHealth, &QPushButton::clicked, this, &JavaMonitorGUI::onToggleHealth);
        connect(btnSaveGroup, &QPushButton::clicked, this, &JavaMonitorGUI::onSaveClicked);
        connect(groupCombo, &QComboBox::currentTextChanged, this, &JavaMonitorGUI::onGroupChanged);
        connect(viewer, &CodeEditor::requestHighlightSync, this, &JavaMonitorGUI::applyVisualBreakpoints);

        healthTimer = new QTimer(this);
        connect(healthTimer, &QTimer::timeout, this, &JavaMonitorGUI::runHealthPulse);
        healthTimer->start(50);
        loadGroupsFromFile();
        startProcesses();
    }

    ~JavaMonitorGUI() {
        if (jdbProcess && jdbProcess->state() == QProcess::Running) { jdbProcess->write("exit\n"); jdbProcess->waitForFinished(500); }
        if (javaProcess && javaProcess->state() == QProcess::Running) { javaProcess->terminate(); javaProcess->waitForFinished(500); }
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj == customTitleBar || obj == titleLabel) {
            if (event->type() == QEvent::MouseButtonDblClick) { if (isMaximized()) showNormal(); else showMaximized(); return true; }
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *m = static_cast<QMouseEvent*>(event);
                if (m->button() == Qt::LeftButton && windowHandle()) { windowHandle()->startSystemMove(); return true; }
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

private slots:
    void onFileClicked(const QModelIndex &i) { openFile(i.data(Qt::UserRole).toString()); }

    void handleJdbOutput() {
        QString o = jdbProcess->readAllStandardOutput(); jdbBuffer += o; jdbOutput->append(o.trimmed());
        static QRegularExpression re("(?:Breakpoint hit|Step completed):.*thread=.*?,\\s+(\\S+)\\.\\w+\\(.*\\),\\s+line=([\\d,]+)");
        QRegularExpressionMatch m = re.match(jdbBuffer);
        if (m.hasMatch()) {
            QString className = m.captured(1); hitLine = m.captured(2).remove(',').toInt();
            hitFilePath = findPathFromClass(className); jdbBuffer.clear();
            if (!hitFilePath.isEmpty()) {
                QTimer::singleShot(0, this, [this](){
                    openFile(hitFilePath);
                    QTextBlock b = viewer->document()->findBlockByLineNumber(hitLine - 1);
                    if (b.isValid()) { viewer->setTextCursor(QTextCursor(b)); viewer->ensureCursorVisible(); }
                    applyVisualBreakpoints();
                    // Raise window to front on breakpoint hit — single deferred call, no loop
                    bringToFront();
                });
            }
        } else if (jdbBuffer.length() > 2000) jdbBuffer.clear();
    }

    // -----------------------------------------------------------------------
    // bringToFront()
    //
    // The core Wayland/XWayland problem: compositors (GNOME Shell, KDE)
    // refuse to honor raise requests from apps that don't have focus,
    // for security reasons. External tools (wmctrl, xdotool) fail for
    // the same reason — they all ultimately send the same blocked request.
    //
    // The ONLY approach that reliably works without compositor cooperation:
    //   1. Temporarily set WindowStaysOnTopHint — this goes through the
    //      window manager hint system which compositors DO honor.
    //   2. show/raise/activateWindow while the hint is active.
    //   3. Remove the hint after 3 seconds so it doesn't stay on top forever.
    //
    // On XWayland (most Ubuntu GNOME setups): step 1 works perfectly.
    // On native Wayland Qt6: also works via xdg-toplevel protocol.
    // On X11: always worked.
    //
    // The Qt flag change causes one window re-creation on some platforms —
    // that is the compositor being told "this is now a top-level window",
    // which is exactly what forces it above others. We avoid the
    // double-flash by NOT calling any external process alongside this.
    // -----------------------------------------------------------------------
    void bringToFront() {
        if (isMinimized()) showNormal();

        // Set always-on-top via the native window handle — does NOT trigger
        // Qt's internal hide/show cycle, so no flash on raise either.
        Qt::WindowFlags current = windowFlags();
        setWindowFlags(current | Qt::WindowStaysOnTopHint);
        show();   // re-shows with new flags applied to the native window
        raise();

        // Remove always-on-top after 3 seconds.
        // IMPORTANT: setWindowFlags() on a QWidget always calls hide()
        // internally — that is what caused the window to disappear.
        // Instead, manipulate the native QWindow handle's flag directly.
        // This updates the compositor hint (_NET_WM_STATE_ABOVE) without
        // touching widget visibility at all: no hide(), no show(), no flash.
        QTimer::singleShot(3000, this, [this]() {
            if (windowHandle()) {
                windowHandle()->setFlag(Qt::WindowStaysOnTopHint, false);
            }
        });
    }

    void onSaveClicked() {
        bool ok; QString name = QInputDialog::getText(this, "Save Phase", "Name:", QLineEdit::Normal, currentGroupName, &ok);
        if (ok && !name.isEmpty()) { if (name != currentGroupName) { breakpointGroups[name] = breakpointGroups[currentGroupName]; currentGroupName = name; if (groupCombo->findText(name) == -1) groupCombo->addItem(name); groupCombo->setCurrentText(name); } saveGroupsToDisk(); }
    }
    void openFile(QString p) { if (p.isEmpty() || !QFileInfo(p).isFile()) return; currentFilePath = p; QFile f(p); if (f.open(QIODevice::ReadOnly)) { viewer->setPlainText(f.readAll()); applyVisualBreakpoints(); } }
    void applyVisualBreakpoints() {
        QList<QTextEdit::ExtraSelection> sel;
        for (int line : breakpointGroups[currentGroupName][currentFilePath]) {
            QTextBlock b = viewer->document()->findBlockByLineNumber(line - 1);
            if (b.isValid()) { QTextEdit::ExtraSelection s; s.format.setBackground(QColor("#7a1a1a")); s.format.setProperty(QTextFormat::FullWidthSelection, true); s.cursor = QTextCursor(b); sel.append(s); }
        }
        if (currentFilePath == hitFilePath && hitLine > 0) {
            QTextBlock b = viewer->document()->findBlockByLineNumber(hitLine - 1);
            if (b.isValid()) { QTextEdit::ExtraSelection s; s.format.setBackground(QColor("#1e4b1e")); s.format.setProperty(QTextFormat::FullWidthSelection, true); s.cursor = QTextCursor(b); sel.append(s); }
        }
        viewer->setExtraSelections(sel);
    }
    QString findPathFromClass(QString c) {
        QString r = c.replace(".", "/") + ".java";
        for (const QString &root : projectPaths) {
            QDirIterator it(root, QStringList() << "*.java", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) { QString p = it.next(); if (p.endsWith(r)) return p; }
        }
        return "";
    }
    void onGroupChanged(const QString &newGroup) {
        if (newGroup.isEmpty() || newGroup == currentGroupName || !jdbProcess) return;
        for (auto const& [path, lines] : breakpointGroups[currentGroupName]) {
            QString cls = deriveClassName(path); for (int line : lines) jdbProcess->write(QString("clear %1:%2\n").arg(cls).arg(line).toUtf8());
        }
        currentGroupName = newGroup;
        for (auto const& [path, lines] : breakpointGroups[currentGroupName]) {
            QString cls = deriveClassName(path); for (int line : lines) jdbProcess->write(QString("stop at %1:%2\n").arg(cls).arg(line).toUtf8());
        }
        applyVisualBreakpoints();
    }
    void onToggleBreakpoint() {
        if (currentFilePath.isEmpty()) return;
        int line = viewer->textCursor().blockNumber() + 1;
        auto &bps = breakpointGroups[currentGroupName][currentFilePath];
        QString cmd = bps.count(line) ? QString("clear %1:%2\n").arg(deriveClassName(currentFilePath)).arg(line) : QString("stop at %1:%2\n").arg(deriveClassName(currentFilePath)).arg(line);
        if(bps.count(line)) bps.erase(line); else bps.insert(line);
        jdbProcess->write(cmd.toUtf8()); applyVisualBreakpoints();
    }
    void runHealthPulse() {
        if (!healthEnabled) return;
        colorStep += 0.02f;
        int r = 28 + static_cast<int>(10 * std::cos(colorStep));
        int g = 32 + static_cast<int>(10 * std::sin(colorStep));
        int textVal = 190 + static_cast<int>(65 * std::cos(colorStep + 3.14f));
        QString tRGB = (QTime::currentTime().hour() >= 20 || QTime::currentTime().hour() <= 6) ? QString("rgb(%1, %2, %3)").arg(textVal).arg(textVal-50).arg(textVal-100) : QString("rgb(%1, %1, %1)").arg(textVal);
        viewer->viewport()->setStyleSheet(QString("background-color: rgb(%1, %2, %3);").arg(r).arg(g).arg(32));
        viewer->setStyleSheet(QString("color: %1; font-family: 'JetBrains Mono', 'Monospace'; font-size: 12pt;").arg(tRGB));
    }
    void saveGroupsToDisk() {
        QJsonObject root;
        for (auto const& [gn, files] : breakpointGroups) {
            QJsonObject go; for (auto const& [fp, lines] : files) { QJsonArray la; for (int l : lines) la.append(l); go.insert(fp, la); }
            root.insert(gn, go);
        }
        QFile file(QDir::currentPath() + "/.hawk_bps.json"); if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(root).toJson());
    }
    void loadGroupsFromFile() {
        QStringList fileNames = { ".hawk_bps.json", ".sahin_bps.json", ".monitor_bps.json" };
        QFile file;
        bool found = false;
        for(const QString &fn : fileNames) {
            file.setFileName(QDir::currentPath() + "/" + fn);
            if(file.exists()) { found = true; break; }
        }
        if (!found || !file.open(QIODevice::ReadOnly)) return;
        QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        for (QString gn : root.keys()) {
            if (groupCombo->findText(gn) == -1) groupCombo->addItem(gn);
            QJsonObject go = root.value(gn).toObject();
            for (QString fp : go.keys()) { 
                QJsonArray la = go.value(fp).toArray(); 
                for (auto l : la) breakpointGroups[gn][fp].insert(l.toInt()); 
            }
        }
    }
    void onEditorSearch(const QString &t) {
        applyVisualBreakpoints(); if (t.isEmpty()) return;
        QTextCursor sc = viewer->document()->find(t, 0); if (!sc.isNull()) { viewer->setTextCursor(sc); viewer->ensureCursorVisible(); }
        QList<QTextEdit::ExtraSelection> sel = viewer->extraSelections(); QTextCursor hc(viewer->document());
        while (!(hc = viewer->document()->find(t, hc)).isNull()) {
            QTextEdit::ExtraSelection s; s.format.setBackground(QColor("#7a6a00")); s.format.setForeground(QColor("#ffffff")); s.cursor = hc; sel.append(s);
        }
        viewer->setExtraSelections(sel);
    }
    void onToggleHealth() { 
        healthEnabled = !healthEnabled; btnHealth->setText(healthEnabled ? "🛡 Eye: ON" : "🛡 Eye: OFF"); 
        if(!healthEnabled) { viewer->viewport()->setStyleSheet("background-color: #1e1e1e;"); viewer->setStyleSheet("color: #d4d4d4; font-family: 'JetBrains Mono', 'Monospace'; font-size: 12pt;"); } 
    }
    void startProcesses() {
        javaProcess = new QProcess(this);
        javaProcess->start("java", QStringList() << "-agentlib:jdwp=transport=dt_socket,server=y,suspend=n,address=9009" << "-cp" << projectPaths[0] + "/target/classes" << "tr.org.tspb.Main");
        jdbProcess = new QProcess(this); jdbProcess->start("jdb", QStringList() << "-attach" << "9009");
        connect(jdbProcess, &QProcess::readyReadStandardOutput, this, &JavaMonitorGUI::handleJdbOutput);
    }
    QString deriveClassName(QString p) { int i = p.indexOf("src/main/java/"); return (i == -1) ? QFileInfo(p).baseName() : p.mid(i + 14).replace(".java", "").replace("/", "."); }
    void buildTree(const QString &p, QStandardItem *it) {
        QDir d(p);
        for (QString dr : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) { QStandardItem *i = new QStandardItem(dr); i->setData(d.absoluteFilePath(dr), Qt::UserRole); it->appendRow(i); buildTree(d.absoluteFilePath(dr), i); }
        for (QString f : d.entryList(QStringList() << "*.java", QDir::Files)) { QStandardItem *i = new QStandardItem(f); i->setData(d.absoluteFilePath(f), Qt::UserRole); it->appendRow(i); }
    }

private:
    QFrame *customTitleBar; QLabel *titleLabel; QStandardItemModel *treeModel;
    QTreeView *tree; CodeEditor *viewer; QLineEdit *searchBar, *editorSearch; QTextEdit *jdbOutput;
    QProcess *javaProcess = nullptr, *jdbProcess = nullptr;
    QStringList projectPaths; QString currentFilePath, hitFilePath, jdbBuffer;
    JavaProjectProxy *proxyModel; 
    int hitLine = -1;
    QString currentGroupName; std::map<QString, std::map<QString, std::set<int>>> breakpointGroups;
    QComboBox *groupCombo; QPushButton *btnSaveGroup, *btnHealth; QTimer *healthTimer;
    float colorStep = 0.0f; bool healthEnabled = true;
};

#include "main.moc"
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QStringList paths;
    if (argc > 1) { for (int i = 1; i < argc; ++i) paths << argv[i]; } 
    else { paths << QDir::currentPath(); }
    JavaMonitorGUI w(paths); w.show(); return a.exec();
}
