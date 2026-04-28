#include <QTextEdit>
#include <QInputDialog>
#include <QDialog>
#include <QLabel>
#include <QApplication>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTreeView>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
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
#include <iostream>
#include <set>
#include <map>

// --- Code Editor with Line Numbers ---
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
        setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: 'Monospace'; font-size: 12pt;");
    }

    void lineNumberAreaPaintEvent(QPaintEvent *event) {
        QPainter painter(lineNumberArea);
        painter.fillRect(event->rect(), QColor("#2b2b2b"));
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
            block = block.next();
            top = bottom;
            bottom = top + qRound(blockBoundingRect(block).height());
            ++blockNumber;
        }
    }

    int lineNumberAreaWidth() {
        int digits = 1;
        int max = qMax(1, blockCount());
        while (max >= 10) { max /= 10; digits++; }
        return 15 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QPlainTextEdit::resizeEvent(event);
        QRect cr = contentsRect();
        lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.bottom()));
    }

public slots:
    void highlightCurrentLine() { emit requestHighlightSync(); }

signals:
    void requestHighlightSync();

private slots:
    void updateLineNumberAreaWidth(int) { setViewportMargins(lineNumberAreaWidth(), 0, 0, 0); }
    void updateLineNumberArea(const QRect &rect, int dy) {
        if (dy) lineNumberArea->scroll(0, dy);
        else lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
        if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
    }

private:
    class LineNumberArea : public QWidget {
    public:
        LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}
        QSize sizeHint() const override { return QSize(codeEditor->lineNumberAreaWidth(), 0); }
    protected:
        void paintEvent(QPaintEvent *event) override { codeEditor->lineNumberAreaPaintEvent(event); }
    private:
        CodeEditor *codeEditor;
    };
    QWidget *lineNumberArea;
};

// --- Proxy ---
class JavaProjectProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    JavaProjectProxy(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        if (sourceModel()->data(index).toString().contains(filterRegExp().pattern(), Qt::CaseInsensitive)) return true;
        for (int i = 0; i < sourceModel()->rowCount(index); ++i) if (filterAcceptsRow(i, index)) return true;
        return false;
    }
};

