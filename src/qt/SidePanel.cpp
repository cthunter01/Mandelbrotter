#include "qt/SidePanel.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>
#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/app/format.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/fractal.h"
#include "qt/JuliaPreview.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

/// The panel's content: the section boxes, with the tour's ring painted around one of them (in the
/// margin, where the boxes do not paint over it).
class SidePanel::Container : public QWidget
{
public:
    using QWidget::QWidget;

    QPointer<QGroupBox> highlighted;

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        if (highlighted.isNull())
        {
            return;
        }
        constexpr int kInflate = app::kSectionRingInflate;
        QPainter      painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(palette().color(QPalette::Highlight), app::kSectionRingWidth));
        painter.drawRoundedRect(
            QRectF(highlighted->geometry().adjusted(-kInflate, -kInflate, kInflate, kInflate)),
            app::kSectionRingRadius, app::kSectionRingRadius);
    }
};

namespace
{

constexpr std::size_t index(SidePanel::Section section)
{
    return static_cast<std::size_t>(section);
}

QString formatDouble(double value)
{
    return toQt(app::formatSeedComponent(value));
}

QFormLayout* formFor(QGroupBox& box)
{
    auto* form = new QFormLayout(&box);
    form->setVerticalSpacing(app::kRowGap * 2);
    form->setHorizontalSpacing(app::kLabelGap);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

}  // namespace

SidePanel::SidePanel(QWidget* parent) : QScrollArea(parent), m_container(new Container(this))
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    verticalScrollBar()->setSingleStep(app::kPanelScrollStep);

    auto* layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(app::kSectionBorder, app::kSectionBorder, app::kSectionBorder,
                               app::kSectionBorder);
    layout->setSpacing(app::kSectionBorder * 2);
    for (std::size_t i = 0; i < m_sections.size(); ++i)
    {
        m_sections.at(i) =
            new QGroupBox(toQt(app::sectionTitle(static_cast<Section>(i))), m_container);
        layout->addWidget(m_sections.at(i));
    }
    layout->addStretch(1);
    buildFractalSection();
    buildIterationSection();
    buildColoringSection();
    buildOverlaySection();
    buildBookmarkSection();
    setWidget(m_container);

    // Wide enough for the widest row plus the scroll bar; the height is whatever the dock gets.
    setMinimumWidth(m_container->minimumSizeHint().width() + app::kPanelWidthSlack);
    setMinimumHeight(app::kPanelMinHeight);
    setSettings(m_settings);
}

