#include "cleaner.h"
#include "database.h"
#include <QMetaObject>

namespace ui_shared {
namespace behavior {

Cleaner::Cleaner(QObject* parent)
    : QObject(parent) {}

Cleaner::~Cleaner() {
    stop();
}

void Cleaner::start() {
    if (thread_) return;
    thread_ = new QThread;
    this->moveToThread(thread_);
    thread_->start();
}

void Cleaner::stop() {
    if (!thread_) return;
    thread_->quit();
    thread_->wait();
    delete thread_;
    thread_ = nullptr;
}

void Cleaner::cleanAsync(int retentionDays) {
    if (!thread_) return;
    QMetaObject::invokeMethod(this, [this, retentionDays]() {
        doClean(retentionDays);
    }, Qt::QueuedConnection);
}

void Cleaner::doClean(int retentionDays) {
    int removed = Database::instance().cleanOldData(retentionDays);
    emit finished(removed);
}

} // namespace behavior
} // namespace ui_shared
