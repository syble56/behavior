#include "calendar_picker.h"
#include "dropdown_popup.h"

#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDate>
#include <QVector>
#include <QPixmap>
#include <QIcon>

// ── Language helpers ──

static const char* cnWeekdays[] = {"一","二","三","四","五","六","日"};
static const char* enWeekdays[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};

static const char* cnMonths[] = {
    "1月","2月","3月","4月","5月","6月",
    "7月","8月","9月","10月","11月","12月"
};
static const char* enMonths[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
};

static QString monthLabel(int month, CalendarPicker::Language lang)
{
    if (lang == CalendarPicker::Chinese)
        return QString::fromUtf8(cnMonths[month - 1]);
    return QString::fromUtf8(enMonths[month - 1]);
}

static QString yearLabel(int year, CalendarPicker::Language lang)
{
    if (lang == CalendarPicker::Chinese)
        return QString("%1 年").arg(year);
    return QString::number(year);
}

// ── Icon helpers ──
static QIcon makeArrowIcon(Qt::Orientation dir, const QColor &color, int size = 24)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    int cx = size / 2, cy = size / 2;
    int w = 4, h = 7;  // chevron dimensions

    if (dir == Qt::Horizontal) {
        // left chevron: <
        path.moveTo(cx + w/2, cy - h);
        path.lineTo(cx - w/2, cy);
        path.lineTo(cx + w/2, cy + h);
    } else {
        // right chevron: >
        path.moveTo(cx - w/2, cy - h);
        path.lineTo(cx + w/2, cy);
        path.lineTo(cx - w/2, cy + h);
    }

    QPen pen(color, 2.2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPath(path);
    return QIcon(pm);
}

// Draw a small downward chevron icon for dropdown buttons
static QIcon makeDropdownChevron(const QColor &color, int size = 16)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    int cx = size / 2, cy = size / 2;
    int w = 4, h = 3;  // small chevron dimensions

    // downward chevron: ∨
    path.moveTo(cx - w, cy - h);
    path.lineTo(cx, cy + h);
    path.lineTo(cx + w, cy - h);

    QPen pen(color, 1.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPath(path);
    return QIcon(pm);
}

CalendarPicker::CalendarPicker(QWidget *parent)
    : QWidget(parent)
{
    date_ = QDate::currentDate();

    // ── header ──
    header_ = new QFrame(this);
    header_->setFixedHeight(48);
    header_->setObjectName("calHeader");

    prevBtn_ = new QToolButton(header_);
    prevBtn_->setIcon(makeArrowIcon(Qt::Horizontal, QColor(160, 160, 255), 24));
    prevBtn_->setIconSize(QSize(24, 24));
    nextBtn_ = new QToolButton(header_);
    nextBtn_->setIcon(makeArrowIcon(Qt::Vertical, QColor(160, 160, 255), 24));
    nextBtn_->setIconSize(QSize(24, 24));

    yearBtn_  = new QPushButton(header_);
    monthBtn_ = new QPushButton(header_);
    yearBtn_->setObjectName("yearBtn");
    monthBtn_->setObjectName("monthBtn");
    // icon on the right side (dropdown style)
    yearBtn_->setLayoutDirection(Qt::RightToLeft);
    monthBtn_->setLayoutDirection(Qt::RightToLeft);

    auto *hl = new QHBoxLayout(header_);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(4);
    hl->addWidget(prevBtn_);
    hl->addStretch();
    hl->addWidget(yearBtn_);
    hl->addSpacing(8);
    hl->addWidget(monthBtn_);
    hl->addStretch();
    hl->addWidget(nextBtn_);

    // ── grid widget (custom paint) ──
    gridWidget_ = new QWidget(this);
    gridWidget_->setMinimumHeight(300);
    gridWidget_->setAttribute(Qt::WA_TranslucentBackground);
    gridWidget_->setStyleSheet("background: transparent;");

    // ── root layout ──
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(header_);
    root->addWidget(gridWidget_, 1);

    // ── connections ──
    connect(prevBtn_,  &QToolButton::clicked, this, &CalendarPicker::prevMonth);
    connect(nextBtn_,  &QToolButton::clicked, this, &CalendarPicker::nextMonth);
    connect(yearBtn_,  &QPushButton::clicked, this, &CalendarPicker::showYearPopup);
    connect(monthBtn_, &QPushButton::clicked, this, &CalendarPicker::showMonthPopup);

    setThemeColor(theme_);
    updateHeader();
}

/* ---------- public ---------- */

void CalendarPicker::setDate(const QDate &date)
{
    if (!date.isValid() || date == date_)
        return;
    date_ = date;
    updateHeader();
    rebuildGrid();
    update();
    emit dateChanged(date_);
}

