/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "trayicon.h"

#include "iconutils.h"
#include "mainwindow.h"

#include <QAction>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>

#ifdef Q_OS_WIN
namespace {
// Windows 11 ignores the tray balloon that QSystemTrayIcon::showMessage sends,
// so the translation would never appear. A small on-screen window does.
QPointer<QWidget> translationToast;

void presentTranslationToast(const QString &message, int timeoutMs)
{
    QString shown = message.trimmed();
    if (shown.isEmpty())
        return;
    if (shown.size() > 1200)
        shown = shown.left(1200) + QStringLiteral("…");

    if (translationToast.isNull()) {
        auto *toast = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        toast->setAttribute(Qt::WA_ShowWithoutActivating);
        toast->setAttribute(Qt::WA_DeleteOnClose);
        const QPalette palette = QGuiApplication::palette();
        toast->setStyleSheet(QStringLiteral("QWidget { background: %1; color: %2; border: 1px solid %3; }"
                                            "QLabel#toastTitle { font-weight: 600; border: none; }"
                                            "QLabel#toastBody { border: none; }"
                                            "QPushButton#toastClose { border: none; background: transparent; font-size: 16px; padding: 0; }"
                                            "QPushButton#toastClose:hover { background: %4; }")
                                 .arg(palette.color(QPalette::Window).name(), palette.color(QPalette::WindowText).name(), palette.color(QPalette::Mid).name(), palette.color(QPalette::Midlight).name()));

        auto *layout = new QVBoxLayout(toast);
        layout->setContentsMargins(14, 10, 10, 12);
        auto *header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        auto *title = new QLabel(TrayIcon::tr("Translation result"), toast);
        title->setObjectName(QStringLiteral("toastTitle"));
        auto *closeButton = new QPushButton(QStringLiteral("×"), toast);
        closeButton->setObjectName(QStringLiteral("toastClose"));
        closeButton->setFixedSize(24, 24);
        closeButton->setFlat(true);
        closeButton->setFocusPolicy(Qt::NoFocus);
        closeButton->setCursor(Qt::PointingHandCursor);
        closeButton->setToolTip(TrayIcon::tr("Close"));
        QObject::connect(closeButton, &QPushButton::clicked, toast, &QWidget::close);
        header->addWidget(title, 1);
        header->addWidget(closeButton, 0, Qt::AlignTop);
        auto *body = new QLabel(toast);
        body->setObjectName(QStringLiteral("toastBody"));
        body->setWordWrap(true);
        body->setFixedWidth(400);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addLayout(header);
        layout->addWidget(body);

        auto *timer = new QTimer(toast);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, toast, &QWidget::close);
        translationToast = toast;
    }

    auto *body = translationToast->findChild<QLabel *>(QStringLiteral("toastBody"));
    if (body != nullptr) {
        body->setText(shown);
        body->setMinimumHeight(body->heightForWidth(body->width()));
    }
    translationToast->adjustSize();

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr)
        screen = QGuiApplication::primaryScreen();
    if (screen != nullptr) {
        const QRect area = screen->availableGeometry();
        translationToast->move(area.right() - translationToast->width() - 16, area.bottom() - translationToast->height() - 16);
    }

    translationToast->show();
    translationToast->raise();
    if (auto *timer = translationToast->findChild<QTimer *>())
        timer->start(qMax(1000, timeoutMs));
}
} // namespace
#endif

TrayIcon::TrayIcon(MainWindow *parent)
    : QSystemTrayIcon(parent)
    , m_trayMenu(new QMenu(parent))
    , m_showMainWindowAction(m_trayMenu->addAction(IconUtils::load(QStringLiteral(APPLICATION_ID "-tray")), tr("Show window"), parent, &MainWindow::open))
    , m_openSettingsAction(m_trayMenu->addAction(IconUtils::load(QStringLiteral("preferences-other")), tr("Settings"), parent, &MainWindow::openSettings))
    , m_quitAction(m_trayMenu->addAction(IconUtils::load(QStringLiteral("application-exit")), tr("Quit"), parent, &MainWindow::quit))
{
    m_reportBugAction = m_trayMenu->addAction(tr("Report Bug…"));
    connect(m_reportBugAction, &QAction::triggered, this, []() {
        QUrl url(QStringLiteral("https://bugs.kde.org/enter_bug.cgi"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("product"), QStringLiteral("Crow Translate"));
        query.addQueryItem(QStringLiteral("component"), QStringLiteral("general"));
        query.addQueryItem(QStringLiteral("format"), QStringLiteral("__default__"));
        query.addQueryItem(QStringLiteral("short_desc"), QString());
        query.addQueryItem(QStringLiteral("comment"),
                           QStringLiteral("Version: %1\nOperating system: %2\n\nWhat happened and what did you expect?\n").arg(QCoreApplication::applicationVersion(), QSysInfo::prettyProductName()));
        url.setQuery(query);
        QDesktopServices::openUrl(url);
    });

    setToolTip(APPLICATION_NAME);
    setContextMenu(m_trayMenu);

    connect(this, &TrayIcon::activated, [parent](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::Trigger)
            return;

        if (parent->isActiveWindow())
            parent->hide();
        else
            parent->open();
    });
}

void TrayIcon::setTranslationNotificationTimeout(int timeout)
{
    m_translationNotificaitonTimeout = timeout;
}

void TrayIcon::retranslateMenu()
{
    m_showMainWindowAction->setText(tr("Show window"));
    m_openSettingsAction->setText(tr("Settings"));
    m_reportBugAction->setText(tr("Report Bug…"));
    m_quitAction->setText(tr("Quit"));
}

void TrayIcon::showTranslationMessage(const QString &message)
{
#ifdef Q_OS_WIN
    presentTranslationToast(message, m_translationNotificaitonTimeout * 1000);
#else
    showMessage(tr("Translation result"), message, QSystemTrayIcon::NoIcon, m_translationNotificaitonTimeout * 1000);
#endif
}

QIcon TrayIcon::customTrayIcon(const QString &customName)
{
    if (QFileInfo::exists(customName))
        return QIcon(customName);
    return QIcon::fromTheme(customName);
}

QString TrayIcon::trayIconName(AppSettings::IconType type)
{
    switch (type) {
    case AppSettings::DefaultIcon:
        return QStringLiteral(APPLICATION_ID "-tray");
    case AppSettings::DarkIcon:
        return QStringLiteral(APPLICATION_ID "-tray-dark");
    case AppSettings::LightIcon:
        return QStringLiteral(APPLICATION_ID "-tray-light");
    default:
        return {};
    }
}
