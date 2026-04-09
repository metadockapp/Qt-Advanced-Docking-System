#ifndef DockSplitterH
#define DockSplitterH
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
/// \file   DockSplitter.h
/// \author Uwe Kindler
/// \date   24.03.2017
/// \brief  Declaration of CDockSplitter
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include <QSplitter>
#include <QSplitterHandle>
#include <vector>
#include "windows.h"

#include "ads_globals.h"

namespace ads
{
struct DockSplitterPrivate;

// Forward declaration
class CDockSplitterHandle;

/**
 * Overlay widget that draws the splitter line above native windows
 */
class SplitterOverlay : public QWidget
{
	Q_OBJECT
private:
	CDockSplitterHandle* handle;
#ifdef Q_OS_WIN
	std::vector<HWND> hiddenWindows;
	void hideNativeWindows();
	void restoreNativeWindows();
	static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);
#endif
public:
	SplitterOverlay(CDockSplitterHandle* handle, QWidget* parent = nullptr);
	~SplitterOverlay();
	void updatePosition();
protected:
	void paintEvent(QPaintEvent *event) override;
};

/**
 * Custom splitter handle that stays above native windows during drag operations
 */
class ADS_EXPORT CDockSplitterHandle : public QSplitterHandle
{
	Q_OBJECT
	friend class SplitterOverlay;
private:
	SplitterOverlay* overlay;
	void createOverlay();
	void destroyOverlay();

public:
	CDockSplitterHandle(Qt::Orientation orientation, QSplitter *parent);
	~CDockSplitterHandle() override;

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
};

/**
 * Splitter used internally instead of QSplitter with some additional
 * functionality.
 */
class ADS_EXPORT CDockSplitter : public QSplitter
{
	Q_OBJECT
private:
	DockSplitterPrivate* d;
	friend struct DockSplitterPrivate;

protected:
	/**
	 * Override to create custom splitter handle that stays above native windows
	 */
	QSplitterHandle* createHandle() override;

public:
	CDockSplitter(QWidget *parent = Q_NULLPTR);
	CDockSplitter(Qt::Orientation orientation, QWidget *parent = Q_NULLPTR);

	/**
	 * Prints debug info
	 */
	virtual ~CDockSplitter();

	/**
	 * Returns true, if any of the internal widgets is visible
	 */
	bool hasVisibleContent() const;

	/**
	 * Returns first widget or nullptr if splitter is empty
	 */
	QWidget* firstWidget() const;

	/**
	 * Returns last widget of nullptr is splitter is empty
	 */
	QWidget* lastWidget() const;

    /**
     * Returns true if the splitter contains central widget of dock manager.
     */
    bool isResizingWithContainer() const;
}; // class CDockSplitter

} // namespace ads

//---------------------------------------------------------------------------
#endif // DockSplitterH
