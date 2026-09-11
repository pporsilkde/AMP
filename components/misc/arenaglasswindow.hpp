#ifndef OPENMW_COMPONENTS_MISC_ARENAGLASSWINDOW_H
#define OPENMW_COMPONENTS_MISC_ARENAGLASSWINDOW_H

#include <QApplication>
#include <QLabel>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSizeGrip>
#include <QToolButton>
#include <QWindow>
#include <QWizard>
#include <QLayout>
#include <QPointer>
#include <QLibrary>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <qt_windows.h>
#endif

namespace ArenaUi
{
    // No private Windows composition APIs. Unsupported systems retain the
    // opaque painted material. No new link dependency on dwmapi is required.
    inline bool requestSystemGlass(QWidget* window)
    {
#ifdef Q_OS_WIN
        static QLibrary dwm(QStringLiteral("dwmapi"));
        using SetAttribute = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
        const auto setAttribute = reinterpret_cast<SetAttribute>(dwm.resolve("DwmSetWindowAttribute"));
        if (setAttribute)
        {
            const HWND hwnd = reinterpret_cast<HWND>(window->winId());
            const int dark = 1, rounded = 2, acrylic = 3;
            setAttribute(hwnd, 20, &dark, sizeof(dark));
            setAttribute(hwnd, 33, &rounded, sizeof(rounded));
            // DWMWA_SYSTEMBACKDROP_TYPE / DWMSBT_TRANSIENTWINDOW (Win11 22H2+).
            return SUCCEEDED(setAttribute(hwnd, 38, &acrylic, sizeof(acrylic)));
        }
#else
        Q_UNUSED(window)
#endif
        return false;
    }

    class GlassTitleBar : public QWidget
    {
        QWidget* mWindow;
        QPoint mOffset;
        bool mDragging = false;
    public:
        explicit GlassTitleBar(QWidget* window) : QWidget(window), mWindow(window)
        {
            setObjectName(QStringLiteral("arenaGlassTitleBar"));
            auto* row = new QHBoxLayout(this);
            row->setContentsMargins(16, 5, 16, 5);
            row->setSpacing(8);
            auto button = [this, row](const QString& name, const QString& text, const QString& label) {
                auto* b = new QToolButton(this);
                b->setObjectName(name);
                b->setText(text);
                b->setFixedSize(24, 24);
                b->setToolTip(label);
                b->setAccessibleName(label);
                row->addWidget(b);
                return b;
            };
            auto* close = button(QStringLiteral("arenaClose"), QStringLiteral("×"), tr("Close"));
            auto* minimize = button(QStringLiteral("arenaMinimize"), QStringLiteral("−"), tr("Minimize"));
            auto* maximize = button(QStringLiteral("arenaMaximize"), QStringLiteral("+"), tr("Maximize / restore"));
            minimize->setVisible(window->windowFlags().testFlag(Qt::WindowMinimizeButtonHint));
            maximize->setVisible(window->minimumSize() != window->maximumSize()
                && window->windowFlags().testFlag(Qt::WindowMaximizeButtonHint));
            connect(close, &QToolButton::clicked, window, [window]() { window->close(); });
            connect(minimize, &QToolButton::clicked, window, &QWidget::showMinimized);
            connect(maximize, &QToolButton::clicked, window, [window]() {
                if (window->isMaximized()) window->showNormal(); else window->showMaximized();
            });
            auto* title = new QLabel(window->windowTitle(), this);
            title->setObjectName(QStringLiteral("arenaWindowTitle"));
            title->setAttribute(Qt::WA_TransparentForMouseEvents);
            title->setAlignment(Qt::AlignCenter);
            row->addWidget(title, 1);
            row->addSpacing(32);
            connect(window, &QWidget::windowTitleChanged, title, &QLabel::setText);
        }
    protected:
        void mousePressEvent(QMouseEvent* event) override
        {
            if (event->button() != Qt::LeftButton) return;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            if (mWindow->windowHandle() && mWindow->windowHandle()->startSystemMove()) return;
#endif
            if (!mWindow->isMaximized())
            {
                mDragging = true;
                mOffset = event->globalPos() - mWindow->frameGeometry().topLeft();
            }
        }
        void mouseMoveEvent(QMouseEvent* event) override
        {
            if (mDragging && (event->buttons() & Qt::LeftButton))
                mWindow->move(event->globalPos() - mOffset);
        }
        void mouseReleaseEvent(QMouseEvent*) override { mDragging = false; }
        void mouseDoubleClickEvent(QMouseEvent* event) override
        {
            mDragging = false;
            if (event->button() == Qt::LeftButton && mWindow->minimumSize() != mWindow->maximumSize()
                && mWindow->windowFlags().testFlag(Qt::WindowMaximizeButtonHint))
            {
                if (mWindow->isMaximized()) mWindow->showNormal(); else mWindow->showMaximized();
            }
        }
    };

