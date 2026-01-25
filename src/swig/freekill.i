// SPDX-License-Identifier: GPL-3.0-or-later

%module fk

%{
#include "client/client.h"
#include "core/player.h"
#include "network/admin_service_proxy.h"
#include "ui/qmlbackend.h"
#include "core/util.h"

const char *FK_VER = FK_VERSION;
%}

%include "naturalvar.i"

%include "qt.i"
%include "player.i"
%include "client.i"

extern char *FK_VER;
QString GetDisabledPacks();

// QVariantMap typemap - 使用已有的 Lua::readValue 函数
%typemap(in) const QVariantMap & (QVariantMap temp) {
  QVariant v = Lua::readValue(L, $input);
  temp = v.toMap();
  $1 = &temp;
}

// QVariantMap 返回值 typemap - 将 QVariantMap 转换为 Lua table
%typemap(out) QVariantMap {
  Lua::pushValue(L, QVariant($1));
  SWIG_arg++;
}

// typecheck 用于函数重载识别
%typecheck(SWIG_TYPECHECK_POINTER) const QVariantMap & {
  $1 = lua_istable(L, $input);
}

// AdminService
%nodefaultctor AdminServiceProxy;
%nodefaultdtor AdminServiceProxy;
class AdminServiceProxy {
public:
  QString executeAsync(const QString &action, const QVariantMap &params = QVariantMap());
  QVariantMap execute(const QString &action, const QVariantMap &params = QVariantMap());
};

extern AdminServiceProxy *AdminService;

// QVariant AskOllama(const QString &apiEndpoint, const QVariant &body);
