// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _ADMIN_SERVICE_PROXY_H
#define _ADMIN_SERVICE_PROXY_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QThread>

class AdminServiceProxy : public QObject {
  Q_OBJECT

public:
  explicit AdminServiceProxy(QObject *parent = nullptr);
  ~AdminServiceProxy();

  void setBaseUrl(const QString &url);
  QString getBaseUrl() const;

  void setAdminSecret(const QString &secret);

  // 异步执行 - POST /api/v1/admin/execute-async
  QString executeAsync(const QString &action, const QVariantMap &params = QVariantMap());

  // 同步执行 - POST /api/v1/admin/execute-async (阻塞等待结果)
  QVariantMap execute(const QString &action, const QVariantMap &params = QVariantMap());

signals:
  void responseReceived(const QString &requestId, const QString &action,
                        bool success, const QVariant &data, const QString &error);

private slots:
  void onReplyFinished(QNetworkReply *reply);

private:
  struct RequestInfo {
    QString requestId;
    QString action;
  };

  void sendRequest(const QString &requestId, const QString &action, const QVariantMap &params);

  QNetworkAccessManager *m_manager;
  QString m_baseUrl;
  QString m_adminSecret;
  QMap<QNetworkReply*, RequestInfo> m_pendingRequests;
};

extern AdminServiceProxy *AdminService;

#endif // _ADMIN_SERVICE_PROXY_H