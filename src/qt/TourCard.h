#pragma once

#include <QFrame>
#include <QLabel>
#include <QPaintEvent>
#include <QPushButton>
#include <QRect>
#include <QWidget>
#include <cstddef>
#include <functional>
#include <string_view>

namespace mandelbrotter::qt
{

/// The guided tour's speech bubble: title, text, "Step i of n" and Learn more / Close / Back /
/// Next. A child of the main window outside its layout, placed beside whatever the step points at
/// (a child widget needs no top-level positioning, which Wayland does not allow).
class TourCard : public QFrame
{
public:
    explicit TourCard(QWidget* parent);

    void setStep(std::string_view title, std::string_view text, std::size_t index,
                 std::size_t count, bool hasHelpPage);
    /// Places the card beside `anchor` (in its parent's coordinates): inside a large anchor such as
    /// the canvas, else to its left when there is room, else to its right (app::placeTourCard).
    void placeNear(const QRect& anchor);

    [[nodiscard]] QLabel&      titleLabel() const noexcept { return *m_title; }
    [[nodiscard]] QLabel&      textLabel() const noexcept { return *m_text; }
    [[nodiscard]] QLabel&      progressLabel() const noexcept { return *m_progress; }
    [[nodiscard]] QPushButton& learnMoreButton() const noexcept { return *m_learnMore; }
    [[nodiscard]] QPushButton& closeButton() const noexcept { return *m_close; }
    [[nodiscard]] QPushButton& backButton() const noexcept { return *m_back; }
    [[nodiscard]] QPushButton& nextButton() const noexcept { return *m_next; }

    std::function<void()> onBack;
    std::function<void()> onNext;
    std::function<void()> onClose;
    std::function<void()> onLearnMore;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void fitText();

    QLabel*      m_title{nullptr};
    QLabel*      m_text{nullptr};
    QLabel*      m_progress{nullptr};
    QPushButton* m_learnMore{nullptr};
    QPushButton* m_close{nullptr};
    QPushButton* m_back{nullptr};
    QPushButton* m_next{nullptr};
};

}  // namespace mandelbrotter::qt
