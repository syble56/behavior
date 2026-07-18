#ifndef DATE_FIELD_H
#define DATE_FIELD_H

#include <QLabel>
#include <QDate>

class CalendarPopup;

// DateField — a clickable date label that pops up a custom calendar picker.
// API mirrors QDateEdit for easy substitution.
class DateField : public QLabel
{
    Q_OBJECT
public:
    enum Language { Chinese, English };

    explicit DateField(QWidget *parent = nullptr);

    QDate date() const { return date_; }
    void setDate(const QDate &d);
    void setDisplayFormat(const QString &fmt) { format_ = fmt; updateText(); }
    void setLanguage(Language lang);

signals:
    void dateChanged(const QDate &date);

protected:
    void mousePressEvent(QMouseEvent *e) override;

private slots:
    void showCalendar();

private:
    void updateText();

    QDate date_ = QDate::currentDate();
    QString format_ = "yyyy-MM-dd";
    Language lang_ = Chinese;
};

#endif // DATE_FIELD_H