void CalendarPicker::setThemeColor(const QColor &c)
{
    theme_ = c;
    QString qss = QString(
        "QFrame#calHeader { background: %1; border: none; }"
        "QFrame#calHeader QToolButton { color: white; background: transparent;"
        "  border: none; font-size: 14px; padding: 4px 8px; border-radius: 4px; }"
        "QFrame#calHeader QToolButton:hover { background: rgba(255,255,255,40); }"
        "QFrame#calHeader QPushButton { color: white; background: transparent;"
        "  border: none; font-size: 16px; font-weight: bold; padding: 4px 10px;"
        "  border-radius: 4px; }"
        "QFrame#calHeader QPushButton:hover { background: rgba(255,255,255,40); }"
        ).arg(c.name());
    header_->setStyleSheet(qss);
    update();
}

void CalendarPicker::setDarkMode(bool dark)
{
    darkMode_ = dark;
    if (dark) {
        QString qss =
            "QFrame#calHeader { background: #1a1a2e; border: none; }"
            "QFrame#calHeader QToolButton { color: #a0a0ff; background: transparent;"
            "  border: none; font-size: 14px; padding: 4px 8px; border-radius: 4px; }"
            "QFrame#calHeader QToolButton:hover { background: #0f3460; }"
            "QFrame#calHeader QPushButton { color: #e0e0e0; background: transparent;"
            "  border: none; font-size: 16px; font-weight: bold; padding: 4px 10px;"
            "  border-radius: 4px; }"
            "QFrame#calHeader QPushButton:hover { background: #0f3460; }";
        header_->setStyleSheet(qss);
        theme_ = QColor(233, 69, 96);  // #e94560
    } else {
        setThemeColor(QColor(0, 0, 254));
    }
    update();
}

/* ---------- header ---------- */

void CalendarPicker::updateHeader()
{
    yearBtn_->setText(yearLabel(date_.year(), lang_));
    monthBtn_->setText(monthLabel(date_.month(), lang_));

    // set custom chevron icon instead of ▼ character
    QColor iconColor = darkMode_ ? QColor(160, 160, 200) : QColor(255, 255, 255);
    QIcon chevron = makeDropdownChevron(iconColor, 16);
    yearBtn_->setIcon(chevron);
    monthBtn_->setIcon(chevron);
    yearBtn_->setIconSize(QSize(16, 16));
    monthBtn_->setIconSize(QSize(16, 16));
}

void CalendarPicker::setLanguage(Language lang)
{
    lang_ = lang;
    updateHeader();
    update();
}

void CalendarPicker::prevMonth()
{
    setDate(date_.addMonths(-1));
}

void CalendarPicker::nextMonth()
{
    setDate(date_.addMonths(1));
}

/* ---------- popup selectors ---------- */

void CalendarPicker::showMonthPopup()
{
    DropdownPopup *popup = new DropdownPopup(DropdownPopup::Grid, this);
    popup->setThemeColor(theme_);

    QStringList months;
    for (int m = 1; m <= 12; ++m)
        months << monthLabel(m, lang_);
    popup->setItems(months, date_.month() - 1);

    connect(popup, &DropdownPopup::itemSelected, this, [this, popup](int idx){
        selectMonth(idx + 1);
        popup->deleteLater();
    });
    connect(popup, &DropdownPopup::destroyed, popup, &DropdownPopup::deleteLater);

    QPoint pos = monthBtn_->mapToGlobal(
        QPoint(monthBtn_->width()/2 - popup->width()/2, monthBtn_->height() + 4));
    popup->move(pos);
    popup->show();
}

void CalendarPicker::showYearPopup()
{
    DropdownPopup *popup = new DropdownPopup(DropdownPopup::YearGrid, this);
    popup->setThemeColor(theme_);

    QStringList years;
    int curYear = date_.year();
    int startYear = curYear - 50;
    int selIdx = 50;
    for (int y = startYear; y <= curYear + 50; ++y)
        years << QString::number(y);
    popup->setItems(years, selIdx);

    connect(popup, &DropdownPopup::itemSelected, this, [this, popup, startYear](int idx){
        selectYear(startYear + idx);
        popup->deleteLater();
    });

    QPoint pos = yearBtn_->mapToGlobal(
        QPoint(yearBtn_->width()/2 - popup->width()/2, yearBtn_->height() + 4));
    popup->move(pos);
    popup->show();
}

void CalendarPicker::selectMonth(int month)
{
    QDate d(date_.year(), month, qMin(date_.day(),
             QDate(date_.year(), month, 1).daysInMonth()));
    setDate(d);
}

void CalendarPicker::selectYear(int year)
{
    QDate d(year, date_.month(), qMin(date_.day(),
             QDate(year, date_.month(), 1).daysInMonth()));
    setDate(d);
}

/* ---------- grid ---------- */

