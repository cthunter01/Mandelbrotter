#pragma once

#include <QAction>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/app/help_sitemap.h"
#include "Mandelbrotter/help_action.h"

namespace mandelbrotter::qt
{

/// A "mandelbrotter:" link as QTextBrowser reports it, back as the help book wrote it
/// ("mandelbrotter:view --zoom 5000"): QUrl refuses to print a fully decoded URL whole, so it is
/// rebuilt from its decoded path.
[[nodiscard]] std::string helpActionText(const QUrl& url);

/// The help window: the book embedded in the resources (:/help/), with a contents tree, an index,
/// full-text search (app/help_sitemap.h), back and forward, up, previous and next page, printing
/// and the text size. Links of the form "mandelbrotter:..." are not pages but actions on the main
/// window; they are reported through onAction. An independent top-level window that does not keep
/// the application running.
class HelpWindow : public QMainWindow
{
public:
    HelpWindow();

    /// Shows the book's first page and raises the window.
    void showContents();
    /// Opens a page of the book by file name ("navigating.html", "navigating.html#mouse") and
    /// raises the window; a page the book does not have opens the contents.
    void showPage(std::string_view page);
    /// The page shown, with its anchor if it has one ("navigating.html#mouse").
    [[nodiscard]] std::string currentPage() const;

    // ---- the parts (the tests)
    [[nodiscard]] QTextBrowser& browser() const noexcept { return *m_browser; }
    [[nodiscard]] QTreeWidget&  contentsTree() const noexcept { return *m_contents; }
    [[nodiscard]] QLineEdit&    indexFilter() const noexcept { return *m_indexFilter; }
    [[nodiscard]] QListWidget&  indexList() const noexcept { return *m_index; }
    [[nodiscard]] QPushButton&  showAllButton() const noexcept { return *m_showAll; }
    [[nodiscard]] QLineEdit&    searchField() const noexcept { return *m_query; }
    [[nodiscard]] QCheckBox&    caseSensitiveBox() const noexcept { return *m_caseSensitive; }
    [[nodiscard]] QCheckBox&    wholeWordsBox() const noexcept { return *m_wholeWords; }
    [[nodiscard]] QListWidget&  searchResults() const noexcept { return *m_results; }
    [[nodiscard]] QTabWidget&   navigation() const noexcept { return *m_tabs; }
    [[nodiscard]] QAction&      backAction() const noexcept { return *m_back; }
    [[nodiscard]] QAction&      forwardAction() const noexcept { return *m_forward; }
    [[nodiscard]] QAction&      upAction() const noexcept { return *m_up; }
    [[nodiscard]] QAction&      previousAction() const noexcept { return *m_previous; }
    [[nodiscard]] QAction&      nextAction() const noexcept { return *m_next; }
    /// Runs the search tab's query.
    void search();

    std::function<void(const HelpAction&)>  onAction;
    std::function<void(const std::string&)> onError;  ///< a malformed action link, a missing book
    /// Opens an http(s) link: the default browser, unless a test puts itself there.
    std::function<void(const QUrl&)> openExternal;

private:
    /// A contents entry in reading order, with its parent's position in that order.
    struct Entry
    {
        std::string name;
        std::string local;
        int         parent{-1};
    };

    bool loadBook();
    void buildNavigation();
    void buildToolBar();
    void addTopics(QTreeWidgetItem* parent, const std::vector<app::HelpTopic>& topics,
                   int parentEntry);
    void onLinkClicked(const QUrl& url);
    void onSourceChanged(const QUrl& source);
    /// "navigating.html#mouse" for qrc:/help/navigating.html#mouse.
    [[nodiscard]] static std::string pageOf(const QUrl& source);
    /// The contents entry a page belongs to (its own, or the first of its file), or -1.
    [[nodiscard]] int entryOf(std::string_view page) const;
    void              filterIndex(const QString& text);
    /// The contents entry the page shown belongs to, or -1.
    [[nodiscard]] int currentEntry() const;
    void              showEntry(int entry);

    QTextBrowser*                    m_browser{nullptr};
    QTabWidget*                      m_tabs{nullptr};
    QTreeWidget*                     m_contents{nullptr};
    QLineEdit*                       m_indexFilter{nullptr};
    QListWidget*                     m_index{nullptr};
    QPushButton*                     m_showAll{nullptr};
    QLineEdit*                       m_query{nullptr};
    QCheckBox*                       m_caseSensitive{nullptr};
    QCheckBox*                       m_wholeWords{nullptr};
    QListWidget*                     m_results{nullptr};
    QAction*                         m_back{nullptr};
    QAction*                         m_forward{nullptr};
    QAction*                         m_up{nullptr};
    QAction*                         m_previous{nullptr};
    QAction*                         m_next{nullptr};
    app::HelpProject                 m_project;
    std::vector<Entry>               m_entries;
    std::unique_ptr<app::HelpSearch> m_search;
    bool                             m_loaded{false};
};

}  // namespace mandelbrotter::qt
