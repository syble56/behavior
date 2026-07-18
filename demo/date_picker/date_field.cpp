#include "date_field.h"
#include "calendar_picker.h"

#include <QMouseEvent>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>

// ── CalendarPopup ──
// Wraps CalendarPicker in a popup window (frameless, closes on day select).
class CalendarPopup : public QWidget
{
    Q_OBJECT
public:
    explicit CalendarPopup(const QDate &initialDate, QWidget *parent = nullptr)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground);

        auto *picker = new CalendarPicker(this);
        picker->setFixedWidth(360);
        picker->setMinimumHeight(400);
        picker->setDate(initialDate);
        picker->setDarkMode(true);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(picker);

        adjustSize();
        setFixedSize(picker->size());

        connect(picker, &CalendarPicker::dayClicked, this, [this](const QDate &d) {
            emit dateSelected(d);
            close();
        });
    }

    void setLanguage(CalendarPicker::Language lang)
    {
        auto *picker = findChild<CalendarPicker*>();
        if (picker) picker->setLanguage(lang);
    }

signals:
    void dateSelected(const QDate &date);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF r = rect().adjusted(2, 2, -2, -2);

        QPainterPath shadow;
        shadow.addRoundedRect(r, 12, 12);
        p.fillPath(shadow, QColor(0, 0, 0, 80));

        QRectF bgRect = r.adjusted(-1, -1, 1, 1);
        QPainterPath bg;
        bg.addRoundedRect(bgRect, 12, 12);
        p.fillPath(bg, QColor(26, 26, 46));
    }
};

// ── DateField ──

DateField::DateField(QWidget *parent)
    : QLabel(parent)
{
    setCursor(Qt::PointingHandCursor);
    setObjectName("dateField");
    setAlignment(Qt::AlignCenter);
    setFixedHeight(36);
    setMinimumWidth(120);
    updateText();
}

void DateField::setDate(const QDate &d)
{
    if (!d.isValid() || d == date_)
        return;
    date_ = d;
    updateText();
    emit dateChanged(date_);
}

void DateField::setLanguage(Language lang)
{
    lang_ = lang;
}

void DateField::showCalendar()
{
    auto *popup = new CalendarPopup(date_, this);
    popup->setLanguage(static_cast<CalendarPicker::Language>(lang_));
    connect(popup, &CalendarPopup::dateSelected, this, [this, popup](const QDate &d) {
        setDate(d);
        popup->deleteLater();
    });
    connect(popup, &CalendarPopup::destroyed, popup, &CalendarPopup::deleteLater);

    QPoint pos = mapToGlobal(
        QPoint(width() / 2 - popup->width() / 2, height() + 8));
    popup->move(pos);
    popup->setFocus();
    popup->show();
}

void DateField::updateText()
{
    setText(date_.toString(format_));
}

void DateField::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        showCalendar();
    QLabel::mousePressEvent(e);
}

#include "date_field.moc"