    class GlassWindowMaterial : public QWidget
    {
        QWidget* mWindow;
        GlassTitleBar* mTitle;
        QSizeGrip* mGrip;
        bool mSystemGlass = false;
        QPointer<QWidget> mWizardBody;
        void arrange()
        {
            // QWizard sizes a private body directly against rect(), bypassing
            // QWidget contentsMargins. Keep that body's existing layout inside
            // the reserved content area; its own children remain untouched.
            if (mWizardBody && mWizardBody->geometry() != mWindow->contentsRect())
                mWizardBody->setGeometry(mWindow->contentsRect());
            setGeometry(mWindow->rect());
            lower();
            mTitle->setGeometry(6, 6, qMax(0, width() - 12), 42);
            mTitle->raise();
            mGrip->move(width() - 24, height() - 24);
            mGrip->setVisible(!mWindow->isMaximized() && mWindow->minimumSize() != mWindow->maximumSize());
            mGrip->raise();
            update();
        }
    public:
        explicit GlassWindowMaterial(QWidget* window)
            : QWidget(window), mWindow(window), mTitle(new GlassTitleBar(window)), mGrip(new QSizeGrip(window))
        {
            setAttribute(Qt::WA_TransparentForMouseEvents);
            setAttribute(Qt::WA_NoSystemBackground);
            mGrip->setFixedSize(18, 18);
            if (qobject_cast<QWizard*>(window))
            {
                for (auto* child : window->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
                    if (child != this && child != mTitle && child != mGrip)
                    {
                        mWizardBody = child;
                        child->installEventFilter(this);
                        break;
                    }
            }
            window->installEventFilter(this);
            arrange();
        }
    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (mWizardBody && watched == mWizardBody
                && (event->type() == QEvent::Resize || event->type() == QEvent::Move))
                arrange();
            if (watched == mWindow)
            {
                if (event->type() == QEvent::Resize || event->type() == QEvent::WindowStateChange
                    || event->type() == QEvent::LayoutRequest)
                    arrange();
                if (event->type() == QEvent::Show)
                {
                    mSystemGlass = requestSystemGlass(mWindow);
                    show();
                    mTitle->show();
                    arrange();
                }
            }
            return QWidget::eventFilter(watched, event);
        }
        void paintEvent(QPaintEvent*) override
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            const QRectF panel = QRectF(rect()).adjusted(6, 6, -6, -6);
            const qreal radius = mWindow->isMaximized() ? 0 : 18;
            // A cheap static shadow; no full-window live GPU blur pass.
            for (int i = 5; i > 0; --i)
            {
                p.setPen(QPen(QColor(0, 0, 0, 12), 2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(panel.adjusted(-i, -i, i, i), radius + i, radius + i);
            }
            QLinearGradient material(panel.topLeft(), panel.bottomRight());
            // Dense tint stays readable if the compositor declines acrylic.
            material.setColorAt(0, QColor(49, 48, 47, mSystemGlass ? 240 : 255));
            material.setColorAt(.48, QColor(27, 28, 31, mSystemGlass ? 244 : 255));
            material.setColorAt(1, QColor(29, 25, 22, 255));
            p.setPen(QPen(QColor(232, 215, 178, 65), 1));
            p.setBrush(material);
            p.drawRoundedRect(panel, radius, radius);
            QPainterPath clip;
            clip.addRoundedRect(panel, radius, radius);
            p.setClipPath(clip);
            QRadialGradient amber(QPointF(width() * .12, 0), width() * .7);
            amber.setColorAt(0, QColor(197, 150, 73, 44));
            amber.setColorAt(1, Qt::transparent);
            p.fillRect(panel, amber);
            QRadialGradient blue(QPointF(width(), height() * .55), width() * .65);
            blue.setColorAt(0, QColor(111, 140, 160, 23));
            blue.setColorAt(1, Qt::transparent);
            p.fillRect(panel, blue);
            p.setPen(QColor(232, 215, 178, 27));
            p.drawLine(22, 48, width() - 22, 48);
        }
    };

    // Call once after constructing a top-level window, before its first show.
    // Keeping the original widget preserves modality, signals, closeEvent and
    // updater cancellation/transaction guards. No wrapper window is introduced.
    inline void installGlassWindow(QWidget& window)
    {
        if (window.property("arenaGlassWindow").toBool()) return;
        window.setProperty("arenaGlassWindow", true);
        window.setWindowFlag(Qt::FramelessWindowHint);
        window.setAttribute(Qt::WA_TranslucentBackground);
        const QMargins old = window.contentsMargins();
        window.setContentsMargins(old.left() + 12, old.top() + 52, old.right() + 12, old.bottom() + 12);
        new GlassWindowMaterial(&window);
    }
}
#endif
