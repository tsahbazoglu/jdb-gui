#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QInputDialog>
#include <QStandardItemModel>
#include <QTreeView>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QDirIterator>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QTextBlock>
#include <QTextCursor>
#include <QPainter>
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <QWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <map>
#include <set>
#include <cmath>

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
public:
    CodeEditor(QWidget *parent = nullptr) : QPlainTextEdit(parent) {
        setReadOnly(true);
        setLineWrapMode(QPlainTextEdit::NoWrap);
        setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: 'JetBrains Mono', 'Monospace'; font-size: 11pt; border: none;");
    }
};

class JavaMonitorGUI : public QMainWindow {
    Q_OBJECT
public:
    JavaMonitorGUI(QStringList paths) : projectPaths(paths) {
        currentGroupName = "Default";
        setWindowTitle("🦅 Hawk Debugger [WAYLAND FORCE]");
        resize(1350, 850);
        setupUI();
        loadGroupsFromFile();
        
        healthTimer = new QTimer(this);
        connect(healthTimer, &QTimer::timeout, this, &JavaMonitorGUI::runHealthPulse);
        healthTimer->start(50);

        QTimer::singleShot(500, this, &JavaMonitorGUI::startProcesses);
    }

private slots:
    void bringToFrontWayland() {
    if (windowHandle()) {
        // 1. If minimized, we MUST restore it first
        if (isMinimized()) {
            showNormal();
        }

        // 2. THE FORCE: We briefly hide the window to break the compositor's 
        // focus-lock on the browser, then set the "Top" layer flag.
        this->hide(); 
        
        windowHandle()->setFlag(Qt::WindowStaysOnTopHint, true);
        
        // 3. Re-show. On Wayland, a newly shown 'StaysOnTop' window 
        // is almost always granted the top-level Z-order.
        this->show();
        this->raise();

        // 4. Cleanup: Remove the "Sticky" behavior after 2 seconds
        QTimer::singleShot(2000, this, [this]() {
            if (windowHandle()) {
                windowHandle()->setFlag(Qt::WindowStaysOnTopHint, false);
                // We do NOT call hide/show here, just drop the priority 
                // so the user can go back to the browser if they want.
                this->raise(); 
            }
        });
    }
}

    void handleJdbOutput() {
        QString o = jdbProcess->readAllStandardOutput();
        jdbBuffer += o;
        jdbOutput->append(o.trimmed());

        static QRegularExpression re("(?:Breakpoint hit|Step completed):.*thread=.*?,\\s+(\\S+)\\.\\w+\\(.*\\),\\s+line=([\\d,]+)");
        QRegularExpressionMatch m = re.match(jdbBuffer);
        
        if (m.hasMatch()) {
            QString className = m.captured(1);
            hitLine = m.captured(2).remove(',').toInt();
            hitFilePath = findPathFromClass(className);
            jdbBuffer.clear();

            if (!hitFilePath.isEmpty()) {
                openFile(hitFilePath);
                QTextBlock b = viewer->document()->findBlockByLineNumber(hitLine - 1);
                if (b.isValid()) { 
                    viewer->setTextCursor(QTextCursor(b)); 
                    viewer->ensureCursorVisible(); 
                }
                applyVisualBreakpoints();
                
                // Small delay to ensure the UI has finished rendering the new file
                // before we attempt the layer shift.
                QTimer::singleShot(100, this, &JavaMonitorGUI::bringToFrontWayland);
            }
        } else if (jdbBuffer.length() > 5000) {
            jdbBuffer.clear(); 
        }
    }

    void onSaveClicked() {
        bool ok; QString name = QInputDialog::getText(this, "Save Phase", "Name:", QLineEdit::Normal, currentGroupName, &ok);
        if (ok && !name.isEmpty()) {
            if (name != currentGroupName) {
                breakpointGroups[name] = breakpointGroups[currentGroupName];
                currentGroupName = name;
                if (groupCombo->findText(name) == -1) groupCombo->addItem(name);
                groupCombo->setCurrentText(name);
            }
            saveGroupsToDisk();
        }
    }

