#ifndef CALENDAR_PICKER_H
#define CALENDAR_PICKER_H

#include <QWidget>
#include <QDate>
#include <QVector>
#include <QFrame>

class QLabel;
class QPushButton;
class QFrame;
class QToolButton;

/**
 * calendar_picker — 自定义日历选择器
 *
 * 布局：
 *   ┌────────────────────────────────────┐
 *   │ ◀  [年 ∨]  [月 ∨]  ▶              │  蓝色顶栏
 *   ├────────────────────────────────────┤
 *   │  一  二  三  四  五  六  日        │  星期标题
 *   ├────────────────────────────────────┤
 *   │  28  29  30  31  1   2   3         │
 *   │  4   5   6   7   8   9   10        │  日期网格
 *   │  ...                               │
 *   │  25  26  27  28  29  30  31        │
 *   └────────────────────────────────────┘
 *
 * 点击 [年∨] / [月∨] 弹出下拉列表选择。
 */
class CalendarPicker : public QWidget
{
    Q_OBJECT

public:
    enum Language { Chinese, English };

    explicit CalendarPicker(QWidget *parent = nullptr);

    QDate date() const { return date_; }
    void  setDate(const QDate &date);

    // 颜色定制
    void setThemeColor(const QColor &c);
    void setDarkMode(bool dark);

    // 语言
    void setLanguage(Language lang);
    Language language() const { return lang_; }

signals:
    void dateChanged(const QDate &date);
    void dayClicked(const QDate &date);  // only when a day cell is pressed

protected:
    void paintEvent(QPaintEvent *) override;

private slots:
    void prevMonth();
    void nextMonth();
    void selectMonth(int month);
    void selectYear(int year);

private:
    void updateHeader();
    void showMonthPopup();
    void showYearPopup();

    QDate date_;

    // header widgets
    QFrame    *header_;
    QToolButton *prevBtn_;
    QToolButton *nextBtn_;
    QPushButton *yearBtn_;
    QPushButton *monthBtn_;

    // grid area
    QWidget *gridWidget_;

    QColor theme_ = QColor(0, 0, 254);
    bool darkMode_ = false;
    Language lang_ = Chinese;

    // grid layout data
    struct Cell {
        int day;       // 1-31, or 0 for empty
        bool current;  // belongs to current month
        bool selected;
        QRect rect;
    };
    QVector<QVector<Cell>> cells_;  // [row][col]

    void rebuildGrid();
    int  cellAt(const QPoint &pt) const;  // returns day or -1

    // mouse
    void mousePressEvent(QMouseEvent *) override;
};

#endif // CALENDAR_PICKER_H
