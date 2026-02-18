// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_service_proxy.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>

DataServiceProxy *DataService = nullptr;

DataServiceProxy::DataServiceProxy(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
  // 从配置文件读取 gatewayUrl
  m_gatewayUrl = "http://localhost:9001";  // 默认值
  QFile conf("herokill.client.config.json");
  if (conf.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(conf.readAll());
    conf.close();
    if (doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains("gatewayUrl") && !obj["gatewayUrl"].toString().isEmpty()) {
        m_gatewayUrl = obj["gatewayUrl"].toString();
      }
    }
  }
  m_baseUrl = m_gatewayUrl + "/api/user/v1";

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

QString DataServiceProxy::getGatewayUrl() const {
  return m_gatewayUrl;
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

QString DataServiceProxy::gatewayGet(const QString &path) {
  QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  QString fullUrl = m_gatewayUrl + path;


  QNetworkRequest request;
  request.setUrl(QUrl(fullUrl));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QNetworkReply *reply = m_manager->get(request);

  RequestInfo info;
  info.requestId = requestId;
  info.action = path;
  info.isGateway = true;
  m_pendingRequests[reply] = info;

  return requestId;
}

QString DataServiceProxy::gatewayPost(const QString &path, const QVariantMap &params) {
  QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  QNetworkRequest request;
  request.setUrl(QUrl(m_gatewayUrl + path));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QByteArray data = QJsonDocument(QJsonObject::fromVariantMap(params)).toJson(QJsonDocument::Compact);
  QNetworkReply *reply = m_manager->post(request, data);

  RequestInfo info;
  info.requestId = requestId;
  info.action = path;
  info.isGateway = true;
  m_pendingRequests[reply] = info;

  return requestId;
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
  info.isGateway = false;
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
  info.isGateway = false;
  m_pendingRequests[reply] = info;

  return requestId;
}

void DataServiceProxy::onReplyFinished(QNetworkReply *reply) {
  RequestInfo info = m_pendingRequests.take(reply);
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QByteArray responseData = reply->readAll();
  bool hasNetworkError = reply->error() != QNetworkReply::NoError;
  QString networkError = reply->errorString();
  reply->deleteLater();

  // 处理 Gateway 通用请求
  if (info.isGateway) {

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QVariant data = doc.object().toVariantMap();
    bool success = !hasNetworkError && statusCode == 200;
    emit gatewayResponseReceived(info.requestId, success, data, networkError);
    return;
  }

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

  QJsonDocument doc = QJsonDocument::fromJson(responseData);
  QJsonObject obj = doc.object();

  QVariant error;
  QJsonObject errorObj = obj["error"].toObject();
  if (!errorObj.isEmpty()) {
    QVariantMap errorMap;
    errorMap["code"] = errorObj["code"].toString();
    errorMap["display"] = errorObj["display"].toString();
    errorMap["message"] = errorObj["message"].toString();
    error = errorMap;
  } else if (hasNetworkError) {
    QVariantMap errorMap;
    errorMap["code"] = "NETWORK_ERROR";
    errorMap["display"] = "toast";
    errorMap["message"] = networkError;
    error = errorMap;
  } else if (!obj["message"].isNull() && !obj["message"].toString().isEmpty()) {
    QVariantMap errorMap;
    errorMap["code"] = "";
    errorMap["display"] = "toast";
    errorMap["message"] = obj["message"].toString();
    error = errorMap;
  }

  // HTTP 错误或网络错误
  if (statusCode != 200 || hasNetworkError) {
    emit responseReceived(info.requestId, info.action, false, QVariant(), error);
    return;
  }

  bool success = obj["success"].toBool();
  QVariant data = obj["data"].toVariant();

  if (!success) {
    emit responseReceived(info.requestId, info.action, false, QVariant(), error);
    return;
  }

  emit responseReceived(info.requestId, info.action, true, data, QVariant());
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
    QVariantMap errorMap;
    errorMap["code"] = "TOKEN_REFRESH_FAILED";
    errorMap["display"] = "toast";
    errorMap["message"] = "Token refresh failed: " + error;
    emit responseReceived(info.requestId, info.action, false,
                          QVariant(), errorMap);
  }
  m_pendingRetryRequests.clear();
}

void DataServiceProxy::retryRequest(const RequestInfo &info) {
  sendRequest(info.endpoint, info.action, info.params, true);
}