    void onEditorSearch(const QString &t) {
        applyVisualBreakpoints();
        if (t.isEmpty()) return;
        QList<QTextEdit::ExtraSelection> sel = viewer->extraSelections();
        QTextCursor hc(viewer->document());
        bool first = true;
        while (!(hc = viewer->document()->find(t, hc)).isNull()) {
            if (first) { viewer->setTextCursor(hc); viewer->ensureCursorVisible(); first = false; }
            QTextEdit::ExtraSelection s;
            s.format.setBackground(QColor("#7a6a00"));
            s.format.setForeground(QColor("#ffffff"));
            s.cursor = hc;
            sel.append(s);
        }
        viewer->setExtraSelections(sel);
    }

    void onGroupChanged(const QString &newGroup) {
        if (newGroup.isEmpty() || newGroup == currentGroupName || !jdbProcess) return;
        for (auto const& [path, lines] : breakpointGroups[currentGroupName]) {
            QString cls = deriveClassName(path);
            for (int line : lines) jdbProcess->write(QString("clear %1:%2\n").arg(cls).arg(line).toUtf8());
        }
        currentGroupName = newGroup;
        for (auto const& [path, lines] : breakpointGroups[currentGroupName]) {
            QString cls = deriveClassName(path);
            for (int line : lines) jdbProcess->write(QString("stop at %1:%2\n").arg(cls).arg(line).toUtf8());
        }
        applyVisualBreakpoints();
    }

    void onToggleBreakpoint() {
        if (currentFilePath.isEmpty()) return;
        int line = viewer->textCursor().blockNumber() + 1;
        auto &bps = breakpointGroups[currentGroupName][currentFilePath];
        QString cls = deriveClassName(currentFilePath);
        QString cmd = bps.count(line) ? QString("clear %1:%2\n").arg(cls).arg(line) : QString("stop at %1:%2\n").arg(cls).arg(line);
        if(bps.count(line)) bps.erase(line); else bps.insert(line);
        jdbProcess->write(cmd.toUtf8()); 
        applyVisualBreakpoints();
    }

