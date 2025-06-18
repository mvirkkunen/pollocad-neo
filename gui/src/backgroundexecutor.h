#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>

#include "executor.h"
#include "value.h"
#include "logmessage.h"

class BackgroundExecutorResult;

class BackgroundExecutor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged);

public:
    Q_INVOKABLE void execute(QString code);
    bool isBusy() const;

signals:
    void result(BackgroundExecutorResult *result);
    void isBusyChanged();

private:
    Executor m_executor;
    QThreadPool m_threadPool;
};

class LogMessageModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        Level = 1,
        Message = 2,
        Location = 3,
    };

    explicit LogMessageModel(std::vector<LogMessage> messages);

    int rowCount(const QModelIndex & = QModelIndex()) const override;
    int columnCount(const QModelIndex & = QModelIndex()) const override;
    //QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<LogMessage> m_messages;
};

class BackgroundExecutorResult : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool hasShapes READ hasShapes CONSTANT);

public:
    BackgroundExecutorResult(const BackgroundExecutorResult &other) : m_messages(other.m_messages), m_shapes(other.m_shapes) { }
    BackgroundExecutorResult(std::vector<LogMessage> messages, std::optional<ShapeList> shapes) : m_messages(messages), m_shapes(shapes) { }

public:
    Q_INVOKABLE const LogMessageModel *messagesModel() const;
    const std::vector<LogMessage> &messages() const { return m_messages; }
    const std::optional<ShapeList> &shapes() const { return m_shapes; }
    bool hasShapes() const { return m_shapes.has_value(); }

private:
    std::vector<LogMessage> m_messages;
    std::optional<ShapeList> m_shapes;
};
