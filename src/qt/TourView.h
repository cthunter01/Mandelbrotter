#pragma once

#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <cstddef>
#include <optional>

#include "Mandelbrotter/app/TourScript.h"
#include "qt/TourCard.h"

namespace mandelbrotter::qt
{

class MainWindow;

/// The guided tour on screen: app::TourScript runs the steps, this shows them. It places the step's
/// card beside its anchor (again whenever the window is resized), highlights the canvas or a side
/// panel section, and routes the card's buttons back to the script.
class TourView : public QObject
{
public:
    explicit TourView(MainWindow& window);
    ~TourView() override;
    TourView(const TourView&)            = delete;
    TourView& operator=(const TourView&) = delete;
    TourView(TourView&&)                 = delete;
    TourView& operator=(TourView&&)      = delete;

    // app::TourScript::Hooks
    void showCard(const app::TourStep& step, std::size_t index, std::size_t count);
    void hideCard();
    void highlight(std::optional<app::TourTarget> target);

    /// The card, while the tour runs.
    [[nodiscard]] TourCard* card() const noexcept { return m_card; }
    /// Where the card belongs: the anchor of the step shown, in the window's coordinates.
    [[nodiscard]] QRect anchorRect() const;

    /// The window was resized: the card is placed again.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void place();
    void learnMore();

    MainWindow&                    m_window;
    QPointer<TourCard>             m_card;
    std::optional<app::TourTarget> m_anchor;  ///< where the card sits, while it is shown
};

}  // namespace mandelbrotter::qt
