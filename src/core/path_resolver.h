// SPDX-License-Identifier: GPL-3.0-or-later
// 路径解析工具：将 packages/standard[_cards] 映射到 packages/herokill-core/standard[_cards]

#ifndef PATH_RESOLVER_H
#define PATH_RESOLVER_H

#include <QString>

// 将 packages/standard[_cards]/... 映射到 packages/herokill-core/standard[_cards]/...
// 消除对符号链接的依赖，Windows 上 QFile::link 只能创建 .lnk 快捷方式
inline QString resolvePackagePath(const QString &path) {
  // 资源包路径不做转换，保持覆写能力
  if (path.contains("resource_pak/"))
    return path;

  QString p = path;
  p.replace("packages/standard_cards/", "packages/herokill-core/standard_cards/");
  p.replace("packages/standard/", "packages/herokill-core/standard/");
  // 路径以 packages/standard[_cards] 结尾（无尾 /）
  if (p.endsWith("packages/standard_cards"))
    p.replace(p.lastIndexOf("packages/standard_cards"), 23,
              "packages/herokill-core/standard_cards");
  else if (p.endsWith("packages/standard"))
    p.replace(p.lastIndexOf("packages/standard"), 17,
              "packages/herokill-core/standard");
  return p;
}

#endif // PATH_RESOLVER_H