    void runHealthPulse() {
        if (!healthEnabled) return;
        colorStep += 0.02f;
        int r = 28 + static_cast<int>(10 * std::cos(colorStep));
        int g = 32 + static_cast<int>(10 * std::sin(colorStep));
        viewer->viewport()->setStyleSheet(QString("background-color: rgb(%1, %2, %3);").arg(r).arg(g).arg(32));
    }

private:
    void setupUI() {
        QWidget *central = new QWidget();
        QVBoxLayout *mainLayout = new QVBoxLayout(central);
        QSplitter *split = new QSplitter(Qt::Horizontal);

        // SIDEBAR
        QWidget *left = new QWidget();
        QVBoxLayout *lbox = new QVBoxLayout(left);
        lbox->setContentsMargins(0,0,0,0);
        searchBar = new QLineEdit();
        searchBar->setPlaceholderText(" 🔍 Teleport...");
        treeModel = new QStandardItemModel(this);
        for (const QString &p : projectPaths) {
            QString abs = QDir(p).absolutePath();
            QStandardItem *root = new QStandardItem(QFileInfo(abs).fileName());
            root->setData(abs, Qt::UserRole);
            treeModel->invisibleRootItem()->appendRow(root);
            buildTree(abs, root);
        }
        proxyModel = new JavaProjectProxy(this);
        proxyModel->setSourceModel(treeModel);
        tree = new QTreeView();
        tree->setModel(proxyModel);
        tree->setHeaderHidden(true);
        lbox->addWidget(searchBar); lbox->addWidget(tree);

        // EDITOR
        QWidget *right = new QWidget();
        QVBoxLayout *rbox = new QVBoxLayout(right);
        rbox->setContentsMargins(0,0,0,0);
        editorSearch = new QLineEdit();
        editorSearch->setPlaceholderText(" 🔍 Find in file...");
        viewer = new CodeEditor();
        jdbOutput = new QTextEdit();
        jdbOutput->setFixedHeight(150);
        jdbOutput->setReadOnly(true);
        jdbOutput->setStyleSheet("background: #000; color: #0f9; font-family: monospace;");

        // BUTTONS
        QHBoxLayout *btnBar = new QHBoxLayout();
        btnBar->setContentsMargins(5,5,5,5); btnBar->setSpacing(4);
        auto createBtn = [&](QString txt, QString col, int w = 75) {
            QPushButton *b = new QPushButton(txt);
            b->setFixedWidth(w);
            b->setStyleSheet(QString("background: %1; color: white; font-weight: bold; font-size: 10px; border-radius: 2px; height: 26px;").arg(col));
            return b;
        };

        QPushButton *btnBP = createBtn("🔴 BP", "#7a1a1a");
        QPushButton *btnNext = createBtn("NEXT", "#1a4a7a");
        QPushButton *btnCont = createBtn("RESUME", "#1a7a1a");
        QPushButton *btnStep = createBtn("STEP", "#1a5a5a");
        QPushButton *btnLocals = createBtn("LOCALS", "#4a4a1a");
        QPushButton *btnStack = createBtn("STACK", "#1a4a4a");
        QPushButton *btnThreads = createBtn("THREADS", "#4a1a4a");
        QPushButton *btnPrint = createBtn("PRINT", "#2a5a2a");
        btnHealth = createBtn("EYE: ON", "#2a5a5a", 100);
        groupCombo = new QComboBox();
        groupCombo->setFixedWidth(140);
        QPushButton *btnSave = createBtn("💾 SAVE", "#444", 60);

        btnBar->addWidget(btnBP); btnBar->addWidget(btnNext); btnBar->addWidget(btnCont);
        btnBar->addWidget(btnStep); btnBar->addWidget(btnLocals); btnBar->addWidget(btnStack);
        btnBar->addWidget(btnThreads); btnBar->addWidget(btnPrint); btnBar->addWidget(btnHealth);
        btnBar->addStretch(); btnBar->addWidget(new QLabel("PHASE:")); btnBar->addWidget(groupCombo);
        btnBar->addWidget(btnSave);

        rbox->addWidget(editorSearch); rbox->addWidget(viewer); rbox->addWidget(jdbOutput); rbox->addLayout(btnBar);
        split->addWidget(left); split->addWidget(right); split->setStretchFactor(1, 4);
        mainLayout->addWidget(split);
        setCentralWidget(central);

        connect(tree, &QTreeView::clicked, this, [this](const QModelIndex &i){ openFile(i.data(Qt::UserRole).toString()); });
        connect(btnBP, &QPushButton::clicked, this, &JavaMonitorGUI::onToggleBreakpoint);
        connect(btnNext, &QPushButton::clicked, this, [this](){ jdbProcess->write("next\n"); });
        connect(btnCont, &QPushButton::clicked, this, [this](){ jdbProcess->write("cont\n"); hitLine = -1; applyVisualBreakpoints(); });
        connect(btnStep, &QPushButton::clicked, this, [this](){ jdbProcess->write("step\n"); });
        connect(btnLocals, &QPushButton::clicked, this, [this](){ jdbProcess->write("locals\n"); });
        connect(btnStack, &QPushButton::clicked, this, [this](){ jdbProcess->write("where\n"); });
        connect(btnThreads, &QPushButton::clicked, this, [this](){ jdbProcess->write("threads\n"); });
        connect(btnPrint, &QPushButton::clicked, this, [this](){ bool ok; QString v = QInputDialog::getText(this, "Print", "Var:", QLineEdit::Normal, "", &ok); if(ok) jdbProcess->write(QString("print %1\n").arg(v).toUtf8()); });
        connect(btnSave, &QPushButton::clicked, this, &JavaMonitorGUI::onSaveClicked);
        connect(btnHealth, &QPushButton::clicked, this, [this](){ healthEnabled = !healthEnabled; btnHealth->setText(healthEnabled ? "EYE: ON" : "EYE: OFF"); if(!healthEnabled) viewer->viewport()->setStyleSheet("background-color: #1e1e1e;"); });
        connect(searchBar, &QLineEdit::textChanged, this, [this](const QString &t){ proxyModel->setFilterRegularExpression(t); tree->expandAll(); });
        connect(editorSearch, &QLineEdit::textChanged, this, &JavaMonitorGUI::onEditorSearch);
        connect(groupCombo, &QComboBox::currentTextChanged, this, &JavaMonitorGUI::onGroupChanged);
    }

    void openFile(QString p) { if (p.isEmpty() || !QFileInfo(p).isFile()) return; currentFilePath = p; QFile f(p); if (f.open(QIODevice::ReadOnly)) { viewer->setPlainText(f.readAll()); applyVisualBreakpoints(); } }