void CalendarPicker::rebuildGrid()
{
    cells_.clear();
    cells_.resize(6);
    for (auto &row : cells_)
        row.resize(7);

    int year  = date_.year();
    int month = date_.month();

    QDate first(year, month, 1);
    int firstDayOfWeek = first.dayOfWeek();
    int daysInMonth = first.daysInMonth();
    int daysInPrev = first.addDays(-1).daysInMonth();

    int startCol = firstDayOfWeek - 1;
    int day = 1;
    int prevDay = daysInPrev - startCol + 1;

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            int idx = row * 7 + col;
            if (idx < startCol) {
                cells_[row][col].day = prevDay++;
                cells_[row][col].current = false;
            } else if (idx < startCol + daysInMonth) {
                cells_[row][col].day = day++;
                cells_[row][col].current = true;
            } else {
                cells_[row][col].day = day - daysInMonth;
                ++day;
                cells_[row][col].current = false;
            }
            cells_[row][col].selected =
                cells_[row][col].current && cells_[row][col].day == date_.day();
        }
    }
}

int CalendarPicker::cellAt(const QPoint &pt) const
{
    for (const auto &row : cells_)
        for (const auto &cell : row)
            if (cell.rect.contains(pt) && cell.current)
                return cell.day;
    return -1;
}

/* ---------- paint ---------- */

void CalendarPicker::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // dark mode background
    if (darkMode_) {
        p.fillRect(rect(), QColor(26, 26, 46));  // #1a1a2e
    }

    QRect gridRect = gridWidget_->geometry();
    int gx = gridRect.x();
    int gy = gridRect.y();
    int gw = gridRect.width();
    int gh = gridRect.height();

    static const char *weekdays[] = {"一","二","三","四","五","六","日"};
    const char **wd = (lang_ == Chinese) ? cnWeekdays : enWeekdays;
    int headerH = 32;
    int cellW = gw / 7;
    int cellH = (gh - headerH) / 6;

    // weekday labels
    p.setFont(QFont(QString::fromUtf8("Microsoft YaHei"), 10));
    QColor weekdayColor = darkMode_ ? QColor(102, 102, 102) : QColor(80, 80, 80);
    for (int i = 0; i < 7; ++i) {
        QRect r(gx + i * cellW, gy, cellW, headerH);
        p.setPen(weekdayColor);
        p.drawText(r, Qt::AlignCenter, QString::fromUtf8(wd[i]));
    }

    QColor lineColor = darkMode_ ? QColor(15, 52, 96) : QColor(220, 220, 220);
    p.setPen(QPen(lineColor, 1));
    p.drawLine(gx, gy + headerH, gx + gw, gy + headerH);

    QFont dayFont(QString::fromUtf8("Microsoft YaHei"), 11);
    QFont selFont(QString::fromUtf8("Microsoft YaHei"), 12);
    selFont.setBold(true);

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            int x = gx + col * cellW;
            int y = gy + headerH + row * cellH;
            QRect r(x, y, cellW, cellH);
            cells_[row][col].rect = r;

            const Cell &c = cells_[row][col];
            if (c.day == 0)
                continue;

            if (c.selected) {
                p.setBrush(theme_);
                p.setPen(Qt::NoPen);
                int sz = qMin(cellW, cellH) - 8;
                QRect circle(r.center().x() - sz/2, r.center().y() - sz/2, sz, sz);
                p.drawEllipse(circle);
                p.setBrush(Qt::NoBrush);

                p.setFont(selFont);
                p.setPen(Qt::white);
            } else {
                p.setFont(dayFont);
                if (!c.current) {
                    p.setPen(darkMode_ ? QColor(68, 68, 68) : QColor(200, 200, 200));
                } else {
                    p.setPen(darkMode_ ? QColor(192, 192, 192) : QColor(40, 40, 40));
                }
            }

            p.drawText(r, Qt::AlignCenter, QString::number(c.day));
        }
    }

    // today indicator
    QDate today = QDate::currentDate();
    if (today.year() == date_.year() && today.month() == date_.month()) {
        for (int row = 0; row < 6; ++row) {
            for (int col = 0; col < 7; ++col) {
                const Cell &c = cells_[row][col];
                if (c.current && c.day == today.day() && !c.selected) {
                    p.setBrush(theme_);
                    p.setPen(Qt::NoPen);
                    int dotSz = 4;
                    p.drawEllipse(c.rect.center().x() - dotSz/2,
                                  c.rect.bottom() - 10, dotSz, dotSz);
                    p.setBrush(Qt::NoBrush);
                }
            }
        }
    }
}

/* ---------- mouse ---------- */

void CalendarPicker::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        int day = cellAt(e->pos());
        if (day > 0) {
            date_.setDate(date_.year(), date_.month(), day);
            rebuildGrid();
            update();
            emit dateChanged(date_);
            emit dayClicked(date_);
        }
    }
    QWidget::mousePressEvent(e);
}
