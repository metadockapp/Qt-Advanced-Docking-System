/*******************************************************************************
** Qt Advanced Docking System
** Copyright (C) 2017 Uwe Kindler
** 
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
** 
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public
** License along with this library; If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/


//============================================================================
/// \file   DockSplitter.cpp
/// \author Uwe Kindler
/// \date   24.03.2017
/// \brief  Implementation of CDockSplitter
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include "DockSplitter.h"
#include "DockManager.h"

#include <QDebug>
#include <QChildEvent>
#include <QVariant>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QWindow>
#include "DockAreaWidget.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace ads
{

//============================================================================
// SplitterOverlay implementation
//============================================================================

SplitterOverlay::SplitterOverlay(CDockSplitterHandle* handle, QWidget* parent)
	: QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint),
	  handle(handle)
{
	this->setAttribute(Qt::WA_TranslucentBackground);
	this->setAttribute(Qt::WA_TransparentForMouseEvents);
	this->setAttribute(Qt::WA_ShowWithoutActivating);
	this->setAttribute(Qt::WA_NoSystemBackground);

#ifdef Q_OS_WIN
	// Ensure this window is created immediately so we can manipulate its HWND
	this->winId();

	// Use Windows API to force this window to be TOPMOST (above all other windows)
	if (this->windowHandle()) {
		HWND hwnd = reinterpret_cast<HWND>(this->windowHandle()->winId());
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

		// Also set the WS_EX_TOPMOST extended style
		LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED);

		// Set layered window attributes for transparency
		SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
	}

	// Temporarily hide all native child windows
	this->hideNativeWindows();
#endif
}

//============================================================================
SplitterOverlay::~SplitterOverlay()
{
#ifdef Q_OS_WIN
	// Restore all hidden native windows
	this->restoreNativeWindows();
#endif
}

#ifdef Q_OS_WIN
//============================================================================
BOOL CALLBACK SplitterOverlay::EnumChildProc(HWND hwnd, LPARAM lParam)
{
	SplitterOverlay* overlay = reinterpret_cast<SplitterOverlay*>(lParam);

	// Check if this is a visible native window (not a Qt widget window)
	if (IsWindowVisible(hwnd)) {
		wchar_t className[256];
		GetClassNameW(hwnd, className, 256);
		QString classStr = QString::fromWCharArray(className);

		// Look for WebView2 windows and other native embedded windows
		if (classStr.contains("Chrome", Qt::CaseInsensitive) ||
			classStr.contains("WebView", Qt::CaseInsensitive) ||
			classStr == "Internet Explorer_Server") {

			// Temporarily hide this window
			ShowWindow(hwnd, SW_HIDE);
			overlay->hiddenWindows.push_back(hwnd);
		}
	}

	return TRUE; // Continue enumeration
}

//============================================================================
void SplitterOverlay::hideNativeWindows()
{
	if (!this->handle || !this->handle->window()) {
		return;
	}

	// Get the main window HWND
	QWidget* mainWindow = this->handle->window();
	if (mainWindow->windowHandle()) {
		HWND mainHwnd = reinterpret_cast<HWND>(mainWindow->windowHandle()->winId());

		// Enumerate all child windows and hide native ones
		EnumChildWindows(mainHwnd, EnumChildProc, reinterpret_cast<LPARAM>(this));
	}
}

//============================================================================
void SplitterOverlay::restoreNativeWindows()
{
	// Restore all hidden windows
	for (HWND hwnd : this->hiddenWindows) {
		if (IsWindow(hwnd)) {
			ShowWindow(hwnd, SW_SHOW);
		}
	}
	this->hiddenWindows.clear();
}
#endif

//============================================================================
void SplitterOverlay::updatePosition()
{
	if (!this->handle || !this->handle->window()) {
		return;
	}

	// Position the overlay over the entire main window
	QWidget* mainWindow = this->handle->window();
	QPoint globalPos = mainWindow->mapToGlobal(QPoint(0, 0));
	this->setGeometry(globalPos.x(), globalPos.y(),
		mainWindow->width(), mainWindow->height());

#ifdef Q_OS_WIN
	// Re-assert TOPMOST status every time we update position
	if (this->windowHandle()) {
		HWND hwnd = reinterpret_cast<HWND>(this->windowHandle()->winId());
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
	}
#endif

	this->update();
}

//============================================================================
void SplitterOverlay::paintEvent(QPaintEvent *event)
{
	if (!this->handle) {
		return;
	}

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// Calculate handle position in overlay coordinates
	QWidget* mainWindow = this->handle->window();
	if (!mainWindow) {
		return;
	}

	QPoint globalPos = mainWindow->mapToGlobal(QPoint(0, 0));
	QRect handleRect = this->handle->geometry();
	QPoint handleGlobalPos = this->handle->mapToGlobal(handleRect.topLeft());
	QPoint handleLocalPos = QPoint(
		handleGlobalPos.x() - globalPos.x(),
		handleGlobalPos.y() - globalPos.y()
	);

	// Draw the splitter line
	painter.fillRect(QRect(handleLocalPos, handleRect.size()),
		QColor(128, 128, 128, 180));
}

//============================================================================
// CDockSplitterHandle implementation
//============================================================================

CDockSplitterHandle::CDockSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
	: QSplitterHandle(orientation, parent), overlay(nullptr)
{
}

//============================================================================
CDockSplitterHandle::~CDockSplitterHandle()
{
	this->destroyOverlay();
}

//============================================================================
void CDockSplitterHandle::createOverlay()
{
	if (this->overlay || !this->window()) {
		return;
	}

#ifdef Q_OS_WIN
	// Create a top-level transparent overlay window that sits above native HWNDs
	this->overlay = new SplitterOverlay(this);
	this->overlay->updatePosition();
	this->overlay->show();
#endif
}

//============================================================================
void CDockSplitterHandle::destroyOverlay()
{
	if (this->overlay) {
		this->overlay->deleteLater();
		this->overlay = nullptr;
	}
}

//============================================================================
void CDockSplitterHandle::mousePressEvent(QMouseEvent *event)
{
	QSplitterHandle::mousePressEvent(event);
	this->createOverlay();
}

//============================================================================
void CDockSplitterHandle::mouseMoveEvent(QMouseEvent *event)
{
	QSplitterHandle::mouseMoveEvent(event);

	// Update overlay position to track the handle as it moves
	if (this->overlay) {
		this->overlay->updatePosition();
	}
}

//============================================================================
void CDockSplitterHandle::mouseReleaseEvent(QMouseEvent *event)
{
	QSplitterHandle::mouseReleaseEvent(event);
	this->destroyOverlay();
}

//============================================================================
void CDockSplitterHandle::paintEvent(QPaintEvent *event)
{
	// Only paint if we don't have an overlay active
	if (!this->overlay) {
		QPainter painter(this);
		QStyleOption opt;
		opt.initFrom(this);
		opt.state |= QStyle::State_Horizontal;
		if (this->orientation() == Qt::Vertical) {
			opt.state &= ~QStyle::State_Horizontal;
		}

		// Draw the splitter handle - pass parent splitter so Qt's style engine
		// finds ::handle sub-control rules from the stylesheet
		this->style()->drawControl(QStyle::CE_Splitter, &opt, &painter, this->splitter());
	}
}

//============================================================================
// CDockSplitter implementation
//============================================================================
/**
 * Private dock splitter data
 */
