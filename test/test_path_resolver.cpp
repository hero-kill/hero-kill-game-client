// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include "core/path_resolver.h"

class TestPathResolver : public QObject {
  Q_OBJECT

private slots:
  // 相对路径: ./packages/standard/...
  void relativeStandard() {
    QCOMPARE(resolvePackagePath("./packages/standard/init.lua"),
             QString("./packages/herokill-core/standard/init.lua"));
  }

  void relativeStandardCards() {
    QCOMPARE(resolvePackagePath("./packages/standard_cards/audio/card/halberd"),
             QString("./packages/herokill-core/standard_cards/audio/card/halberd"));
  }

  // 相对路径无 ./ 前缀
  void relativeNoPrefix() {
    QCOMPARE(resolvePackagePath("packages/standard/pkg/skills/guose.lua"),
             QString("packages/herokill-core/standard/pkg/skills/guose.lua"));
  }

  // 绝对路径
  void absoluteStandard() {
    QCOMPARE(resolvePackagePath("/Users/xxx/packages/standard/image/anim/slash"),
             QString("/Users/xxx/packages/herokill-core/standard/image/anim/slash"));
  }

  void absoluteStandardCards() {
    QCOMPARE(resolvePackagePath("/abs/path/packages/standard_cards/audio/x.wav"),
             QString("/abs/path/packages/herokill-core/standard_cards/audio/x.wav"));
  }

  // resource_pak 路径: 不做转换
  void resourcePakSkipped() {
    const QString input = "resource_pak/foo/packages/standard/image/y.png";
    QCOMPARE(resolvePackagePath(input), input);
  }

  void resourcePakAbsoluteSkipped() {
    const QString input = "/abs/path/resource_pak/bar/packages/standard_cards/audio/z.wav";
    QCOMPARE(resolvePackagePath(input), input);
  }

  // 无尾 / 的情况
  void noTrailingSlashStandard() {
    QCOMPARE(resolvePackagePath("packages/standard"),
             QString("packages/herokill-core/standard"));
  }

  void noTrailingSlashStandardCards() {
    QCOMPARE(resolvePackagePath("packages/standard_cards"),
             QString("packages/herokill-core/standard_cards"));
  }

  // 已经是正确路径: 不应重复转换
  void alreadyResolved() {
    const QString input = "packages/herokill-core/standard/init.lua";
    QCOMPARE(resolvePackagePath(input), input);
  }

  void alreadyResolvedCards() {
    const QString input = "packages/herokill-core/standard_cards/audio/card/halberd";
    QCOMPARE(resolvePackagePath(input), input);
  }

  // 空字符串
  void emptyString() {
    QCOMPARE(resolvePackagePath(""), QString(""));
  }

  // 不含 packages/standard 的普通路径
  void unrelatedPath() {
    const QString input = "./audio/system/ready";
    QCOMPARE(resolvePackagePath(input), input);
  }

  // file:// URL 格式（QML 传入）
  void fileUrlFormat() {
    QCOMPARE(
      resolvePackagePath("file:///Users/xxx/./packages/standard_cards/image/anim/slash"),
      QString("file:///Users/xxx/./packages/herokill-core/standard_cards/image/anim/slash"));
  }
};

QTEST_MAIN(TestPathResolver)
#include "test_path_resolver.moc"
