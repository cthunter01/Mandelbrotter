#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::qt
{

class JuliaPreview;

/// The controls beside the canvas, in a scroll area (the main window puts it in a dock). Edits are
/// reported as complete RenderSettings through onSettingsChanged; setSettings() pushes the model
/// back into the controls without echoing.
class SidePanel : public QScrollArea
{
public:
    /// The panel's boxes, top to bottom.
    using Section = app::PanelSection;

    /// Its controls, for the main window (F1), the tour and the tests.
    struct Controls
    {
        QComboBox*      family{nullptr};
        QSpinBox*       exponent{nullptr};
        QCheckBox*      julia{nullptr};
        QLineEdit*      seedRe{nullptr};
        QLineEdit*      seedIm{nullptr};
        QPushButton*    pickSeed{nullptr};
        JuliaPreview*   preview{nullptr};
        QSpinBox*       iterations{nullptr};
        QCheckBox*      autoIterations{nullptr};
        QLabel*         effectiveLimit{nullptr};
        QComboBox*      palette{nullptr};
        QDoubleSpinBox* density{nullptr};
        QSlider*        offset{nullptr};
        QCheckBox*      orbit{nullptr};
        QListWidget*    bookmarks{nullptr};
        QPushButton*    addBookmark{nullptr};
        QPushButton*    loadBookmark{nullptr};
        QPushButton*    deleteBookmark{nullptr};
    };

    explicit SidePanel(QWidget* parent);
    ~SidePanel() override;
    SidePanel(const SidePanel&)            = delete;
    SidePanel& operator=(const SidePanel&) = delete;
    SidePanel(SidePanel&&)                 = delete;
    SidePanel& operator=(SidePanel&&)      = delete;

    void setSettings(const RenderSettings& settings);
    void setEffectiveIterations(int iterations);
    void setBookmarks(std::span<const Bookmark> bookmarks);
    void setPickSeedMode(bool enabled);
    void setShowOrbit(bool enabled);
    /// The seed the Julia preview follows (the hovered point, or the current seed in Julia mode).
    void setPreviewSeed(std::optional<Complex> seed);

    [[nodiscard]] const Controls& controls() const noexcept { return m_controls; }
    [[nodiscard]] QGroupBox&      sectionBox(Section section) const;
    /// The section a widget belongs to (F1, the tour); nullopt outside the boxes.
    [[nodiscard]] std::optional<Section> sectionOf(const QWidget* widget) const;
    /// True for the Julia controls of the Fractal section.
    [[nodiscard]] bool isJuliaControl(const QWidget* widget) const;
    /// A section's box in the coordinates of `ancestor` (the main window), as currently scrolled.
    [[nodiscard]] QRect sectionRect(Section section, const QWidget& ancestor) const;
    /// Scrolls the section's top into view unless it is fully visible already.
    void scrollToSection(Section section);
    /// The tour's accent ring around one section; nullopt removes it.
    void                                 setHighlightedSection(std::optional<Section> section);
    [[nodiscard]] std::optional<Section> highlightedSection() const noexcept
    {
        return m_highlighted;
    }
    /// The height of the whole panel's content, as it would show without scrolling.
    [[nodiscard]] int contentHeight() const;

    std::function<void(const RenderSettings&)> onSettingsChanged;
    std::function<void(bool)>                  onPickSeedToggled;
    std::function<void(bool)>                  onOrbitToggled;
    std::function<void()>                      onBookmarkAdd;
    std::function<void(std::size_t)>           onBookmarkLoad;
    std::function<void(std::size_t)>           onBookmarkDelete;

private:
    class Container;

    void buildFractalSection();
    void buildIterationSection();
    void buildColoringSection();
    void buildOverlaySection();
    void buildBookmarkSection();

    void readSeedFields();
    void emitChange();
    void syncEnabledState();
    /// The selected bookmark's index, if any.
    [[nodiscard]] std::optional<std::size_t> selectedBookmark() const;

    RenderSettings                                  m_settings;
    Container*                                      m_container{nullptr};
    std::array<QGroupBox*, app::kPanelSectionCount> m_sections{};
    std::optional<Section>                          m_highlighted;
    Controls                                        m_controls;
};

}  // namespace mandelbrotter::qt
