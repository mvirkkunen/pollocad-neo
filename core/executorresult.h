#pragma once

#include <QAbstractListModel>
#include <QObject>

#include <TopoDS_Shape.hxx>
#include "logmessage.h"

class LogMessageModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        Type = 1,
        Message = Qt::DisplayRole,
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

class ExecutorResult : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool hasShape READ hasShape CONSTANT);

public:
    ExecutorResult(const ExecutorResult &other) : m_messages(other.m_messages), m_shape(other.m_shape) { }
    ExecutorResult(std::vector<LogMessage> messages, TopoDS_Shape shape) : m_messages(messages), m_shape(shape) { }

public:
    const std::vector<LogMessage> &messages() const;
    Q_INVOKABLE const LogMessageModel *messagesModel() const;
    const TopoDS_Shape &shape() const { return m_shape; }
    bool hasShape() const { return !m_shape.IsNull(); }

private:
    std::vector<LogMessage> m_messages;
    TopoDS_Shape m_shape;
};