// --- Main GUI ---
class JavaMonitorGUI : public QMainWindow {
    Q_OBJECT
public:
    JavaMonitorGUI(QString path) {
        projectPath = QDir(path).absolutePath();
        setWindowTitle("Java Monitor [RESTORED] - " + projectPath);
        resize(1300, 850);

        treeModel = new QStandardItemModel(this);
        buildTree(projectPath, treeModel->invisibleRootItem());
        proxyModel = new JavaProjectProxy(this);
        proxyModel->setSourceModel(treeModel);

        searchBar = new QLineEdit();
        searchBar->setPlaceholderText("Teleport search...");
        tree = new QTreeView();
        tree->setModel(proxyModel);
        tree->header()->hide();
        viewer = new CodeEditor();

        // --- Editor search bar ---
        editorSearch = new QLineEdit();
        editorSearch->setPlaceholderText("🔍 Search in file...");
        editorSearch->setStyleSheet("background-color: #2b2b2b; color: #d4d4d4; padding: 6px; font-size: 11pt; border: 1px solid #555;");

        // --- JDB Buttons ---
        QPushButton *btnBP      = new QPushButton("🔴 Breakpoint");
        QPushButton *btnNext    = new QPushButton("⏭ Next");
        QPushButton *btnStop    = new QPushButton("▶ Continue");
        QPushButton *btnStep    = new QPushButton("⬇ Step Into");
        QPushButton *btnOut     = new QPushButton("⬆ Step Out");
        QPushButton *btnLocals  = new QPushButton("📋 Locals");
        QPushButton *btnStack   = new QPushButton("📚 Stack");
        QPushButton *btnThreads = new QPushButton("🧵 Threads");
        QPushButton *btnWhere   = new QPushButton("📍 Where");
        QPushButton *btnExit    = new QPushButton("⏹ Exit JDB");
        
        QPushButton *btnPrint = new QPushButton("🔎 Print Var");        
        QPushButton *btnPrintDump = new QPushButton("🔎 Dump Var");


        btnBP->setStyleSheet("background-color: #7a1a1a; color: white; font-weight: bold; padding: 8px;");
        btnNext->setStyleSheet("background-color: #1a4a7a; color: white; font-weight: bold; padding: 8px;");
        btnStop->setStyleSheet("background-color: #1a7a1a; color: white; font-weight: bold; padding: 8px;");
        btnStep->setStyleSheet("background-color: #1a5a5a; color: white; font-weight: bold; padding: 8px;");
        btnOut->setStyleSheet("background-color: #5a1a7a; color: white; font-weight: bold; padding: 8px;");
        btnLocals->setStyleSheet("background-color: #4a4a1a; color: white; font-weight: bold; padding: 8px;");
        btnStack->setStyleSheet("background-color: #1a4a4a; color: white; font-weight: bold; padding: 8px;");
        btnThreads->setStyleSheet("background-color: #4a1a4a; color: white; font-weight: bold; padding: 8px;");
        btnWhere->setStyleSheet("background-color: #2a2a5a; color: white; font-weight: bold; padding: 8px;");
        btnExit->setStyleSheet("background-color: #3a3a3a; color: white; font-weight: bold; padding: 8px;");
        btnPrint->setStyleSheet("background-color: #2a5a2a; color: white; font-weight: bold; padding: 8px;");        
        btnPrintDump->setStyleSheet("background-color: #2a5a2a; color: white; font-weight: bold; padding: 8px;");

        
        

        // --- Layout ---
        QSplitter *split = new QSplitter(Qt::Horizontal);

        QWidget *left = new QWidget();
        QVBoxLayout *lbox = new QVBoxLayout(left);
        lbox->addWidget(searchBar);
        lbox->addWidget(tree);

        QWidget *right = new QWidget();
        QVBoxLayout *rbox = new QVBoxLayout(right);

        QHBoxLayout *btnBar = new QHBoxLayout();
        btnBar->addWidget(btnBP);
        btnBar->addWidget(btnNext);
        btnBar->addWidget(btnStop);
        btnBar->addWidget(btnStep);
        btnBar->addWidget(btnOut);
        btnBar->addWidget(btnLocals);
        btnBar->addWidget(btnStack);
        btnBar->addWidget(btnThreads);
        btnBar->addWidget(btnWhere);
        btnBar->addWidget(btnExit);
        btnBar->addWidget(btnPrint);
        btnBar->addWidget(btnPrintDump);
        
        jdbOutput = new QTextEdit();
        jdbOutput->setReadOnly(true);
        jdbOutput->setMaximumHeight(150);
        jdbOutput->setStyleSheet("background-color: #111111; color: #00ff99; font-family: 'Monospace'; font-size: 10pt; border: 1px solid #333;");
        jdbOutput->setPlaceholderText("JDB output will appear here...");


        rbox->addWidget(editorSearch);  // search bar above viewer
        rbox->addWidget(viewer);
        rbox->addWidget(jdbOutput);   // <-- ADD THIS
        rbox->addLayout(btnBar);

        split->addWidget(left);
        split->addWidget(right);
        split->setStretchFactor(1, 2);
        setCentralWidget(split);

        // --- Connects ---
        connect(searchBar,    &QLineEdit::textChanged, this, &JavaMonitorGUI::onSearch);
        connect(editorSearch, &QLineEdit::textChanged, this, &JavaMonitorGUI::onEditorSearch);
        connect(tree,         &QTreeView::clicked,     this, &JavaMonitorGUI::onFileClicked);

        connect(btnBP,      &QPushButton::clicked, this, &JavaMonitorGUI::onToggleBreakpoint);
        connect(btnNext,    &QPushButton::clicked, this, &JavaMonitorGUI::onNext);
        connect(btnStop,    &QPushButton::clicked, this, &JavaMonitorGUI::onStop);
        connect(btnStep,    &QPushButton::clicked, this, &JavaMonitorGUI::onStepInto);
        connect(btnOut,     &QPushButton::clicked, this, &JavaMonitorGUI::onStepOut);
        connect(btnLocals,  &QPushButton::clicked, this, &JavaMonitorGUI::onLocals);
        connect(btnStack,   &QPushButton::clicked, this, &JavaMonitorGUI::onStack);
        connect(btnThreads, &QPushButton::clicked, this, &JavaMonitorGUI::onThreads);
        connect(btnWhere,   &QPushButton::clicked, this, &JavaMonitorGUI::onWhere);
        connect(btnExit,    &QPushButton::clicked, this, &JavaMonitorGUI::onExitJdb);
        connect(btnPrint, &QPushButton::clicked, this, &JavaMonitorGUI::onPrintVar);
        connect(btnPrintDump, &QPushButton::clicked, this, &JavaMonitorGUI::onPrintDumpVar);

        connect(viewer, &CodeEditor::requestHighlightSync, this, &JavaMonitorGUI::applyVisualBreakpoints);

        startProcesses();
    }

