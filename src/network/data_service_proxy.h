// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _DATA_SERVICE_PROXY_H
#define _DATA_SERVICE_PROXY_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

class DataServiceProxy : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString baseUrl READ getBaseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
  Q_PROPERTY(QString gatewayUrl READ getGatewayUrl CONSTANT)

public:
  explicit DataServiceProxy(QObject *parent = nullptr);
  ~DataServiceProxy();

  void setBaseUrl(const QString &url);
  QString getBaseUrl() const;
  QString getGatewayUrl() const;

  // JWT Token管理
  Q_INVOKABLE void setAccessToken(const QString &token);
  Q_INVOKABLE  void setRefreshToken(const QString &token);
  QString getAccessToken() const;
  QString getRefreshToken() const;

  // 同步执行 - POST /api/v1/execute
  Q_INVOKABLE QString execute(const QString &action, const QVariantMap &params = QVariantMap());

  // 异步执行 - POST /api/v1/execute-async
  Q_INVOKABLE QString executeAsync(const QString &action, const QVariantMap &params = QVariantMap());

  // 批量执行 - POST /api/v1/execute-batch
  Q_INVOKABLE QString executeBatch(const QVariantList &requests);

  // 获取 Game Server 地址 (通过 Gateway 服务发现)
  Q_INVOKABLE void fetchGameServer();
  // 获取指定 serverId 的 Game Server 地址
  Q_INVOKABLE void fetchGameServerById(const QString &serverId);

  // 刷新Token完成后调用，用于重试待处理的请求
  Q_INVOKABLE void onTokenRefreshed(const QString &newAccessToken);
  // 刷新Token失败后调用
  Q_INVOKABLE void onTokenRefreshFailed(const QString &error);

signals:
  void baseUrlChanged();
  // 通用响应信号: requestId, action, success, data, error
  void responseReceived(const QString &requestId, const QString &action,
                        bool success, const QVariant &data, const QString &error);
  // 请求刷新Token信号 - 客户端收到后应向游戏服务器请求刷新
  void tokenRefreshRequested(const QString &refreshToken);
  // Game Server 地址获取成功
  void gameServerReceived(const QString &host, int port, int udpPort, const QString &serverId);
  // Game Server 地址获取失败
  void gameServerFailed(const QString &error);

private slots:
  void onReplyFinished(QNetworkReply *reply);

private:
  struct RequestInfo {
    QString requestId;
    QString action;
    QString endpoint;
    QVariantMap params;  // 保存参数用于重试
    bool isRetry = false;
    bool isDiscovery = false;  // 是否是服务发现请求
  };

  QString sendRequest(const QString &endpoint, const QString &action,
                      const QVariantMap &params, bool isRetry = false);
  QString sendBatchRequest(const QVariantList &requests);
  void retryRequest(const RequestInfo &info);

  QNetworkAccessManager *m_manager;
  QString m_gatewayUrl;
  QString m_baseUrl;
  QString m_accessToken;
  QString m_refreshToken;
  bool m_isRefreshing = false;
  QList<RequestInfo> m_pendingRetryRequests;  // 等待Token刷新后重试的请求
  QMap<QNetworkReply*, RequestInfo> m_pendingRequests;
};

extern DataServiceProxy *DataService;

#endif // _DATA_SERVICE_PROXY_H
