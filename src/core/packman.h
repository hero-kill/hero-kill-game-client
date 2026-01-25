// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _PACKMAN_H
#define _PACKMAN_H

class PackMan : public QObject {
  Q_OBJECT

public:
  PackMan(QObject *parent = nullptr);
  ~PackMan();

  Q_INVOKABLE QString getPackSummary();
  Q_INVOKABLE QStringList getDisabledPacks();
  Q_INVOKABLE void loadSummary(const QString &jsonData, bool useThread = false);
  Q_INVOKABLE void removePack(const QString &pack);

  bool shouldUseCore();

private:
  int clone(const QString &url);
  int pull(const QString &name);
  int updatePack(const QString &pack, const QString &hash);
  int checkout(const QString &name, const QString &hash);
  int resetWorkspace(const QString &name);
  int hasCommit(const QString &name, const QString &hash);
  int status(const QString &name);
  QString head(const QString &name);

  void loadPackagesJson();
  void savePackagesJson();
  void enablePack(const QString &pack);
  void disablePack(const QString &pack);
  void updatePackageInJson(const QString &name, const QString &url,
                           const QString &hash, bool enabled);

  QJsonArray packages;
  QStringList disabled_packs;
  QString packagesJsonPath;
};

extern PackMan *Pacman;

#endif