    ~JavaMonitorGUI() {
        if (jdbProcess && jdbProcess->state() == QProcess::Running) {
            jdbProcess->write("exit\n");
            jdbProcess->waitForFinished(1000);
        }
        if (javaProcess && javaProcess->state() == QProcess::Running) {
            javaProcess->terminate();
            javaProcess->waitForFinished(1000);
        }
    }

private slots:

    // --- Tree search ---
    void onSearch(const QString &text) {
        proxyModel->setFilterFixedString(text);
        if (!text.isEmpty()) tree->expandAll(); else tree->collapseAll();
    }

    // --- Editor file search ---
    void onEditorSearch(const QString &text) {
        if (text.isEmpty()) {
            applyVisualBreakpoints(); // restore normal highlights, clear yellow
            return;
        }

        QTextDocument *doc = viewer->document();

        // Scroll to first match
        QTextCursor cursor = doc->find(text, 0);
        if (!cursor.isNull()) {
            viewer->setTextCursor(cursor);
            viewer->ensureCursorVisible();
        }

        // Start from existing breakpoint/hit highlights
        applyVisualBreakpoints();
        QList<QTextEdit::ExtraSelection> selections = viewer->extraSelections();

        // Highlight ALL matches in yellow ON TOP
        QTextCursor c = doc->find(text, 0);
        while (!c.isNull()) {
            QTextEdit::ExtraSelection s;
            s.format.setBackground(QColor("#7a6a00"));
            s.format.setForeground(QColor("#ffffff"));
            s.cursor = c;
            selections.append(s);
            c = doc->find(text, c);
        }

        viewer->setExtraSelections(selections);
    }

    void onFileClicked(const QModelIndex &index) {
        QString path = proxyModel->data(index, Qt::UserRole).toString();
        openFile(path);
    }