SidePanel::~SidePanel()
{
    // The controls die after this panel's members, and a focused line edit reports editingFinished
    // while it goes: nothing they report may reach the panel any more.
    for (const QObject* child : m_container->findChildren<QObject*>())
    {
        QObject::disconnect(child, nullptr, this, nullptr);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Construction

void SidePanel::buildFractalSection()
{
    QGroupBox&   box  = sectionBox(Section::FRACTAL);
    QFormLayout* form = formFor(box);
    Controls&    c    = m_controls;

    c.family = new QComboBox(&box);
    for (const FractalFamily family : allFamilies())
    {
        c.family->addItem(toQt(displayName(family)));
    }
    connect(c.family, &QComboBox::activated, this, [this](int row) {
        const auto families = allFamilies();
        if (row >= 0 && static_cast<std::size_t>(row) < families.size())
        {
            m_settings.fractal.family = families[static_cast<std::size_t>(row)];
            emitChange();
        }
    });
    form->addRow(toQt(app::kFamilyLabel), c.family);

    c.exponent = new QSpinBox(&box);
    c.exponent->setRange(kMinExponent, kMaxExponent);
    c.exponent->setKeyboardTracking(false);  // commit on Enter, the arrows or leaving the field
    connect(c.exponent, &QSpinBox::valueChanged, this, [this](int value) {
        m_settings.fractal.exponent = clampExponent(value);
        emitChange();
    });
    form->addRow(toQt(app::kExponentLabel), c.exponent);

    c.julia = new QCheckBox(toQt(app::kJuliaLabel), &box);
    connect(c.julia, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings.fractal.julia = checked;
        emitChange();
    });
    form->addRow(c.julia);

    auto* seedRow = new QHBoxLayout();
    c.seedRe      = new QLineEdit(&box);
    c.seedIm      = new QLineEdit(&box);
    seedRow->addWidget(new QLabel(toQt(app::kSeedPrefix), &box));
    seedRow->addWidget(c.seedRe, 1);
    seedRow->addWidget(new QLabel(toQt(app::kSeedPlus), &box));
    seedRow->addWidget(c.seedIm, 1);
    seedRow->addWidget(new QLabel(toQt(app::kSeedSuffix), &box));
    seedRow->setSpacing(app::kSeedGap);
    for (const QLineEdit* field : {c.seedRe, c.seedIm})
    {
        connect(field, &QLineEdit::editingFinished, this, [this] { readSeedFields(); });
    }
    form->addRow(seedRow);

    c.pickSeed = new QPushButton(toQt(app::kPickSeedLabel), &box);
    c.pickSeed->setCheckable(true);
    c.pickSeed->setToolTip(toQt(app::kPickSeedTip));
    connect(c.pickSeed, &QPushButton::clicked, this, [this](bool checked) {
        if (onPickSeedToggled)
        {
            onPickSeedToggled(checked);
        }
    });
    form->addRow(c.pickSeed);

    form->addRow(new QLabel(toQt(app::kPreviewLabel), &box));
    c.preview = new JuliaPreview(&box);
    form->addRow(c.preview);
}

void SidePanel::buildIterationSection()
{
    QGroupBox&   box  = sectionBox(Section::ITERATIONS);
    QFormLayout* form = formFor(box);
    Controls&    c    = m_controls;

    c.iterations = new QSpinBox(&box);
    c.iterations->setRange(kMinIterations, kMaxIterations);
    c.iterations->setKeyboardTracking(false);
    connect(c.iterations, &QSpinBox::valueChanged, this, [this](int value) {
        m_settings.maxIterations = value;
        emitChange();
    });
    form->addRow(toQt(app::kMaximumLabel), c.iterations);

    c.autoIterations = new QCheckBox(toQt(app::kAutoIterationsLabel), &box);
    connect(c.autoIterations, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings.autoIterations = checked;
        emitChange();
    });
    form->addRow(c.autoIterations);

    c.effectiveLimit = new QLabel(&box);
    form->addRow(c.effectiveLimit);
}

void SidePanel::buildColoringSection()
{
    QGroupBox&   box  = sectionBox(Section::COLORING);
    QFormLayout* form = formFor(box);
    Controls&    c    = m_controls;

    c.palette = new QComboBox(&box);
    for (const auto name : paletteNames())
    {
        c.palette->addItem(toQt(name));
    }
    connect(c.palette, &QComboBox::activated, this, [this](int row) {
        const auto names = paletteNames();
        if (row >= 0 && static_cast<std::size_t>(row) < names.size())
        {
            m_settings.coloring.palette = std::string(names[static_cast<std::size_t>(row)]);
            emitChange();
        }
    });
    form->addRow(toQt(app::kPaletteLabel), c.palette);

    c.density = new QDoubleSpinBox(&box);
    c.density->setRange(kMinDensity, kMaxDensity);
    c.density->setSingleStep(app::kDensityStep);
    c.density->setDecimals(app::kDensityDigits);
    c.density->setKeyboardTracking(false);
    c.density->setToolTip(toQt(app::kDensityTip));
    connect(c.density, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        m_settings.coloring.density = value;
        emitChange();
    });
    form->addRow(toQt(app::kDensityLabel), c.density);

    c.offset = new QSlider(Qt::Horizontal, &box);
    c.offset->setRange(0, app::kOffsetSliderSteps);
    c.offset->setToolTip(toQt(app::kOffsetTip));
    connect(c.offset, &QSlider::valueChanged, this, [this](int position) {
        m_settings.coloring.offset = app::sliderToOffset(position);
        emitChange();
    });
    form->addRow(toQt(app::kOffsetLabel), c.offset);
}

void SidePanel::buildOverlaySection()
{
    QGroupBox&   box  = sectionBox(Section::OVERLAY);
    QFormLayout* form = formFor(box);
    m_controls.orbit  = new QCheckBox(toQt(app::kShowOrbitLabel), &box);
    connect(m_controls.orbit, &QCheckBox::clicked, this, [this](bool checked) {
        if (onOrbitToggled)
        {
            onOrbitToggled(checked);
        }
    });
    form->addRow(m_controls.orbit);
}

