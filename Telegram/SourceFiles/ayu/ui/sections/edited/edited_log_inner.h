// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "history/view/history_view_element.h"
#include "ayu/ui/sections/edited/edited_log_item.h"
#include "ayu/ui/sections/edited/edited_log_section.h"
#include "menu/menu_antispam_validator.h"
#include "ui/rp_widget.h"
#include "ui/effects/animations.h"
#include "ui/widgets/tooltip.h"
#include "mtproto/sender.h"
#include "base/timer.h"

struct ChatRestrictionsInfo;

namespace Main
{
class Session;
} // namespace Main

namespace HistoryView
{
class Element;
struct TextState;
struct StateRequest;
enum class CursorState : char;
enum class PointState : char;
} // namespace HistoryView

namespace Ui
{
class PopupMenu;
class ChatStyle;
struct PeerUserpicView;
} // namespace Ui

namespace Window
{
class SessionController;
} // namespace Window

namespace EditedLog
{

class SectionMemento;

class InnerWidget final
	: public Ui::RpWidget, public Ui::AbstractTooltipShower, public HistoryView::ElementDelegate
{
public:
	InnerWidget(
		QWidget *parent,
		not_null<Window::SessionController *> controller,
		not_null<PeerData *> peer,
		not_null<HistoryItem *> item);

	[[nodiscard]] Main::Session &session() const;
	[[nodiscard]] not_null<Ui::ChatTheme *> theme() const
	{
		return _theme.get();
	}

	[[nodiscard]] rpl::producer<int> scrollToSignal() const;

	[[nodiscard]] not_null<PeerData *> channel() const
	{
		return _peer;
	}

	// Set the correct scroll position after being resized.
	void restoreScrollPosition();

	void resizeToWidth(int newWidth, int minHeight)
	{
		_minHeight = minHeight;
		return TWidget::resizeToWidth(newWidth);
	}

	void saveState(not_null<SectionMemento *> memento);
	void restoreState(not_null<SectionMemento *> memento);

	// Ui::AbstractTooltipShower interface.
	QString tooltipText() const override;
	QPoint tooltipPos() const override;
	bool tooltipWindowActive() const override;

	// HistoryView::ElementDelegate interface.
	HistoryView::Context elementContext() override;
	bool elementUnderCursor(
		not_null<const HistoryView::Element *> view) override;
	[[nodiscard]] float64 elementHighlightOpacity(
		not_null<const HistoryItem *> item) const override;
	bool elementInSelectionMode() override;
	bool elementIntersectsRange(
		not_null<const HistoryView::Element *> view,
		int from,
		int till) override;
	void elementStartStickerLoop(
		not_null<const HistoryView::Element *> view) override;
	void elementShowPollResults(
		not_null<PollData *> poll,
		FullMsgId context) override;
	void elementOpenPhoto(
		not_null<PhotoData *> photo,
		FullMsgId context) override;
	void elementOpenDocument(
		not_null<DocumentData *> document,
		FullMsgId context,
		bool showInMediaView = false) override;
	void elementCancelUpload(const FullMsgId &context) override;
	void elementShowTooltip(
		const TextWithEntities &text,
		Fn<void()> hiddenCallback) override;
	bool elementAnimationsPaused() override;
	bool elementHideReply(
		not_null<const HistoryView::Element *> view) override;
	bool elementShownUnread(
		not_null<const HistoryView::Element *> view) override;
	void elementSendBotCommand(
		const QString &command,
		const FullMsgId &context) override;
	void elementHandleViaClick(not_null<UserData *> bot) override;
	bool elementIsChatWide() override;
	not_null<Ui::PathShiftGradient *> elementPathShiftGradient() override;
	void elementReplyTo(const FullReplyTo &to) override;
	void elementStartInteraction(
		not_null<const HistoryView::Element *> view) override;
	void elementStartPremium(
		not_null<const HistoryView::Element *> view,
		HistoryView::Element *replacing) override;
	void elementCancelPremium(
		not_null<const HistoryView::Element *> view) override;
	QString elementAuthorRank(
		not_null<const HistoryView::Element *> view) override;

	~InnerWidget();

protected:
	void visibleTopBottomUpdated(
		int visibleTop,
		int visibleBottom) override;

	void paintEvent(QPaintEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void enterEventHook(QEnterEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

	// Resizes content and counts natural widget height for the desired width.
	int resizeGetHeight(int newWidth) override;

private:
	using Element = HistoryView::Element;
	enum class Direction
	{
		Up,
		Down,
	};
	enum class MouseAction
	{
		None,
		PrepareDrag,
		Dragging,
		Selecting,
	};
	enum class EnumItemsDirection
	{
		TopToBottom,
		BottomToTop,
	};
	using TextState = HistoryView::TextState;
	using CursorState = HistoryView::CursorState;
	using PointState = HistoryView::PointState;
	using StateRequest = HistoryView::StateRequest;

	void mouseActionStart(const QPoint &screenPos, Qt::MouseButton button);
	void mouseActionUpdate(const QPoint &screenPos);
	void mouseActionFinish(const QPoint &screenPos, Qt::MouseButton button);
	void mouseActionCancel();
	void updateSelected();
	void performDrag();
	int itemTop(not_null<const Element *> view) const;
	void repaintItem(const Element *view);
	void refreshItem(not_null<const Element *> view);
	void resizeItem(not_null<Element *> view);
	QPoint mapPointToItem(QPoint point, const Element *view) const;

	void showContextMenu(QContextMenuEvent *e, bool showFromTouch = false);
	void savePhotoToFile(not_null<PhotoData *> photo);
	void saveDocumentToFile(not_null<DocumentData *> document);
	void copyContextImage(not_null<PhotoData *> photo);
	void showStickerPackInfo(not_null<DocumentData *> document);
	void cancelContextDownload(not_null<DocumentData *> document);
	void showContextInFolder(not_null<DocumentData *> document);
	void openContextGif(FullMsgId itemId);
	void copyContextText(FullMsgId itemId);
	void copySelectedText();
	TextForMimeData getSelectedText() const;

	void updateVisibleTopItem();
	void itemsAdded(Direction direction, int addedCount);
	void updateSize();
	void updateEmptyText();
	void paintEmpty(Painter &p, not_null<const Ui::ChatStyle *> st);
	void addEvents(Direction direction);
	Element *viewForItem(const HistoryItem *item);

	void toggleScrollDateShown();
	void repaintScrollDateCallback();
	bool displayScrollDate() const;
	void scrollDateHide();
	void scrollDateCheck();
	void scrollDateHideByTimer();

	// This function finds all history items that are displayed and calls template method
	// for each found message (in given direction) in the passed history with passed top offset.
	//
	// Method has "bool (*Method)(not_null<Element*> view, int itemtop, int itembottom)" signature
	// if it returns false the enumeration stops immidiately.
	template<EnumItemsDirection direction, typename Method>
	void enumerateItems(Method method);

	// This function finds all userpics on the left that are displayed and calls template method
	// for each found userpic (from the top to the bottom) using enumerateItems() method.
	//
	// Method has "bool (*Method)(not_null<Element*> view, int userpicTop)" signature
	// if it returns false the enumeration stops immediately.
	template<typename Method>
	void enumerateUserpics(Method method);

	// This function finds all date elements that are displayed and calls template method
	// for each found date element (from the bottom to the top) using enumerateItems() method.
	//
	// Method has "bool (*Method)(not_null<HistoryItem*> item, int itemtop, int dateTop)" signature
	// if it returns false the enumeration stops immediately.
	template<typename Method>
	void enumerateDates(Method method);

	const not_null<Window::SessionController *> _controller;
	const not_null<PeerData *> _peer;
	const not_null<HistoryItem *> _item;
	const not_null<History *> _history;
	MTP::Sender _api;

	const std::unique_ptr<Ui::PathShiftGradient> _pathGradient;
	std::shared_ptr<Ui::ChatTheme> _theme;

	std::vector<OwnedItem> _items;
	std::set<uint64> _eventIds;
	std::map<not_null<const HistoryItem *>, not_null<Element *>> _itemsByData;
	base::flat_map<not_null<const HistoryItem *>, TimeId> _itemDates;
	base::flat_set<FullMsgId> _animatedStickersPlayed;
	base::flat_map<not_null<PeerData *>, Ui::PeerUserpicView> _userpics;
	base::flat_map<not_null<PeerData *>, Ui::PeerUserpicView> _userpicsCache;
	int _itemsTop = 0;
	int _itemsWidth = 0;
	int _itemsHeight = 0;

	int _minHeight = 0;
	int _visibleTop = 0;
	int _visibleBottom = 0;
	Element *_visibleTopItem = nullptr;
	int _visibleTopFromItem = 0;

	bool _isChatWide = false;
	bool _scrollDateShown = false;
	Ui::Animations::Simple _scrollDateOpacity;
	SingleQueuedInvokation _scrollDateCheck;
	base::Timer _scrollDateHideTimer;
	Element *_scrollDateLastItem = nullptr;
	int _scrollDateLastItemTop = 0;

	// Don't load anything until the memento was read.
	bool _upLoaded = true;
	bool _downLoaded = true;
	bool _filterChanged = false;
	Ui::Text::String _emptyText;

	MouseAction _mouseAction = MouseAction::None;
	TextSelectType _mouseSelectType = TextSelectType::Letters;
	QPoint _dragStartPosition;
	QPoint _mousePosition;
	Element *_mouseActionItem = nullptr;
	CursorState _mouseCursorState = CursorState();
	uint16 _mouseTextSymbol = 0;
	bool _pressWasInactive = false;

	Element *_selectedItem = nullptr;
	TextSelection _selectedText;
	bool _wasSelectedText = false; // was some text selected in current drag action
	Qt::CursorShape _cursor = style::cur_default;

	base::unique_qptr<Ui::PopupMenu> _menu;

	QPoint _trippleClickPoint;
	base::Timer _trippleClickTimer;

	rpl::event_stream<int> _scrollToSignal;

};

} // namespace EditedLog
