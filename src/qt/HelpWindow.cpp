#include "qt/HelpWindow.h"

#include <QAction>
#include <QBoxLayout>
#include <QByteArray>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QFile>
#include <QIODevice>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QString>
#include <QTabWidget>
#include <QTextBrowser>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVariant>
#include <QWidget>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Mandelbrotter/app/help_routing.h"
#include "Mandelbrotter/app/help_sitemap.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/help_action.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

using namespace Qt::StringLiterals;

namespace
{

/// Where the book lives in the resources.
const QString kBookRoot = u":/help/"_s;
const QString kBookUrl  = u"qrc:/help/"_s;

std::optional<std::string> readBookFile(std::string_view name)
{
    QFile file(kBookRoot + toQt(name));
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    return file.readAll().toStdString();
}

std::string_view withoutAnchor(std::string_view local)
{
    return local.substr(0, local.find('#'));
}

}  // namespace

std::string helpActionText(const QUrl& url)
{
    std::string text(kHelpActionScheme);
    text += fromQt(url.path(QUrl::FullyDecoded));
    return text;
}

HelpWindow::HelpWindow()
  : m_browser(new QTextBrowser(this)),
    m_tabs(new QTabWidget(this)),
    m_contents(new QTreeWidget(this)),
    m_indexFilter(new QLineEdit(this)),
    m_index(new QListWidget(this)),
    m_showAll(new QPushButton(u"Show all"_s, this)),
    m_query(new QLineEdit(this)),
    m_caseSensitive(new QCheckBox(u"Case sensitive"_s, this)),
    m_wholeWords(new QCheckBox(u"Whole words only"_s, this)),
    m_results(new QListWidget(this))
{
    openExternal = [](const QUrl& url) { QDesktopServices::openUrl(url); };
    // Closing the main window still quits; the main window owns (and deletes) this one.
    setAttribute(Qt::WA_QuitOnClose, false);
    setWindowTitle(toQt(app::kHelpTitlePrefix));
    resize(700, 480);  // wx's default help frame

    m_browser->setOpenLinks(false);  // links go through onLinkClicked
    connect(m_browser, &QTextBrowser::anchorClicked, this,
            [this](const QUrl& url) { onLinkClicked(url); });
    // The new source comes with the signal: source() may not have caught up yet.
    connect(m_browser, &QTextBrowser::sourceChanged, this,
            [this](const QUrl& source) { onSourceChanged(source); });

    buildNavigation();
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_tabs);
    splitter->addWidget(m_browser);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 460});
    setCentralWidget(splitter);
    buildToolBar();
}

// ---------------------------------------------------------------------------------------------------------------
// Construction

void HelpWindow::buildNavigation()
{
    m_contents->setHeaderHidden(true);
    const auto openEntry = [this](QTreeWidgetItem* item) {
        showEntry(item->data(0, Qt::UserRole).toInt());
    };
    connect(m_contents, &QTreeWidget::itemClicked, this, openEntry);
    connect(m_contents, &QTreeWidget::itemActivated, this, openEntry);
    m_tabs->addTab(m_contents, u"Contents"_s);

    auto* indexPage   = new QWidget(this);
    auto* indexLayout = new QVBoxLayout(indexPage);
    auto* filterRow   = new QHBoxLayout();
    filterRow->addWidget(m_indexFilter, 1);
    filterRow->addWidget(m_showAll);
    indexLayout->addLayout(filterRow);
    indexLayout->addWidget(m_index, 1);
    connect(m_indexFilter, &QLineEdit::textChanged, this,
            [this](const QString& text) { filterIndex(text); });
    connect(m_showAll, &QPushButton::clicked, m_indexFilter, &QLineEdit::clear);
    const auto openPage = [this](QListWidgetItem* item) {
        showPage(fromQt(item->data(Qt::UserRole).toString()));
    };
    connect(m_index, &QListWidget::itemClicked, this, openPage);
    connect(m_index, &QListWidget::itemActivated, this, openPage);
    m_tabs->addTab(indexPage, u"Index"_s);

    auto* searchPage   = new QWidget(this);
    auto* searchLayout = new QVBoxLayout(searchPage);
    auto* queryRow     = new QHBoxLayout();
    auto* run          = new QPushButton(u"Search"_s, searchPage);
    queryRow->addWidget(m_query, 1);
    queryRow->addWidget(run);
    searchLayout->addLayout(queryRow);
    searchLayout->addWidget(m_caseSensitive);
    searchLayout->addWidget(m_wholeWords);
    searchLayout->addWidget(m_results, 1);
    connect(m_query, &QLineEdit::returnPressed, this, [this] { search(); });
    connect(run, &QPushButton::clicked, this, [this] { search(); });
    connect(m_results, &QListWidget::itemClicked, this, openPage);
    connect(m_results, &QListWidget::itemActivated, this, openPage);
    m_tabs->addTab(searchPage, u"Search"_s);
}

