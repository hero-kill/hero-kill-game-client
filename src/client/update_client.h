#ifndef UPDATE_CLIENT_H
#define UPDATE_CLIENT_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>

class ClientSocket;
class Router;

/**
 * 预登录更新客户端
 * 在 Lua 环境初始化之前处理更新检查
 * 使用 CBOR 协议与服务端通信
 */
class UpdateClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString publicKey READ publicKey NOTIFY publicKeyReceived)
    Q_PROPERTY(QVariantMap updateInfo READ updateInfo NOTIFY updateInfoReceived)

public:
    explicit UpdateClient(QObject *parent = nullptr);
    ~UpdateClient();

    enum UpdateState {
        Idle,           // 空闲
        Connecting,     // 连接中
        Connected,      // 已连接，等待 NetworkDelayTest
        Checking,       // 检查更新中
        UpToDate,       // 已是最新版本，可以登录
        NeedUpdate,     // 需要更新
        Downloading,    // 下载中
        NeedRestart,    // 需要重启
        VersionTooOld,  // 版本过低，需下载新客户端
        Error           // 错误
    };
    Q_ENUM(UpdateState)

    UpdateState state() const { return m_state; }
    int downloadProgress() const { return m_downloadProgress; }
    QString errorMessage() const { return m_errorMessage; }
    QString publicKey() const { return m_publicKey; }
    QVariantMap updateInfo() const { return m_updateInfo; }

    Q_INVOKABLE void connectToServer(const QString &server, quint16 port);
    Q_INVOKABLE void startDownload();
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void retry();

signals:
    void stateChanged(UpdateClient::UpdateState state);
    void downloadProgressChanged(int progress);
    void publicKeyReceived(const QString &pubkey);
    void updateInfoReceived(const QVariantMap &info);
    void errorOccurred(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(const QString &error);
    void onDownloadProgress(int progress);
    void onDownloadFinished(bool success, const QString &error);

private:
    ClientSocket *m_socket;
    Router *m_router;
    UpdateState m_state;
    int m_downloadProgress;
    QString m_errorMessage;
    QString m_publicKey;
    QVariantMap m_updateInfo;
    QString m_serverAddr;
    quint16 m_serverPort;

    void setState(UpdateState state, const QString &errorMessage = "");
    void sendCheckUpdate();
    void handleNetworkDelayTest(const QByteArray &data);
    void handleUpdateInfo(const QByteArray &data);
};

#endif // UPDATE_CLIENT_H
