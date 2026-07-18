#ifndef DROPDOWN_POPUP_H
#define DROPDOWN_POPUP_H

#include <QWidget>
#include <QStringList>

/**
 * dropdown_popup — 自定义下拉弹出面板
 *
 * 两种模式：
 *   - Grid 模式：用于月份，3列×N行 网格
 *   - YearGrid 模式：用于年份，4列×3行 网格，◀▶ 切换年代
 */
class DropdownPopup : public QWidget
{
    Q_OBJECT

public:
    enum Mode { Grid, YearGrid };

    explicit DropdownPopup(Mode mode, QWidget *parent = nullptr);

    void setItems(const QStringList &items, int selectedIndex = -1);
    void setThemeColor(const QColor &c);

signals:
    void itemSelected(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    Mode mode_;
    QStringList items_;
    int selected_ = -1;
    int hovered_  = -1;
    QColor theme_ = QColor(0, 0, 254);

    // YearGrid pagination
    int pageStart_ = 0;   // first year index on current page
    int cols_ = 4;
    int rows_ = 3;
    int perPage_ = 12;

    QRect itemRect(int index) const;   // index into items_
    int  itemAt(const QPoint &pt) const;
    QRect prevBtnRect() const;
    QRect nextBtnRect() const;
    void prevPage();
    void nextPage();
    void clampPage();
};

#endif // DROPDOWN_POPUP_H
