#include "backgroundexecutor.h"

void BackgroundExecutor::execute(QString code) {
    m_threadPool.start([this, code]() {
        auto r = m_executor.execute(code.toStdString());
        if (!r.result) {
            emit result(new BackgroundExecutorResult{r.messages, {}});
            emit isBusyChanged();
            return;
        }

        std::optional<ShapeList> shapes;
        
        if (auto pshapes = r.result->as<ShapeList>()) {
            shapes = *pshapes;
        } else if (r.result->undefined()) {
            shapes = ShapeList{};
        } else {
            r.messages.push_back(LogMessage{LogMessage::Level::Error, "Top level value is not shapes"});
        }

        emit result(new BackgroundExecutorResult{r.messages, shapes});
        emit isBusyChanged();
    });

    emit isBusyChanged();
}

bool BackgroundExecutor::isBusy() const {
    return m_executor.isBusy();
}

const LogMessageModel *BackgroundExecutorResult::messagesModel() const {
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
    case Level:
        switch (msg.level) {
            case LogMessage::Level::Info: return "info";
            case LogMessage::Level::Warning: return "warning";
            case LogMessage::Level::Error: default: return "error";
        }
    case Message:
        return QString::fromStdString(msg.message);
    case Location:
        return msg.span.begin;
    }

    return {};
}

QHash<int, QByteArray> LogMessageModel::roleNames() const {
    return {
        {Level, "level"},
        {Message, "message"},
        {Location, "location"},
    };
}
