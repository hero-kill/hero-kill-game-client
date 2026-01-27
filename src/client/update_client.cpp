#include "client/update_client.h"
#include "network/client_socket.h"
#include "network/router.h"
#include "core/util.h"
#include "core/packman.h"
#include "ui/qmlbackend.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

UpdateClient::UpdateClient(QObject *parent)
    : QObject(parent)
    , m_socket(new ClientSocket())
    , m_router(new Router(this, m_socket, Router::TYPE_CLIENT))
    , m_state(Idle)
    , m_downloadProgress(0)
    , m_serverPort(9527)
{
    connect(m_socket, &ClientSocket::connected, this, &UpdateClient::onConnected);
    connect(m_socket, &ClientSocket::disconnected, this, &UpdateClient::onDisconnected);
    connect(m_socket, &ClientSocket::error_message, this, &UpdateClient::onError);

    // 连接 Router 的通知信号
    connect(m_router, &Router::notification_got, this, [this](const QByteArray &command, const QByteArray &data) {
        QString cmd = QString::fromUtf8(command);
        if (cmd == "NetworkDelayTest") {
            handleNetworkDelayTest(data);
        } else if (cmd == "UpdateInfo") {
            handleUpdateInfo(data);
        } else if (cmd == "ErrorMsg") {
            QCborValue cbor = QCborValue::fromCbor(data);
            QString error = cbor.toString();
            setState(Error, error);
        }
    });
}

UpdateClient::~UpdateClient()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
}

void UpdateClient::setState(UpdateState state, const QString &errorMessage)
{
    if (m_state != state || m_errorMessage != errorMessage) {
        m_state = state;
        m_errorMessage = errorMessage;
        emit stateChanged(m_state);
        if (!errorMessage.isEmpty()) {
            emit errorOccurred(errorMessage);
        }
    }
}

void UpdateClient::connectToServer(const QString &server, quint16 port)
{
    // 防止重复连接同一个服务器
    if (m_socket->isConnected() && m_serverAddr == server && m_serverPort == port) {
        return;
    }

    m_serverAddr = server;
    m_serverPort = port;
    m_errorMessage.clear();
    m_publicKey.clear();
    m_updateInfo.clear();

    // 如果已连接到其他服务器，先断开
    if (m_socket->isConnected()) {
        m_socket->disconnectFromHost();
    }

    setState(Connecting);
    m_socket->connectToHost(server, port);
}

void UpdateClient::startDownload()
{
    if (m_state != NeedUpdate) {
        return;
    }

    setState(Downloading);

    // 从 updateInfo 获取包列表并开始下载
    QVariantList packages = m_updateInfo.value("packages").toList();
    if (packages.isEmpty()) {
        setState(NeedRestart);
        return;
    }

    // 构建包摘要 JSON
    QJsonArray pkgArray;
    for (const QVariant &pkg : packages) {
        QVariantMap pkgMap = pkg.toMap();
        QJsonObject obj;
        obj["name"] = pkgMap["name"].toString();
        obj["url"] = pkgMap["url"].toString();
        obj["hash"] = pkgMap["hash"].toString();
        pkgArray.append(obj);
    }

    // 使用 PackMan 的 loadSummary 方法进行下载
    QString summary = QJsonDocument(pkgArray).toJson(QJsonDocument::Compact);
    Pacman->loadSummary(summary, true); // true = useThread
}

void UpdateClient::disconnectFromServer()
{
    m_socket->disconnectFromHost();
    setState(Idle);
}

void UpdateClient::retry()
{
    // 允许在非终态下重试（Connected 状态可能是卡在等待 NetworkDelayTest）
    if (m_state != UpToDate && m_state != NeedRestart && m_state != VersionTooOld) {
        // 先断开再连接
        if (m_socket->isConnected()) {
            m_socket->disconnectFromHost();
            // 等待断开完成后再重连
            QTimer::singleShot(100, this, [this]() {
                setState(Connecting);
                m_socket->connectToHost(m_serverAddr, m_serverPort);
            });
        } else {
            setState(Connecting);
            m_socket->connectToHost(m_serverAddr, m_serverPort);
        }
    }
}

void UpdateClient::onConnected()
{
    setState(Connected);
    // 等待服务器发送 NetworkDelayTest
}

void UpdateClient::onDisconnected()
{
    if (m_state != NeedRestart && m_state != UpToDate && m_state != VersionTooOld) {
        setState(Error, tr("Disconnected from server"));
    }
}