void HelpWindow::buildToolBar()
{
    QToolBar* bar = addToolBar(u"Help"_s);
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    QAction* navigation = bar->addAction(u"Navigation"_s);
    navigation->setToolTip(u"Show or hide the contents, index and search"_s);
    navigation->setCheckable(true);
    navigation->setChecked(true);
    connect(navigation, &QAction::toggled, m_tabs, &QWidget::setVisible);

    m_back = bar->addAction(u"Back"_s);
    m_back->setShortcut(QKeySequence::Back);
    m_back->setEnabled(false);
    connect(m_back, &QAction::triggered, m_browser, &QTextBrowser::backward);
    connect(m_browser, &QTextBrowser::backwardAvailable, m_back, &QAction::setEnabled);

    m_forward = bar->addAction(u"Forward"_s);
    m_forward->setShortcut(QKeySequence::Forward);
    m_forward->setEnabled(false);
    connect(m_forward, &QAction::triggered, m_browser, &QTextBrowser::forward);
    connect(m_browser, &QTextBrowser::forwardAvailable, m_forward, &QAction::setEnabled);

    m_up = bar->addAction(u"Up"_s);
    m_up->setToolTip(u"The page this one belongs to in the contents"_s);
    connect(m_up, &QAction::triggered, this, [this] {
        if (const int entry = currentEntry(); entry >= 0)
        {
            showEntry(m_entries.at(static_cast<std::size_t>(entry)).parent);
        }
    });
    m_previous = bar->addAction(u"Previous"_s);
    m_previous->setToolTip(u"The previous page of the contents"_s);
    connect(m_previous, &QAction::triggered, this, [this] { showEntry(currentEntry() - 1); });
    m_next = bar->addAction(u"Next"_s);
    m_next->setToolTip(u"The next page of the contents"_s);
    connect(m_next, &QAction::triggered, this, [this] { showEntry(currentEntry() + 1); });

    bar->addSeparator();
    QAction* print = bar->addAction(u"Print..."_s);
    print->setShortcut(QKeySequence::Print);
    connect(print, &QAction::triggered, this, [this] {
        QPrinter     printer;
        QPrintDialog dialog(&printer, this);
        if (dialog.exec() == QDialog::Accepted)
        {
            m_browser->print(&printer);
        }
    });
    // In place of wx's font dialog.
    QAction* smaller = bar->addAction(u"Smaller"_s);
    smaller->setShortcut(QKeySequence::ZoomOut);
    connect(smaller, &QAction::triggered, this, [this] { m_browser->zoomOut(); });
    QAction* larger = bar->addAction(u"Larger"_s);
    larger->setShortcut(QKeySequence::ZoomIn);
    connect(larger, &QAction::triggered, this, [this] { m_browser->zoomIn(); });
}

bool HelpWindow::loadBook()
{
    if (m_loaded)
    {
        return true;
    }
    std::optional<app::HelpProject> project;
    if (const auto hhp = readBookFile("help.hhp"))
    {
        if (auto parsed = app::parseHelpProject(*hhp))
        {
            project = std::move(*parsed);
        }
    }
    const auto contents = project ? readBookFile(project->contentsFile) : std::nullopt;
    const auto index    = project ? readBookFile(project->indexFile) : std::nullopt;
    if (!contents || !index)
    {
        if (onError)
        {
            onError(std::string(app::kHelpBookUnreadable));
        }
        return false;
    }
    m_project = *project;

    m_contents->clear();
    m_entries.clear();
    addTopics(nullptr, app::parseHelpSitemap(*contents), -1);
    m_contents->expandAll();

    for (const app::HelpTopic& topic : app::parseHelpSitemap(*index))
    {
        auto* item = new QListWidgetItem(toQt(topic.name), m_index);
        item->setData(Qt::UserRole, toQt(topic.local));
    }

    // The search covers every page of the contents, in their order.
    std::vector<std::pair<std::string, std::string>> pages;
    for (const Entry& entry : m_entries)
    {
        const std::string file(withoutAnchor(entry.local));
        bool              seen = false;
        for (const auto& [name, html] : pages)
        {
            seen = seen || name == file;
        }
        if (!seen)
        {
            pages.emplace_back(file, readBookFile(file).value_or(std::string()));
        }
    }
    m_search = std::make_unique<app::HelpSearch>(pages);
    m_loaded = true;
    return true;
}

