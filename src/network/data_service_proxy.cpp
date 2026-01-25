// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_service_proxy.h"
#include <QNetworkRequest>
#include <QUrl>

DataServiceProxy *DataService = nullptr;

DataServiceProxy::DataServiceProxy(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_baseUrl("http://localhost:8082/api/v1")
{
  connect(m_manager, &QNetworkAccessManager::finished,
          this, &DataServiceProxy::onReplyFinished);
}

DataServiceProxy::~DataServiceProxy() {
}

void DataServiceProxy::setBaseUrl(const QString &url) {
  if (m_baseUrl != url) {
    m_baseUrl = url;
    emit baseUrlChanged();
  }
}

QString DataServiceProxy::getBaseUrl() const {
  return m_baseUrl;
}

void DataServiceProxy::setAccessToken(const QString &token) {
  m_accessToken = token;
}

void DataServiceProxy::setRefreshToken(const QString &token) {
  m_refreshToken = token;
}

QString DataServiceProxy::getAccessToken() const {
  return m_accessToken;
}

QString DataServiceProxy::getRefreshToken() const {
  return m_refreshToken;
}

QString DataServiceProxy::execute(const QString &action, const QVariantMap &params) {
  return sendRequest("/execute", action, params);
}

QString DataServiceProxy::executeAsync(const QString &action, const QVariantMap &params) {
  return sendRequest("/execute-async", action, params);
}

QString DataServiceProxy::executeBatch(const QVariantList &requests) {
  return sendBatchRequest(requests);
}

QString DataServiceProxy::sendRequest(const QString &endpoint, const QString &action,
                                       const QVariantMap &params, bool isRetry) {
  QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  QNetworkRequest request;
  request.setUrl(QUrl(m_baseUrl + endpoint));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  // 添加JWT Token到Authorization头
  if (!m_accessToken.isEmpty()) {
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
  }

  QJsonObject body;
  body["action"] = action;
  body["params"] = QJsonObject::fromVariantMap(params);

  QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
  QNetworkReply *reply = m_manager->post(request, data);

  RequestInfo info;
  info.requestId = requestId;
  info.action = action;
  info.endpoint = endpoint;
  info.params = params;
  info.isRetry = isRetry;
  m_pendingRequests[reply] = info;

  return requestId;
}

QString DataServiceProxy::sendBatchRequest(const QVariantList &requests) {
  QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  QNetworkRequest request;
  request.setUrl(QUrl(m_baseUrl + "/execute-batch"));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  // 添加JWT Token到Authorization头
  if (!m_accessToken.isEmpty()) {
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
  }

  QJsonArray requestsArray;
  for (const auto &req : requests) {
    QVariantMap reqMap = req.toMap();
    QJsonObject reqObj;
    reqObj["action"] = reqMap["action"].toString();
    reqObj["params"] = QJsonObject::fromVariantMap(reqMap["params"].toMap());
    requestsArray.append(reqObj);
  }

  QJsonObject body;
  body["requests"] = requestsArray;

  QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
  QNetworkReply *reply = m_manager->post(request, data);

  RequestInfo info;
  info.requestId = requestId;
  info.action = "batch";
  info.endpoint = "/execute-batch";
  m_pendingRequests[reply] = info;

  return requestId;
}

void DataServiceProxy::onReplyFinished(QNetworkReply *reply) {
  RequestInfo info = m_pendingRequests.take(reply);
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  reply->deleteLater();

  // 处理401未授权错误 - 请求刷新Token
  if (statusCode == 401 && !info.isRetry && !m_refreshToken.isEmpty()) {
    // 将请求加入待重试队列
    m_pendingRetryRequests.append(info);
    // 发出信号请求刷新Token（由客户端向游戏服务器请求）
    if (!m_isRefreshing) {
      m_isRefreshing = true;
      emit tokenRefreshRequested(m_refreshToken);
    }
    return;
  }

  QByteArray responseData = reply->readAll();
  QJsonDocument doc = QJsonDocument::fromJson(responseData);
  QJsonObject obj = doc.object();

  // 优先使用 JSON 中的 message 字段作为错误信息
  QString error;
  if (!obj["message"].isNull() && !obj["message"].toString().isEmpty()) {
    error = obj["message"].toString();
  } else if (reply->error() != QNetworkReply::NoError) {
    error = reply->errorString();
  }

  // HTTP 错误或网络错误
  if (statusCode != 200 || reply->error() != QNetworkReply::NoError) {
    emit responseReceived(info.requestId, info.action, false, QVariant(), error);
    return;
  }

  bool success = obj["success"].toBool();
  QVariant data = obj["data"].toVariant();

  emit responseReceived(info.requestId, info.action, success, data, error);
}

void DataServiceProxy::onTokenRefreshed(const QString &newAccessToken) {
  m_accessToken = newAccessToken;
  m_isRefreshing = false;

  // 重试所有待处理的请求
  for (const auto &info : m_pendingRetryRequests) {
    retryRequest(info);
  }
  m_pendingRetryRequests.clear();
}

void DataServiceProxy::onTokenRefreshFailed(const QString &error) {
  m_isRefreshing = false;

  // 通知所有待处理的请求失败
  for (const auto &info : m_pendingRetryRequests) {
    emit responseReceived(info.requestId, info.action, false,
                          QVariant(), "Token refresh failed: " + error);
  }
  m_pendingRetryRequests.clear();
}

void DataServiceProxy::retryRequest(const RequestInfo &info) {
  sendRequest(info.endpoint, info.action, info.params, true);
}
