#include "dropdown_popup.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>

DropdownPopup::DropdownPopup(Mode mode, QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
    , mode_(mode)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
}

void DropdownPopup::setItems(const QStringList &items, int selectedIndex)
{
    items_ = items;
    selected_ = selectedIndex;
    hovered_ = -1;

    if (mode_ == Grid) {
        int cols = 3;
        int rows = (items.size() + cols - 1) / cols;
        int itemW = 72;
        int itemH = 40;
        setFixedSize(cols * itemW + 24, rows * itemH + 24);
    } else {
        // YearGrid: 4 cols × 3 rows = 12 per page
        int itemW = 64;
        int itemH = 40;
        int w = cols_ * itemW + 24;
        int h = rows_ * itemH + 24 + 32;  // extra for nav row
        setFixedSize(w, h);

        // center page on selected
        pageStart_ = (selected_ / perPage_) * perPage_;
        clampPage();
    }
    update();
}

void DropdownPopup::setThemeColor(const QColor &c)
{
    theme_ = c;
    update();
}

/* ---------- helpers ---------- */

QRect DropdownPopup::itemRect(int index) const
{
    if (index < 0 || index >= items_.size())
        return QRect();

    if (mode_ == Grid) {
        int cols = 3;
        int itemW = 72;
        int itemH = 40;
        int margin = 12;
        int col = index % cols;
        int row = index / cols;
        return QRect(margin + col * itemW, margin + row * itemH, itemW, itemH);
    } else {
        // YearGrid
        int pageIdx = index - pageStart_;
        if (pageIdx < 0 || pageIdx >= perPage_)
            return QRect();
        int itemW = 64;
        int itemH = 40;
        int margin = 12;
        int navH = 32;
        int col = pageIdx % cols_;
        int row = pageIdx / cols_;
        return QRect(margin + col * itemW, margin + navH + row * itemH, itemW, itemH);
    }
}

int DropdownPopup::itemAt(const QPoint &pt) const
{
    for (int i = 0; i < items_.size(); ++i) {
        if (itemRect(i).contains(pt))
            return i;
    }
    return -1;
}

QRect DropdownPopup::prevBtnRect() const
{
    return QRect(12, 6, 24, 22);
}

QRect DropdownPopup::nextBtnRect() const
{
    return QRect(width() - 36, 6, 24, 22);
}

void DropdownPopup::prevPage()
{
    pageStart_ -= perPage_;
    clampPage();
    update();
}

void DropdownPopup::nextPage()
{
    pageStart_ += perPage_;
    clampPage();
    update();
}

void DropdownPopup::clampPage()
{
    int totalPages = (items_.size() + perPage_ - 1) / perPage_;
    if (pageStart_ < 0) pageStart_ = 0;
    if (pageStart_ >= totalPages * perPage_) pageStart_ = (totalPages - 1) * perPage_;
}

/* ---------- paint ---------- */

void DropdownPopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // rounded white background with shadow
    QRectF r = rect().adjusted(2, 2, -2, -2);

    QPainterPath shadowPath;
    shadowPath.addRoundedRect(r, 10, 10);
    p.fillPath(shadowPath, QColor(0, 0, 0, 30));

    QRectF bgRect = r.adjusted(-1, -1, 1, 1);
    QPainterPath bgPath;
    bgPath.addRoundedRect(bgRect, 10, 10);
    p.fillPath(bgPath, Qt::white);

    p.setPen(QPen(QColor(230, 230, 230), 1));
    p.drawRoundedRect(bgRect, 10, 10);

    if (items_.isEmpty())
        return;

    QFont font(QString::fromUtf8("Microsoft YaHei"), 10);
    QFont selFont = font;
    selFont.setBold(true);

    // YearGrid: draw nav row
    if (mode_ == YearGrid) {
        // range label
        int lastIdx = qMin(pageStart_ + perPage_ - 1, items_.size() - 1);
        QString range = QString("%1 - %2")
            .arg(items_[pageStart_])
            .arg(items_[lastIdx]);

        p.setFont(QFont(QString::fromUtf8("Microsoft YaHei"), 9, QFont::Bold));
        p.setPen(QColor(100, 100, 100));
        p.drawText(QRect(prevBtnRect().right(), 6,
                         nextBtnRect().left() - prevBtnRect().right() - 4, 22),
                   Qt::AlignCenter, range);

        // prev/next buttons — drawn chevrons
        bool canPrev = pageStart_ > 0;
        bool canNext = pageStart_ + perPage_ < items_.size();

        auto drawChevron = [&p](const QRect &r, bool left, const QColor &c) {
            QPainterPath path;
            int cx = r.center().x(), cy = r.center().y();
            int w = 3, h = 5;
            if (left) {
                path.moveTo(cx + w, cy - h);
                path.lineTo(cx - w, cy);
                path.lineTo(cx + w, cy + h);
            } else {
                path.moveTo(cx - w, cy - h);
                path.lineTo(cx + w, cy);
                path.lineTo(cx - w, cy + h);
            }
            QPen pen(c, 2.0);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.drawPath(path);
        };

        drawChevron(prevBtnRect(), true,  canPrev ? QColor(80, 80, 80) : QColor(200, 200, 200));
        drawChevron(nextBtnRect(), false, canNext ? QColor(80, 80, 80) : QColor(200, 200, 200));
    }

    // draw items on current page
    int startIdx = (mode_ == YearGrid) ? pageStart_ : 0;
    int endIdx   = (mode_ == YearGrid)
                       ? qMin(pageStart_ + perPage_, items_.size())
                       : items_.size();

    for (int i = startIdx; i < endIdx; ++i) {
        QRect ir = itemRect(i);
        if (ir.isNull())
            continue;

        bool isSel = (i == selected_);
        bool isHover = (i == hovered_);

        if (isSel) {
            QPainterPath itemPath;
            itemPath.addRoundedRect(ir, 6, 6);
            p.fillPath(itemPath, theme_.lighter(230));
        } else if (isHover) {
            QPainterPath itemPath;
            itemPath.addRoundedRect(ir, 6, 6);
            p.fillPath(itemPath, QColor(245, 245, 250));
        }

        if (isSel) {
            p.setFont(selFont);
            p.setPen(theme_);
        } else {
            p.setFont(font);
            p.setPen(QColor(50, 50, 50));
        }
        p.drawText(ir, Qt::AlignCenter, items_[i]);
    }
}

/* ---------- mouse ---------- */

void DropdownPopup::mousePressEvent(QMouseEvent *e)
{
    if (mode_ == YearGrid) {
        if (prevBtnRect().contains(e->pos()) && pageStart_ > 0) {
            prevPage();
            return;
        }
        if (nextBtnRect().contains(e->pos()) && pageStart_ + perPage_ < items_.size()) {
            nextPage();
            return;
        }
    }

    int idx = itemAt(e->pos());
    if (idx >= 0) {
        emit itemSelected(idx);
        close();
    } else if (!rect().contains(e->pos())) {
        close();
    }
}

void DropdownPopup::mouseMoveEvent(QMouseEvent *e)
{
    int idx = itemAt(e->pos());
    if (idx != hovered_) {
        hovered_ = idx;
        update();
    }
}

void DropdownPopup::leaveEvent(QEvent *)
{
    hovered_ = -1;
    update();
}