void HelpWindow::addTopics(QTreeWidgetItem* parent, const std::vector<app::HelpTopic>& topics,
                           int parentEntry)
{
    for (const app::HelpTopic& topic : topics)
    {
        const int entry = static_cast<int>(m_entries.size());
        m_entries.push_back({.name = topic.name, .local = topic.local, .parent = parentEntry});
        auto* item =
            parent != nullptr ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_contents);
        item->setText(0, toQt(topic.name));
        item->setData(0, Qt::UserRole, entry);
        addTopics(item, topic.children, entry);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Pages

void HelpWindow::showContents()
{
    if (loadBook())
    {
        showPage(m_project.defaultTopic);
    }
}

void HelpWindow::showPage(std::string_view page)
{
    if (!loadBook())
    {
        return;
    }
    if (!QFile::exists(kBookRoot + toQt(withoutAnchor(page))))
    {
        page = m_project.defaultTopic;
    }
    m_browser->setSource(QUrl(kBookUrl + toQt(page)));
    show();
    raise();
    activateWindow();
}

std::string HelpWindow::pageOf(const QUrl& source)
{
    std::string page = fromQt(source.fileName());
    if (source.hasFragment())
    {
        page += "#" + fromQt(source.fragment());
    }
    return page;
}

std::string HelpWindow::currentPage() const
{
    return pageOf(m_browser->source());
}

int HelpWindow::currentEntry() const
{
    return entryOf(currentPage());
}

int HelpWindow::entryOf(std::string_view page) const
{
    int best = -1;
    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries[i].local == page)
        {
            return static_cast<int>(i);
        }
        if (best < 0 && withoutAnchor(m_entries[i].local) == withoutAnchor(page))
        {
            best = static_cast<int>(i);
        }
    }
    return best;
}

void HelpWindow::showEntry(int entry)
{
    if (entry >= 0 && static_cast<std::size_t>(entry) < m_entries.size())
    {
        showPage(m_entries[static_cast<std::size_t>(entry)].local);
    }
}

void HelpWindow::onSourceChanged(const QUrl& source)
{
    setWindowTitle(toQt(app::kHelpTitlePrefix) + m_browser->documentTitle());
    const int entry = entryOf(pageOf(source));
    m_up->setEnabled(entry >= 0 && m_entries.at(static_cast<std::size_t>(entry)).parent >= 0);
    m_previous->setEnabled(entry > 0);
    m_next->setEnabled(entry >= 0 && static_cast<std::size_t>(entry) + 1 < m_entries.size());

    // The contents tree follows the page shown.
    const QSignalBlocker blocker(m_contents);
    for (QTreeWidgetItemIterator it(m_contents); *it != nullptr; ++it)
    {
        if ((*it)->data(0, Qt::UserRole).toInt() == entry)
        {
            m_contents->setCurrentItem(*it);
            break;
        }
    }
}

void HelpWindow::onLinkClicked(const QUrl& url)
{
    const std::string text =
        url.scheme() == toQt(kHelpActionScheme.substr(0, kHelpActionScheme.size() - 1))
            ? helpActionText(url)
            : fromQt(url.toString());
    switch (app::classifyHelpLink(text))
    {
        case app::HelpLinkKind::ACTION:
        {
            // The window stays on its page.
            const auto action = parseHelpAction(text);
            if (action)
            {
                if (onAction)
                {
                    onAction(*action);
                }
            }
            else if (onError)
            {
                onError(action.error());
            }
            return;
        }
        case app::HelpLinkKind::EXTERNAL:
            if (openExternal)
            {
                openExternal(url);
            }
            return;
        case app::HelpLinkKind::PAGE:
            m_browser->setSource(m_browser->source().resolved(url));
            return;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Index and search

void HelpWindow::filterIndex(const QString& text)
{
    for (int i = 0; i < m_index->count(); ++i)
    {
        QListWidgetItem* item = m_index->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void HelpWindow::search()
{
    m_results->clear();
    if (!loadBook())
    {
        return;
    }
    for (const app::HelpSearch::Hit& hit : m_search->find(
             fromQt(m_query->text()), m_caseSensitive->isChecked(), m_wholeWords->isChecked()))
    {
        auto* item = new QListWidgetItem(toQt(hit.title), m_results);
        item->setData(Qt::UserRole, toQt(hit.page));
    }
}

}  // namespace mandelbrotter::qt