    void openFile(QString path) {
        if (path.isEmpty() || !QFileInfo(path).isFile()) return;
        currentFilePath = path;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            viewer->setPlainText(f.readAll());
            applyVisualBreakpoints();
            // Re-apply editor search highlights if search is active
            if (!editorSearch->text().isEmpty())
                onEditorSearch(editorSearch->text());
        }
    }

    void onToggleBreakpoint() {
        if (currentFilePath.isEmpty()) return;
        QTextBlock block = viewer->textCursor().block();
        int jdbLineNum = block.blockNumber() + 1;
        QString className = deriveClassName(currentFilePath);
        auto &bps = fileBreakpoints[currentFilePath];

        QString command;
        if (bps.count(jdbLineNum)) {
            bps.erase(jdbLineNum);
            command = QString("clear %1:%2\n").arg(className).arg(jdbLineNum);
        } else {
            bps.insert(jdbLineNum);
            command = QString("stop at %1:%2\n").arg(className).arg(jdbLineNum);
        }
        jdbProcess->write(command.toUtf8());
        std::cout << "[JDB COMMAND]: " << command.toStdString() << std::flush;
        applyVisualBreakpoints();
    }

    void onNext() {
        jdbProcess->write("next\n");
        std::cout << "[JDB COMMAND]: next" << std::endl;
    }

    void onStop() {
        jdbProcess->write("cont\n");
        std::cout << "[JDB COMMAND]: cont" << std::endl;
        hitLine = -1;
        hitFilePath.clear();
        applyVisualBreakpoints();
    }

    void onStepInto() {
        jdbProcess->write("step\n");
        std::cout << "[JDB COMMAND]: step" << std::endl;
    }

    void onStepOut() {
        jdbProcess->write("step up\n");
        std::cout << "[JDB COMMAND]: step up" << std::endl;
    }

    void onStack() {
        jdbProcess->write("where\n");
        std::cout << "[JDB COMMAND]: where" << std::endl;
    }

    void onThreads() {
        jdbProcess->write("threads\n");
        std::cout << "[JDB COMMAND]: threads" << std::endl;
    }

    void onWhere() {
        jdbProcess->write("where all\n");
        std::cout << "[JDB COMMAND]: where all" << std::endl;
    }

    void onExitJdb() {
        jdbProcess->write("exit\n");
        std::cout << "[JDB COMMAND]: exit" << std::endl;
        hitLine = -1;
        hitFilePath.clear();
        applyVisualBreakpoints();
    }
    
    void onLocals() {
	jdbProcess->write("locals\n");
	std::cout << "[JDB COMMAND]: locals" << std::endl;
	jdbOutput->append("\n--- locals ---");
    }

    void onPrintDumpVar() {
    	// Ask user which variable to inspect
	bool ok;
	QString varName = QInputDialog::getText(
	    this,
	    "Print Variable",
	    "Enter variable name:",
	    QLineEdit::Normal,
	    "",
	    &ok
	);
	if (ok && !varName.isEmpty()) {
	    QString cmd = QString("print %1\n").arg(varName);
	    jdbProcess->write(cmd.toUtf8());
	    std::cout << "[JDB COMMAND]: " << cmd.toStdString() << std::flush;
	    jdbOutput->append(QString("\n>>> print %1").arg(varName));
	}
    }
    
    void onPrintVar() {
    	bool ok;
	QString varName = QInputDialog::getText(
	    this, "Inspect Variable",
	   "Enter variable (use 'dump <name>' for POJO details):",
	    QLineEdit::Normal, "", &ok
	);
	
	if (ok && !varName.isEmpty()) {
	    QString cmd;
	    // If the user didn't type 'dump' or 'print', default to dump for POJOs
	    if (!varName.startsWith("dump ") && !varName.startsWith("print ")) {
	        cmd = QString("dump %1\n").arg(varName);
	    } else {
	        cmd = varName + "\n";
	    }
	
	    jdbProcess->write(cmd.toUtf8());
	    jdbOutput->append(QString("\n>>> %1").arg(cmd.trimmed()));
	}
    }

    void handleJdbOutput() {
        jdbBuffer += jdbProcess->readAllStandardOutput();
        std::cout << "[JDB]: " << jdbBuffer.toStdString() << std::flush;
        
        jdbOutput->append(jdbBuffer.trimmed());


        if (!jdbBuffer.contains('\n')) return;

        static QRegularExpression re(
            "Breakpoint hit:.*thread=(\\S+).*,\\s+(\\S+)\\.\\w+\\(.*\\),\\s+line=(\\d+)"
        );

        QRegularExpressionMatch match = re.match(jdbBuffer);
        std::cout << "[DEBUG] buffer: " << jdbBuffer.toStdString() << std::endl;

        if (match.hasMatch()) {
            QString className = match.captured(2).trimmed();
            int lineNum = match.captured(3).toInt();

            std::cout << "[DEBUG] className: " << className.toStdString() << std::endl;
            std::cout << "[DEBUG] lineNum: " << lineNum << std::endl;

            hitFilePath = findPathFromClass(className);

            std::cout << "[DEBUG] hitFilePath: " << hitFilePath.toStdString() << std::endl;
            std::cout << "[DEBUG] currentFilePath: " << currentFilePath.toStdString() << std::endl;

            hitLine = lineNum;
            jdbBuffer.clear();

            QTimer::singleShot(0, this, [this](){
                if (!hitFilePath.isEmpty()) {
                    openFile(hitFilePath);
                    QTextBlock block = viewer->document()->findBlockByLineNumber(hitLine - 1);
                    if (block.isValid()) {
                        QTextCursor cursor(block);
                        viewer->setTextCursor(cursor);
                        viewer->ensureCursorVisible();
                    }
                }
                applyVisualBreakpoints();
                viewer->viewport()->update();
            });
        } else {
            if (jdbBuffer.size() > 4096) jdbBuffer.clear();
        }
    }

    void applyVisualBreakpoints() {
        QList<QTextEdit::ExtraSelection> selections;

        // 1. Current Cursor/Line (Gray) — skip if cursor is ON the hit line
        bool cursorIsHitLine = (currentFilePath == hitFilePath && hitLine > 0 &&
                                viewer->textCursor().blockNumber() == hitLine - 1);
        if (!cursorIsHitLine) {
            QTextEdit::ExtraSelection curLine;
            curLine.format.setBackground(QColor("#2c2c2c"));
            curLine.format.setProperty(QTextFormat::FullWidthSelection, true);
            curLine.cursor = viewer->textCursor();
            curLine.cursor.clearSelection();
            selections.append(curLine);
        }

        // 2. Breakpoints (RED) — skip hit line, green wins
        auto &bps = fileBreakpoints[currentFilePath];
        for (int line : bps) {
            if (currentFilePath == hitFilePath && line == hitLine)
                continue;
            QTextBlock b = viewer->document()->findBlockByLineNumber(line - 1);
            if (b.isValid()) {
                QTextEdit::ExtraSelection s;
                s.format.setBackground(QColor("#7a1a1a"));
                s.format.setProperty(QTextFormat::FullWidthSelection, true);
                s.cursor = QTextCursor(b);
                s.cursor.clearSelection();
                selections.append(s);
            }
        }

        // 3. Current execution point (GREEN) — always last, always on top
        if (currentFilePath == hitFilePath && hitLine > 0) {
            QTextBlock b = viewer->document()->findBlockByLineNumber(hitLine - 1);
            if (b.isValid()) {
                QTextEdit::ExtraSelection s;
                s.format.setBackground(QColor("#1e4b1e"));
                s.format.setProperty(QTextFormat::FullWidthSelection, true);
                s.cursor = QTextCursor(b);
                s.cursor.clearSelection();
                selections.append(s);
            }
        }

        viewer->setExtraSelections(selections);
    }

