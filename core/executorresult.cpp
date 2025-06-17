#include "executorresult.h"

const std::vector<LogMessage> &ExecutorResult::messages() const {
    return m_messages;
}

const LogMessageModel *ExecutorResult::messagesModel() const {
    return new LogMessageModel(m_messages);
}

LogMessageModel::LogMessageModel(std::vector<LogMessage> messages) : m_messages(messages) { }


int LogMessageModel::rowCount(const QModelIndex &) const {
    return m_messages.size();
}

int LogMessageModel::columnCount(const QModelIndex &) const {
    return 3;
}

QVariant LogMessageModel::data(const QModelIndex &index, int role) const {
    const auto &msg = m_messages[index.row()];

    switch (role) {
    case Type:
        switch (msg.level()) {
            case LogMessage::Level::Info: return "";
            case LogMessage::Level::Warning: return "Warn";
            case LogMessage::Level::Error: default: return "Err";
        }
    case Message:
        return msg.message();
    case Location:
        return msg.span().begin();
    }

    return {};
}

QHash<int, QByteArray> LogMessageModel::roleNames() const {
    return {
        {Type, "type"},
        {Message, "message"},
        {Location, "location"},
    };
}