void SidePanel::buildBookmarkSection()
{
    QGroupBox& box    = sectionBox(Section::BOOKMARKS);
    auto*      layout = new QVBoxLayout(&box);
    Controls&  c      = m_controls;

    c.bookmarks = new QListWidget(&box);
    c.bookmarks->setMinimumHeight(app::kBookmarkListMinHeight);
    c.bookmarks->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(c.bookmarks, &QListWidget::itemSelectionChanged, this, [this] { syncEnabledState(); });
    // Double-click, or Enter on the selected one.
    connect(c.bookmarks, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        const int row = m_controls.bookmarks->row(item);
        if (onBookmarkLoad && row >= 0)
        {
            onBookmarkLoad(static_cast<std::size_t>(row));
        }
    });
    layout->addWidget(c.bookmarks, 1);

    auto* buttons    = new QHBoxLayout();
    c.addBookmark    = new QPushButton(toQt(app::kAddBookmarkButton), &box);
    c.loadBookmark   = new QPushButton(toQt(app::kLoadBookmarkButton), &box);
    c.deleteBookmark = new QPushButton(toQt(app::kDeleteBookmarkButton), &box);
    c.addBookmark->setToolTip(toQt(app::kAddBookmarkTip));
    connect(c.addBookmark, &QPushButton::clicked, this, [this] {
        if (onBookmarkAdd)
        {
            onBookmarkAdd();
        }
    });
    connect(c.loadBookmark, &QPushButton::clicked, this, [this] {
        if (const auto row = selectedBookmark(); row && onBookmarkLoad)
        {
            onBookmarkLoad(*row);
        }
    });
    connect(c.deleteBookmark, &QPushButton::clicked, this, [this] {
        if (const auto row = selectedBookmark(); row && onBookmarkDelete)
        {
            onBookmarkDelete(*row);
        }
    });
    for (QPushButton* button : {c.addBookmark, c.loadBookmark, c.deleteBookmark})
    {
        buttons->addWidget(button, 1);
    }
    layout->addLayout(buttons);
}

// ---------------------------------------------------------------------------------------------------------------
// Model <-> controls

void SidePanel::setSettings(const RenderSettings& settings)
{
    m_settings        = settings;
    const Controls& c = m_controls;
    {
        // Qt, unlike wx, signals programmatic changes.
        const QSignalBlocker blockFamily(c.family);
        const QSignalBlocker blockExponent(c.exponent);
        const QSignalBlocker blockJulia(c.julia);
        const QSignalBlocker blockRe(c.seedRe);
        const QSignalBlocker blockIm(c.seedIm);
        const QSignalBlocker blockIterations(c.iterations);
        const QSignalBlocker blockAuto(c.autoIterations);
        const QSignalBlocker blockPalette(c.palette);
        const QSignalBlocker blockDensity(c.density);
        const QSignalBlocker blockOffset(c.offset);
        if (const auto family = app::familyIndex(settings.fractal.family))
        {
            c.family->setCurrentIndex(static_cast<int>(*family));
        }
        c.exponent->setValue(settings.fractal.exponent);
        c.julia->setChecked(settings.fractal.julia);
        c.seedRe->setText(formatDouble(settings.fractal.seed.re));
        c.seedIm->setText(formatDouble(settings.fractal.seed.im));
        c.iterations->setValue(settings.maxIterations);
        c.autoIterations->setChecked(settings.autoIterations);
        if (const auto palette = app::paletteIndex(settings.coloring.palette))
        {
            c.palette->setCurrentIndex(static_cast<int>(*palette));
        }
        c.density->setValue(settings.coloring.density);
        c.offset->setValue(app::offsetToSlider(settings.coloring.offset));
    }
    setEffectiveIterations(effectiveIterations(settings));

    c.preview->setFractal(settings.fractal);
    c.preview->setColoring(settings.coloring);
    if (settings.fractal.julia)
    {
        c.preview->setSeed(settings.fractal.seed);
    }
    syncEnabledState();
}

// Not const: it changes what the panel shows.
// NOLINTNEXTLINE(readability-make-member-function-const)
void SidePanel::setEffectiveIterations(int iterations)
{
    m_controls.effectiveLimit->setText(toQt(app::effectiveLimitText(iterations)));
}

void SidePanel::setBookmarks(std::span<const Bookmark> bookmarks)
{
    QListWidget&                     list     = *m_controls.bookmarks;
    const std::optional<std::size_t> selected = selectedBookmark();
    {
        const QSignalBlocker blocker(&list);
        list.clear();
        for (const Bookmark& bookmark : bookmarks)
        {
            list.addItem(toQt(bookmark.name));
        }
        if (selected && *selected < bookmarks.size())
        {
            list.setCurrentRow(static_cast<int>(*selected));
        }
    }
    syncEnabledState();
}