void UpdateClient::onError(const QString &error)
{
    setState(Error, error);
}

void UpdateClient::onDownloadProgress(int progress)
{
    m_downloadProgress = progress;
    emit downloadProgressChanged(progress);
}

void UpdateClient::onDownloadFinished(bool success, const QString &error)
{
    if (success) {
        setState(NeedRestart);
    } else {
        setState(Error, error);
    }
}

void UpdateClient::sendCheckUpdate()
{
    // 计算客户端 MD5
    QString md5 = calcFileMD5();

    // 构建 CheckUpdate 请求: [version, md5]
    QCborArray body;
    body.append(QString::fromLatin1(FK_VERSION));
    body.append(md5);

    int type = Router::TYPE_NOTIFICATION | Router::SRC_CLIENT | Router::DEST_SERVER;
    m_router->notify(type, "CheckUpdate", body.toCborValue().toCbor());

    setState(Checking);
}

void UpdateClient::handleNetworkDelayTest(const QByteArray &data)
{
    // NetworkDelayTest 返回公钥
    QCborValue cbor = QCborValue::fromCbor(data);
    if (cbor.isString()) {
        m_publicKey = cbor.toString();
    } else if (cbor.isByteArray()) {
        m_publicKey = QString::fromUtf8(cbor.toByteArray());
    }

    emit publicKeyReceived(m_publicKey);

    // 收到公钥后发送 CheckUpdate
    sendCheckUpdate();
}

void UpdateClient::handleUpdateInfo(const QByteArray &data)
{
    if (data.isEmpty()) {
        qWarning() << "UpdateClient: UpdateInfo 数据为空";
        setState(Error, tr("Empty UpdateInfo response"));
        return;
    }

    m_updateInfo.clear();

    // CBOR 格式解析
    QCborValue cbor = QCborValue::fromCbor(data);
    if (!cbor.isArray()) {
        qWarning() << "UpdateClient: UpdateInfo 不是 CBOR 数组格式";
        setState(Error, tr("Invalid CBOR format"));
        return;
    }

    QCborArray arr = cbor.toArray();
    if (arr.size() < 6) {
        qWarning() << "UpdateClient: CBOR 数组长度不足:" << arr.size();
        setState(Error, tr("Invalid CBOR format"));
        return;
    }

    // 格式: [status, minVersion, maxVersion, packages, message, needRestart]
    // CBOR 可能将字符串编码为 byte string，需要特殊处理
    auto getStringValue = [](const QCborValue &val) -> QString {
        if (val.isString()) {
            return val.toString();
        } else if (val.isByteArray()) {
            return QString::fromUtf8(val.toByteArray());
        }
        return QString();
    };

    QString status = getStringValue(arr[0]);
    m_updateInfo["status"] = status;
    m_updateInfo["minVersion"] = getStringValue(arr[1]);
    m_updateInfo["maxVersion"] = getStringValue(arr[2]);
    m_updateInfo["message"] = getStringValue(arr[4]);
    m_updateInfo["needRestart"] = arr[5].toBool();

    // 解析包列表: [[name, url, hash], ...]
    QVariantList packages;
    QCborArray pkgsArray = arr[3].toArray();
    for (const QCborValue &pkg : pkgsArray) {
        QCborArray pkgArray = pkg.toArray();
        if (pkgArray.size() >= 3) {
            QVariantMap pkgInfo;
            pkgInfo["name"] = getStringValue(pkgArray[0]);
            pkgInfo["url"] = getStringValue(pkgArray[1]);
            pkgInfo["hash"] = getStringValue(pkgArray[2]);
            packages.append(pkgInfo);
        }
    }
    m_updateInfo["packages"] = packages;

    qInfo() << "UpdateClient: 收到 UpdateInfo, status=" << status << ", packages=" << packages.length();

    emit updateInfoReceived(m_updateInfo);

    // 根据状态切换
    if (status == "UP_TO_DATE") {
        setState(UpToDate);
    } else if (status == "UPDATE_REQUIRED") {
        setState(NeedUpdate);
    } else if (status == "VERSION_TOO_OLD") {
        setState(VersionTooOld);
    } else {
        qWarning() << "UpdateClient: 未知状态" << status;
        setState(Error, tr("Unknown update status: %1").arg(status));
    }
}