    void applyVisualBreakpoints() {
        QList<QTextEdit::ExtraSelection> sel;
        for (int l : breakpointGroups[currentGroupName][currentFilePath]) {
            QTextBlock b = viewer->document()->findBlockByLineNumber(l - 1);
            if (b.isValid()) { QTextEdit::ExtraSelection s; s.format.setBackground(QColor("#7a1a1a")); s.format.setProperty(QTextFormat::FullWidthSelection, true); s.cursor = QTextCursor(b); sel.append(s); }
        }
        if (currentFilePath == hitFilePath && hitLine > 0) {
            QTextBlock b = viewer->document()->findBlockByLineNumber(hitLine - 1);
            if (b.isValid()) { QTextEdit::ExtraSelection s; s.format.setBackground(QColor("#1e4b1e")); s.format.setProperty(QTextFormat::FullWidthSelection, true); s.cursor = QTextCursor(b); sel.append(s); }
        }
        viewer->setExtraSelections(sel);
    }

    void buildTree(const QString &p, QStandardItem *it) {
        QDir d(p);
        for (QString dr : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) { QStandardItem *i = new QStandardItem(dr); i->setData(d.absoluteFilePath(dr), Qt::UserRole); it->appendRow(i); buildTree(d.absoluteFilePath(dr), i); }
        for (QString f : d.entryList(QStringList() << "*.java", QDir::Files)) { QStandardItem *i = new QStandardItem(f); i->setData(d.absoluteFilePath(f), Qt::UserRole); it->appendRow(i); }
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
        QStringList fns = { ".hawk_bps.json", ".sahin_bps.json" };
        for(const QString &fn : fns) {
            QFile f(QDir::currentPath() + "/" + fn);
            if(f.open(QIODevice::ReadOnly)) {
                QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
                for (QString gn : root.keys()) {
                    if (groupCombo->findText(gn) == -1) groupCombo->addItem(gn);
                    QJsonObject go = root.value(gn).toObject();
                    for (QString fp : go.keys()) { QJsonArray la = go.value(fp).toArray(); for (auto l : la) breakpointGroups[gn][fp].insert(l.toInt()); }
                }
                break;
            }
        }
    }

    QString findPathFromClass(QString c) {
        QString r = c.replace(".", "/") + ".java";
        for (const QString &root : projectPaths) {
            QDirIterator it(root, QStringList() << "*.java", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) { QString p = it.next(); if (p.endsWith(r)) return p; }
        }
        return "";
    }

    QString deriveClassName(QString p) {
        int i = p.indexOf("src/main/java/");
        return (i == -1) ? QFileInfo(p).baseName() : p.mid(i + 14).replace(".java", "").replace("/", ".");
    }

    void startProcesses() {
        if (projectPaths.isEmpty()) return;
        javaProcess = new QProcess(this);
        javaProcess->start("java", QStringList() << "-agentlib:jdwp=transport=dt_socket,server=y,suspend=n,address=9009" << "-cp" << QDir(projectPaths[0]).absolutePath() + "/target/classes" << "tr.org.tspb.Main");
        jdbProcess = new QProcess(this);
        jdbProcess->start("jdb", QStringList() << "-attach" << "9009");
        connect(jdbProcess, &QProcess::readyReadStandardOutput, this, &JavaMonitorGUI::handleJdbOutput);
    }

    QTreeView *tree; CodeEditor *viewer; QLineEdit *searchBar, *editorSearch; QTextEdit *jdbOutput;
    QProcess *javaProcess = nullptr, *jdbProcess = nullptr; QStringList projectPaths;
    QString currentFilePath, hitFilePath, jdbBuffer; JavaProjectProxy *proxyModel; QStandardItemModel *treeModel;
    int hitLine = -1; QString currentGroupName; QComboBox *groupCombo; QPushButton *btnHealth;
    std::map<QString, std::map<QString, std::set<int>>> breakpointGroups;
    QTimer *healthTimer; float colorStep = 0.0f; bool healthEnabled = true;
};

int main(int argc, char *argv[]) {
    // FORCE NATIVE WAYLAND
    qputenv("QT_QPA_PLATFORM", "wayland");
    
    QApplication a(argc, argv);
    QStringList paths;
    if (argc > 1) { for (int i = 1; i < argc; ++i) paths << argv[i]; } 
    else { paths << QDir::currentPath(); }
    JavaMonitorGUI w(paths); w.show(); return a.exec();
}

#include "main.moc"