// Not const: it changes what the panel shows.
// NOLINTNEXTLINE(readability-make-member-function-const)
void SidePanel::setPickSeedMode(bool enabled)
{
    const QSignalBlocker blocker(m_controls.pickSeed);
    m_controls.pickSeed->setChecked(enabled);
}

// Not const: it changes what the panel shows.
// NOLINTNEXTLINE(readability-make-member-function-const)
void SidePanel::setShowOrbit(bool enabled)
{
    const QSignalBlocker blocker(m_controls.orbit);
    m_controls.orbit->setChecked(enabled);
}

// Not const: it changes what the panel shows.
// NOLINTNEXTLINE(readability-make-member-function-const)
void SidePanel::setPreviewSeed(std::optional<Complex> seed)
{
    m_controls.preview->setSeed(app::previewSeedFor(m_settings.fractal, seed));
}

void SidePanel::readSeedFields()
{
    const app::SeedEdit edit =
        app::commitSeedFields(fromQt(m_controls.seedRe->text()), fromQt(m_controls.seedIm->text()),
                              m_settings.fractal.seed);
    switch (edit.kind)
    {
        case app::SeedEdit::Kind::RESTORE:
        {
            // The last valid values rather than a guess.
            const QSignalBlocker blockRe(m_controls.seedRe);
            const QSignalBlocker blockIm(m_controls.seedIm);
            m_controls.seedRe->setText(formatDouble(edit.seed.re));
            m_controls.seedIm->setText(formatDouble(edit.seed.im));
            break;
        }
        case app::SeedEdit::Kind::UNCHANGED:
            break;
        case app::SeedEdit::Kind::APPLY:
            m_settings.fractal.seed = edit.seed;
            emitChange();
            break;
    }
}

void SidePanel::emitChange()
{
    syncEnabledState();
    if (onSettingsChanged)
    {
        onSettingsChanged(m_settings);
    }
}

void SidePanel::syncEnabledState()
{
    const bool hasSelection = selectedBookmark().has_value();
    m_controls.loadBookmark->setEnabled(hasSelection);
    m_controls.deleteBookmark->setEnabled(hasSelection);
}

std::optional<std::size_t> SidePanel::selectedBookmark() const
{
    const QList<QListWidgetItem*> selected = m_controls.bookmarks->selectedItems();
    if (selected.isEmpty())
    {
        return std::nullopt;
    }
    const int row = m_controls.bookmarks->row(selected.front());
    return row >= 0 ? std::optional<std::size_t>(static_cast<std::size_t>(row)) : std::nullopt;
}

// ---------------------------------------------------------------------------------------------------------------
// Sections (context help, the tour)

QGroupBox& SidePanel::sectionBox(Section section) const
{
    return *m_sections.at(index(section));
}

std::optional<SidePanel::Section> SidePanel::sectionOf(const QWidget* widget) const
{
    if (widget == nullptr)
    {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < m_sections.size(); ++i)
    {
        const QGroupBox* box = m_sections.at(i);
        if (box == widget || box->isAncestorOf(widget))
        {
            return static_cast<Section>(i);
        }
    }
    return std::nullopt;
}

bool SidePanel::isJuliaControl(const QWidget* widget) const
{
    const Controls& c = m_controls;
    return widget != nullptr && (widget == c.julia || widget == c.seedRe || widget == c.seedIm ||
                                 widget == c.pickSeed || widget == c.preview);
}

QRect SidePanel::sectionRect(Section section, const QWidget& ancestor) const
{
    const QGroupBox& box = sectionBox(section);
    return {box.mapTo(&ancestor, QPoint(0, 0)), box.size()};
}

void SidePanel::scrollToSection(Section section)
{
    const QGroupBox& box    = sectionBox(section);
    const int        scroll = verticalScrollBar()->value();
    const int        top    = box.y() - scroll;
    if (top >= 0 && top + box.height() <= viewport()->height())
    {
        return;  // already in view
    }
    verticalScrollBar()->setValue(std::max(0, box.y() - app::kScrollToMargin));
}

void SidePanel::setHighlightedSection(std::optional<Section> section)
{
    if (m_highlighted == section)
    {
        return;
    }
    m_highlighted            = section;
    m_container->highlighted = section ? &sectionBox(*section) : nullptr;
    m_container->update();
}

int SidePanel::contentHeight() const
{
    return m_container->minimumSizeHint().height();
}

}  // namespace mandelbrotter::qt
