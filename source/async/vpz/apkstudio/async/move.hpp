#ifndef VPZ_APKSTUDIO_ASYNC_MOVE_HPP
#define VPZ_APKSTUDIO_ASYNC_MOVE_HPP

#include "helpers/adb.hpp"
#include "task.hpp"

namespace VPZ {
namespace APKStudio {
namespace Async {

class Move : public Task
{
    Q_OBJECT
private:
    QString destination;
    QString device;
    QStringList files;
public:
    explicit Move(const QString &, const QStringList &, const QString &, QObject * = 0);
    void start();
signals:
    void finished(QVariant, QStringList);
};

} // namespace Async
} // namespace APKStudio
} // namespace VPZ

#endif // VPZ_APKSTUDIO_ASYNC_MOVE_HPP
