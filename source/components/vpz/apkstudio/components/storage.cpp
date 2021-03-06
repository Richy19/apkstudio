#include "storage.hpp"

using namespace VPZ::APKStudio::Async;
using namespace VPZ::APKStudio::Helpers;
using namespace VPZ::APKStudio::Resources;

namespace VPZ {
namespace APKStudio {
namespace Components {

Storage::Storage(const QString &device, QWidget *parent) :
    QWidget(parent), device(device)
{
    path = new Clearable(this);
    tree = new TreeWidget(true, true, this);
    QLayout *layout = new QVBoxLayout(this);
    QStringList labels;
    labels << translate("header_name");
    labels << translate("header_size");
    labels << translate("header_permissions");
    labels << translate("header_owner");
    labels << translate("header_group");
    labels << translate("header_modified");
    layout->addWidget(path);
    layout->addWidget(tree);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    connections.append(connect(tree, &TreeWidget::doubleClicked, this, &Storage::onDoubleClicked));
    connections.append(connect(tree, &TreeWidget::itemsDropped, this, &Storage::onItemsDropped));
    connections.append(connect(tree, &TreeWidget::filesDropped, this, &Storage::onFilesDropped));
    tree->setHeaderLabels(labels);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->setRootIsDecorated(false);
    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setSortingEnabled(false);
    tree->setUniformRowHeights(true);
    tree->setColumnWidth(0, 160);
    tree->setColumnWidth(1, 96);
    tree->setColumnWidth(2, 96);
    tree->setColumnWidth(3, 96);
    tree->setColumnWidth(4, 96);
    tree->setColumnWidth(5, 128);
    setLayout(layout);
    // -- //
    new QShortcut(QKeySequence::Copy, this, SLOT(onCopy()));
    new QShortcut(QKeySequence::New, this, SLOT(onCreate()));
    new QShortcut(QKeySequence(Qt::Key_F6), this, SLOT(onEdit()));
    new QShortcut(QKeySequence::Cut, this, SLOT(onMove()));
    new QShortcut(QKeySequence::Refresh, this, SLOT(onRefresh()));
    new QShortcut(QKeySequence(Qt::Key_Delete), this, SLOT(onRemove()));
    new QShortcut(QKeySequence(Qt::Key_F2), this, SLOT(onRename()));
    new QShortcut(QKeySequence(Qt::Key_Return), this, SLOT(onReturn()));
    new QShortcut(QKeySequence(Qt::Key_Backspace), this, SLOT(onUp()));
}

void Storage::onAction(QAction *action)
{
    switch (action->data().toInt())
    {
    case ACTION_CHMOD:
        onCHMOD();
        break;
    case ACTION_COPY:
        onCopy();
        break;
    case ACTION_CREATE:
        onCreate();
        break;
    case ACTION_DETAILS:
        onDetails();
        break;
    case ACTION_MOVE:
        onMove();
        break;
    case ACTION_PULL:
        onPull();
        break;
    case ACTION_PUSH:
        onPush();
        break;
    case ACTION_REMOVE:
        onRemove();
        break;
    case ACTION_RENAME:
        onRename();
        break;
    }
}

void Storage::onCHMOD()
{
    QVector<File> files = selected();
    if (files.isEmpty() || files.count() != 1)
        return;
    emit showCHMOD(files.first());
}

void Storage::onCopy()
{
    if (path->hasFocus())
        return;
    QVector<File> files = selected();
    if (files.isEmpty())
        return;
    bool ok = false;
    QString destination = QInputDialog::getText(this, translate("title_copy"), translate("label_copy"), QLineEdit::Normal, path->text(), &ok);
    if (!ok || destination.trimmed().isEmpty())
        return;
    onCopy(files, destination);
}

void Storage::onCopy(const QVector<File> &files, const QString &destination)
{
    int result =  QMessageBox::question(this, translate("title_copy"), translate("message_copy").arg(QString::number(files.count()), destination), QMessageBox::No | QMessageBox::Yes);
    if (result != QMessageBox::Yes)
        return;
    QMap<QString, bool> paths;
    foreach(const File &file, files)
        paths[file.path] = ((file.type == File::FOLDER) || (file.type == File::SYMLINK_FOLDER));
    Copy *copy = new Copy(device, paths, destination);
    connections.append(connect(copy, SIGNAL(finished(QVariant)), this, SLOT(onCopyFinished(QVariant)), Qt::QueuedConnection));
    Tasks::instance()->add(translate("task_copy").arg(QString::number(paths.count())), copy);
}

void Storage::onCopyFinished(const QVariant &result)
{
    QPair<int, int> pair = result.value<QPair<int, int>>();
    if (pair.second >= 1)
        QMessageBox::critical(this, translate("title_failure"), translate("message_copy_failed").arg(QString::number(pair.first), QString::number(pair.second)), QMessageBox::Close);
}

void Storage::onCreate()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, translate("title_create"), translate("label_create"), QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    QString path(this->path->text());
    if (!path.endsWith('/'))
        path.append('/');
    path.append(name);
    if (ADB::instance()->create(device, path))
        onRefresh();
    else
        QMessageBox::critical(this, translate("title_failure"), translate("message_create_failed").arg(path), QMessageBox::Close);
}

void Storage::onDetails()
{
    QVector<File> files = selected();
    if (files.isEmpty())
        return;
    File file = files.first();
    switch (file.type) {
    case File::FOLDER:
    case File::SYMLINK_FOLDER:
        break;
    default:
        emit showFile(file.path);
        break;
    }
}

void Storage::onDoubleClicked(const QModelIndex &index)
{
    File file = index.data(ROLE_STRUCT).value<File>();
    switch (file.type) {
    case File::FOLDER:
    case File::SYMLINK_FOLDER: {
        path->setText(file.path);
        onRefresh();
        break;
    }
    default:
        break;
    }
}

void Storage::onEdit()
{
    if (path->hasFocus())
        return;
    path->selectAll();
    path->setFocus();
}

void Storage::onFilesDropped(const QStringList &files, const QModelIndex &at)
{
    if (files.count() <= 0)
        return;
    QString path;
    if (at.isValid()) {
        File file = at.data(ROLE_STRUCT).value<File>();
        if ((file.type == File::FOLDER) || (file.type == File::SYMLINK_FOLDER))
            path = file.path;
    }
    if (path.isEmpty())
        path = this->path->text();
    int result =  QMessageBox::question(this, translate("title_push"), translate("message_push").arg(QString::number(files.count()), path), QMessageBox::No | QMessageBox::Yes);
    if (result != QMessageBox::Yes)
        return;
    Push *push = new Push(device, files, path);
    connections.append(connect(push, SIGNAL(finished(QVariant)), this, SLOT(onPushFinished(QVariant)), Qt::QueuedConnection));
    Tasks::instance()->add(translate("task_push").arg(QString::number(files.count())), push);
}

void Storage::onItemsDropped(const QModelIndexList &rows, const QModelIndex &at, Qt::DropAction action)
{
    if (!at.isValid())
        return;
    File destination = at.data(ROLE_STRUCT).value<File>();
    if (!((destination.type == File::FOLDER) || (destination.type == File::SYMLINK_FOLDER)))
        return;
    if (action == Qt::MoveAction) {
        QStringList files;
        foreach (const QModelIndex &index, rows)
            files << index.data(ROLE_STRUCT).value<File>().path;
        onMove(files, at.data(ROLE_STRUCT).value<File>().path);
    } else {
        QVector<File> files;
        foreach (const QModelIndex &index, rows)
            files << index.data(ROLE_STRUCT).value<File>();
        onCopy(files, at.data(ROLE_STRUCT).value<File>().path);
    }
}

void Storage::onMove()
{
    if (path->hasFocus())
        return;
    QVector<File> files = selected();
    if (files.isEmpty())
        return;
    QStringList sources;
    foreach (const File &file, files)
        sources << file.path;
    bool ok = false;
    QString destination = QInputDialog::getText(this, translate("title_move"), translate("label_move"), QLineEdit::Normal, path->text(), &ok);
    if (!ok || destination.trimmed().isEmpty())
        return;
    onMove(sources, destination);
}

void Storage::onMove(const QStringList &files, const QString &destination)
{
    Move *move = new Move(device, files, destination);
    connections.append(connect(move, SIGNAL(finished(QVariant, QStringList)), this, SLOT(onMoveFinished(QVariant, QStringList)), Qt::QueuedConnection));
    Tasks::instance()->add(translate("task_move").arg(QString::number(files.count())), move);
}

void Storage::onMoveFinished(const QVariant &result, const QStringList &moved)
{
    QPair<int, int> pair = result.value<QPair<int, int>>();
    if (pair.second >= 1)
        QMessageBox::critical(this, translate("title_failure"), translate("message_move_failed").arg(QString::number(pair.first), QString::number(pair.second)), QMessageBox::Close);
    if (moved.isEmpty())
        return;
    foreach (const QString &path, moved) {
        QList<QTreeWidgetItem *> rows = tree->findItems(path.section('/', -1, -1), Qt::MatchExactly, 0);
        if (rows.count() != 1)
            continue;
        delete rows.first();
    }
}

void Storage::onPull()
{
    QVector<File> files = selected();
    if (files.isEmpty())
        return;
    QFileDialog dialog(this, translate("title_browse"), Helpers::Settings::previousDirectory());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    if (dialog.exec() != QFileDialog::Accepted)
        return;
    QStringList folders = dialog.selectedFiles();
    if (folders.count() != 1)
        return;
    QDir directory(folders.first());
    Helpers::Settings::previousDirectory(directory.absolutePath());
    QMap<QString, bool> paths;
    foreach(const File &file, files)
        paths[file.path] = ((file.type == File::FOLDER) || (file.type == File::SYMLINK_FOLDER));
    Pull *pull = new Pull(device, paths, directory);
    connections.append(connect(pull, SIGNAL(finished(QVariant)), this, SLOT(onPullFinished(QVariant)), Qt::QueuedConnection));
    Tasks::instance()->add(translate("task_pull").arg(QString::number(paths.count())), pull);
}

void Storage::onPullFinished(const QVariant &result)
{
    QPair<int, int> pair = result.value<QPair<int, int>>();
    if (pair.second >= 1)
        QMessageBox::critical(this, translate("title_failure"), translate("message_pull_failed").arg(QString::number(pair.first), QString::number(pair.second)), QMessageBox::Close);
}

void Storage::onPush()
{
    QFileDialog dialog(this, translate("title_select"), Helpers::Settings::previousDirectory());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    if (dialog.exec() != QFileDialog::Accepted)
        return;
    QStringList files = dialog.selectedFiles();
    if (files.isEmpty())
        return;
    Helpers::Settings::previousDirectory(dialog.directory().absolutePath());
    onFilesDropped(files, QModelIndex());
}

void Storage::onPushFinished(const QVariant &result)
{
    QPair<int, int> pair = result.value<QPair<int, int>>();
    if (pair.second >= 1)
        QMessageBox::critical(this, translate("title_failure"), translate("message_push_failed").arg(QString::number(pair.first), QString::number(pair.second)), QMessageBox::Close);
    if (pair.first >= 1)
        onRefresh();
}

void Storage::onRefresh()
{
    int files = 0;
    int folders = 0;
    QString location(path->text());
    if (!location.startsWith('/')) {
        location.prepend('/');
        path->setText(location);
    }
    QString previous('/');
    if (location.count('/') > 1) {
        if (location.endsWith('/'))
            location = location.left(location.length() - 1);
        previous.append(location.mid(1, location.lastIndexOf('/')));
    }
    if (tree->model()->hasChildren())
        tree->model()->removeRows(0, tree->model()->rowCount());
    File file;
    file.type = File::FOLDER;
    file.path = previous;
    QTreeWidgetItem *up = new QTreeWidgetItem();
    for (int i = 0; i < 6; ++i)
        up->setData(i, ROLE_STRUCT, QVariant::fromValue(file));
    up->setIcon(0, ::icon("folder_previous"));
    up->setFlags(up->flags() & ~Qt::ItemIsSelectable);
    up->setText(0, "..");
    up->setToolTip(0, file.path);
    tree->addTopLevelItem(up);
    tree->setFirstColumnSpanned(0, tree->rootIndex(), true);
    QVector<File> list = ADB::instance()->files(device, location);
    foreach (file, list) {
        QTreeWidgetItem *row = new QTreeWidgetItem();
        switch (file.type) {
        case File::FOLDER:
        case File::SYMLINK_FOLDER:
            folders++;
            break;
        default:
            files++;
            break;
        }
        switch (file.type) {
        case File::BLOCK:
            row->setIcon(0, ::icon("file_block"));
            break;
        case File::CHARACTER:
            row->setIcon(0, ::icon("file_character"));
            break;
        case File::FOLDER:
            row->setIcon(0, ::icon("folder"));
            break;
        case File::FILE:
            row->setIcon(0, ::icon("file"));
            break;
        case File::PIPE:
            row->setIcon(0, ::icon("file_pipe"));
            break;
        case File::SOCKET:
            row->setIcon(0, ::icon("file_socket"));
            break;
        case File::SYMLINK_FOLDER:
            row->setIcon(0, ::icon("folder_symlink"));
            break;
        case File::SYMLINK_FILE:
            row->setIcon(0, ::icon("file_symlink"));
            break;
        case File::OTHER:
        case File::SYMLINK:
        default:
            row->setIcon(0, ::icon("file_unknown"));
            break;
        }
        for (int i = 0; i < 6; ++i)
            row->setData(i, ROLE_STRUCT, QVariant::fromValue(file));
        row->setText(0, file.name);
        if ((file.type == File::FOLDER) || (file.type == File::SYMLINK_FOLDER))
            row->setText(1, "");
        else
            row->setText(1, Format::size(file.size));
        row->setText(2, file.permission.display);
        row->setText(3, file.owner);
        row->setText(4, file.group);
        row->setText(5, QString(file.date).append(' ').append(file.time));
        row->setToolTip(0, file.path);
        tree->addTopLevelItem(row);
    }
    tree->scrollToTop();
    tree->setFocus();
}

void Storage::onRemove()
{
    if (path->hasFocus())
        return;
    QVector<File> files = selected();
    if (files.isEmpty())
        return;
    int result =  QMessageBox::question(this, translate("title_remove"), translate("message_remove").arg(QString::number(files.count())), QMessageBox::No | QMessageBox::Yes);
    if (result != QMessageBox::Yes)
        return;
    QMap<QString, bool> removeable;
    foreach (const File &file, files)
        removeable[file.path] = ((file.type == File::FOLDER) ||(file.type == File::SYMLINK_FOLDER));
    Remove *remove = new Remove(device, removeable);
    connections.append(connect(remove, SIGNAL(finished(QVariant, QStringList)), this, SLOT(onRemoveFinished(QVariant, QStringList)), Qt::QueuedConnection));
    Tasks::instance()->add(translate("task_remove").arg(QString::number(removeable.count())), remove);
}

void Storage::onRemoveFinished(const QVariant &result, const QStringList &removed)
{
    QPair<int, int> pair = result.value<QPair<int, int>>();
    if (pair.second >= 1)
        QMessageBox::critical(this, translate("title_failure"), translate("message_remove_failed").arg(QString::number(pair.first), QString::number(pair.second)), QMessageBox::Close);
    if (removed.isEmpty())
        return;
    foreach (const QString &path, removed) {
        QList<QTreeWidgetItem *> rows = tree->findItems(path.section('/', -1, -1), Qt::MatchExactly, 0);
        if (rows.count() != 1)
            continue;
        delete rows.first();
    }
}

void Storage::onRename()
{
    QVector<File> files = selected();
    if (files.isEmpty())
        return;
    bool ok = false;
    QString name = QInputDialog::getText(this, translate("title_rename"), translate("label_rename"), QLineEdit::Normal, files.first().name, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    bool multiple = (files.count() > 1);
    QMap<QString, QString> renameable;
    for (int i = 0; i < files.count(); ++i) {
        QString newname(name);
        if (multiple)
            newname.prepend(QString("(%1) ").arg(QString::number(i + 1)));
        File file = files.at(i);
        QString newpath(file.path.section('/', 0, -2));
        newpath.append('/');
        newpath.append(newname);
        renameable[file.path] = newpath;
    }
    Rename *rename = new Rename(device, renameable);
    connections.append(connect(rename, SIGNAL(finished(QVariant, QStringList)), this, SLOT(onRenameFinished(QVariant, QStringList)), Qt::QueuedConnection));
    Tasks::instance()->add(translate("task_rename").arg(QString::number(renameable.count())), rename);
}

void Storage::onRenameFinished(const QVariant &result, const QStringList &a)
{
    Q_UNUSED(a)
    QPair<int, int> pair = result.value<QPair<int, int>>();
    if (pair.second >= 1)
        QMessageBox::critical(this, translate("title_failure"), translate("message_rename_failed").arg(QString::number(pair.first), QString::number(pair.second)), QMessageBox::Close);
    if (pair.first >= 1)
        onRefresh();
}

void Storage::onReturn()
{
    if (path->hasFocus()) {
        onRefresh();
        return;
    }
    onDoubleClicked(tree->currentIndex());
}

void Storage::onUp()
{
    if (path->hasFocus())
        return;
    onDoubleClicked(tree->model()->index(0, 0, tree->rootIndex()));
}

QVector<File> Storage::selected()
{
    QVector<File> files;
    foreach (QTreeWidgetItem *item, tree->selectedItems())
        files.append(item->data(0, ROLE_STRUCT).value<File>());
    return files;
}

Storage::~Storage()
{
    foreach (QMetaObject::Connection connection, connections)
        disconnect(connection);
}

} // namespace Components
} // namespace APKStudio
} // namespace VPZ
