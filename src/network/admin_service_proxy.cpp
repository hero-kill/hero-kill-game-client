// SPDX-License-Identifier: GPL-3.0-or-later

#include "admin_service_proxy.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QEventLoop>

AdminServiceProxy *AdminService = nullptr;

AdminServiceProxy::AdminServiceProxy(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_baseUrl("http://localhost:8080/api/v1")
{
  connect(m_manager, &QNetworkAccessManager::finished,
          this, &AdminServiceProxy::onReplyFinished);
}

AdminServiceProxy::~AdminServiceProxy() {
}

void AdminServiceProxy::setBaseUrl(const QString &url) {
  m_baseUrl = url;
}

QString AdminServiceProxy::getBaseUrl() const {
  return m_baseUrl;
}

void AdminServiceProxy::setAdminSecret(const QString &secret) {
  m_adminSecret = secret;
}

QString AdminServiceProxy::executeAsync(const QString &action, const QVariantMap &params) {
  QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  // 如果不在主线程，转发到主线程执行
  if (QThread::currentThread() != this->thread()) {
    QMetaObject::invokeMethod(this, [this, requestId, action, params]() {
      sendRequest(requestId, action, params);
    }, Qt::QueuedConnection);
  } else {
    sendRequest(requestId, action, params);
  }

  return requestId;
}

QVariantMap AdminServiceProxy::execute(const QString &action, const QVariantMap &params) {
  QVariantMap result;
  result["success"] = false;

  // 创建同步网络管理器
  QNetworkAccessManager syncManager;

  QNetworkRequest request;
  request.setUrl(QUrl(m_baseUrl + "/admin/execute-async"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  if (!m_adminSecret.isEmpty()) {
    request.setRawHeader("admin-secret", ("Bearer " + m_adminSecret).toUtf8());
  }

  QJsonObject body;
  body["action"] = action;
  body["params"] = QJsonObject::fromVariantMap(params);

  QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);

  // 使用事件循环等待响应
  QEventLoop loop;
  QNetworkReply *reply = syncManager.post(request, data);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QByteArray responseData = reply->readAll();
  QJsonDocument doc = QJsonDocument::fromJson(responseData);
  QJsonObject obj = doc.object();

  if (statusCode == 200 && reply->error() == QNetworkReply::NoError) {
    result["success"] = obj["success"].toBool();
    result["data"] = obj["data"].toVariant();
  } else {
    QString error;
    if (!obj["message"].isNull() && !obj["message"].toString().isEmpty()) {
      error = obj["message"].toString();
    } else if (reply->error() != QNetworkReply::NoError) {
      error = reply->errorString();
    }
    result["error"] = error;
  }

  reply->deleteLater();
  return result;
}

void AdminServiceProxy::sendRequest(const QString &requestId, const QString &action, const QVariantMap &params) {

  QNetworkRequest request;
  request.setUrl(QUrl(m_baseUrl + "/admin/execute-async"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  // 使用 admin-secret header 认证
  if (!m_adminSecret.isEmpty()) {
    request.setRawHeader("admin-secret", ("Bearer " + m_adminSecret).toUtf8());
  }

  QJsonObject body;
  body["action"] = action;
  body["params"] = QJsonObject::fromVariantMap(params);

  QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
  QNetworkReply *reply = m_manager->post(request, data);

  RequestInfo info;
  info.requestId = requestId;
  info.action = action;
  m_pendingRequests[reply] = info;
}

void AdminServiceProxy::onReplyFinished(QNetworkReply *reply) {
  RequestInfo info = m_pendingRequests.take(reply);
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  reply->deleteLater();

  QByteArray responseData = reply->readAll();
  QJsonDocument doc = QJsonDocument::fromJson(responseData);
  QJsonObject obj = doc.object();

  QString error;
  if (!obj["message"].isNull() && !obj["message"].toString().isEmpty()) {
    error = obj["message"].toString();
  } else if (reply->error() != QNetworkReply::NoError) {
    error = reply->errorString();
  }

  if (statusCode != 200 || reply->error() != QNetworkReply::NoError) {
    emit responseReceived(info.requestId, info.action, false, QVariant(), error);
    return;
  }

  bool success = obj["success"].toBool();
  QVariant data = obj["data"].toVariant();

  emit responseReceived(info.requestId, info.action, success, data, error);
}