private:
    void startProcesses() {
        javaProcess = new QProcess(this);
        javaProcess->start("java", QStringList() << "-agentlib:jdwp=transport=dt_socket,server=y,suspend=n,address=9009" << "-cp" << projectPath + "/target/classes" << "com.tspb.Main");

        jdbProcess = new QProcess(this);
        jdbProcess->start("jdb", QStringList() << "-attach" << "9009");
        connect(jdbProcess, &QProcess::readyReadStandardOutput, this, &JavaMonitorGUI::handleJdbOutput);
    }

    QString deriveClassName(QString path) {
        int idx = path.indexOf("src/main/java/");
        return (idx == -1) ? QFileInfo(path).baseName() : path.mid(idx + 14).replace(".java", "").replace("/", ".");
    }

    QString findPathFromClass(QString className) {
        QString relPath = className.replace(".", "/") + ".java";
        QDirIterator it(projectPath, QStringList() << "*.java", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString p = it.next();
            if (p.endsWith(relPath)) return p;
        }
        return "";
    }

    void buildTree(const QString &path, QStandardItem *p) {
        QDir dir(path);
        for (QString d : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QStandardItem *i = new QStandardItem(d); i->setData(dir.absoluteFilePath(d), Qt::UserRole);
            p->appendRow(i); buildTree(dir.absoluteFilePath(d), i);
        }
        for (QString f : dir.entryList(QStringList() << "*.java", QDir::Files)) {
            QStandardItem *i = new QStandardItem(f); i->setData(dir.absoluteFilePath(f), Qt::UserRole); p->appendRow(i);
        }
    }

    QStandardItemModel *treeModel;
    JavaProjectProxy *proxyModel;
    QTreeView *tree;
    CodeEditor *viewer;
    QLineEdit *searchBar;
    QLineEdit *editorSearch;   // search inside file
    QTextEdit *jdbOutput; // <--- ADD THIS HERE
    QProcess *javaProcess = nullptr;
    QProcess *jdbProcess = nullptr;
    QString projectPath, currentFilePath, hitFilePath;
    int hitLine = -1;
    std::map<QString, std::set<int>> fileBreakpoints;
    QString jdbBuffer;
};

#include "main.moc"
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    JavaMonitorGUI w((argc > 1) ? argv[1] : QDir::currentPath());
    w.show();
    return a.exec();
}