struct DockSplitterPrivate
{
	CDockSplitter* _this;
	int VisibleContentCount = 0;

	DockSplitterPrivate(CDockSplitter* _public) : _this(_public) {}
};

//============================================================================
CDockSplitter::CDockSplitter(QWidget *parent)
	: QSplitter(parent),
	  d(new DockSplitterPrivate(this))
{
    setProperty("ads-splitter", QVariant(true));
	setChildrenCollapsible(false);
}


//============================================================================
CDockSplitter::CDockSplitter(Qt::Orientation orientation, QWidget *parent)
	: QSplitter(orientation, parent),
	  d(new DockSplitterPrivate(this))
{

}

//============================================================================
CDockSplitter::~CDockSplitter()
{
    ADS_PRINT("~CDockSplitter");
	delete d;
}


//============================================================================
bool CDockSplitter::hasVisibleContent() const
{
	// TODO Cache or precalculate this to speed up
	for (int i = 0; i < count(); ++i)
	{
		if (!widget(i)->isHidden())
		{
			return true;
		}
	}

	return false;
}


//============================================================================
QWidget* CDockSplitter::firstWidget() const
{
	return (count() > 0) ? widget(0) : nullptr;
}


//============================================================================
QWidget* CDockSplitter::lastWidget() const
{
	return (count() > 0) ? widget(count() - 1) : nullptr;
}

//============================================================================
bool CDockSplitter::isResizingWithContainer() const
{
    for (auto area : findChildren<CDockAreaWidget*>())
    {
        if(area->isCentralWidgetArea())
        {
            return true;
        }
    }

    return false;
}

//============================================================================
QSplitterHandle* CDockSplitter::createHandle()
{
	auto *handle = new CDockSplitterHandle(this->orientation(), this);

	// Walk up to the DockManager and emit splitterHandleCreated
	QWidget *parent = this->parentWidget();
	while (parent) {
		auto *manager = qobject_cast<CDockManager *>(parent);
		if (manager) {
			Q_EMIT manager->splitterHandleCreated(handle);
			break;
		}
		parent = parent->parentWidget();
	}

	return handle;
}

} // namespace ads

//---------------------------------------------------------------------------
// EOF DockSplitter.cpp
