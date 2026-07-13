#pragma once

#include <QDialog>

class SettingsDialog : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
};

class SystemButtonsDialog : public QDialog {
public:
    explicit SystemButtonsDialog(QWidget* parent = nullptr);
};
