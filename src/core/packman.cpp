// SPDX-License-Identifier: GPL-3.0-or-later

#include <git2.h>
#include <git2/errors.h>
#include "core/packman.h"
#include "ui/qmlbackend.h"

PackMan *Pacman = nullptr;

PackMan::PackMan(QObject *parent) : QObject(parent) {
  git_libgit2_init();
  packagesJsonPath = "./packages/packages.json";
  loadPackagesJson();

#ifdef Q_OS_ANDROID
  git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS, NULL, "./certs");
#endif
}

PackMan::~PackMan() {
  git_libgit2_shutdown();
}

QStringList PackMan::getDisabledPacks() {
  return disabled_packs;
}

QString PackMan::getPackSummary() {
  QJsonArray enabledPackages;
  for (const auto &pkg : packages) {
    auto obj = pkg.toObject();
    if (obj["enabled"].toBool(true)) {
      QJsonObject summary;
      summary["name"] = obj["name"];
      summary["url"] = obj["url"];
      summary["hash"] = obj["hash"];
      enabledPackages.append(summary);
    }
  }
  return QJsonDocument(enabledPackages).toJson(QJsonDocument::Compact);
}

void PackMan::loadPackagesJson() {
  QFile file(packagesJsonPath);
  if (file.open(QIODevice::ReadOnly)) {
    auto doc = QJsonDocument::fromJson(file.readAll());
    packages = doc.array();
    file.close();

    for (const auto &pkg : packages) {
      auto obj = pkg.toObject();
      if (!obj["enabled"].toBool(true)) {
        disabled_packs << obj["name"].toString();
      }
    }
  }
}

void PackMan::savePackagesJson() {
  QDir().mkpath(QFileInfo(packagesJsonPath).absolutePath());
  QFile file(packagesJsonPath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(packages).toJson());
    file.close();
  }
}

void PackMan::updatePackageInJson(const QString &name, const QString &url,
                                   const QString &hash, bool enabled) {
  for (int i = 0; i < packages.size(); i++) {
    auto obj = packages[i].toObject();
    if (obj["name"].toString() == name) {
      obj["url"] = url;
      obj["hash"] = hash;
      obj["enabled"] = enabled;
      packages[i] = obj;
      return;
    }
  }
  QJsonObject newPkg;
  newPkg["name"] = name;
  newPkg["url"] = url;
  newPkg["hash"] = hash;
  newPkg["enabled"] = enabled;
  packages.append(newPkg);
}

void PackMan::enablePack(const QString &pack) {
  for (int i = 0; i < packages.size(); i++) {
    auto obj = packages[i].toObject();
    if (obj["name"].toString() == pack) {
      obj["enabled"] = true;
      packages[i] = obj;
      break;
    }
  }
  disabled_packs.removeOne(pack);
}

void PackMan::disablePack(const QString &pack) {
  for (int i = 0; i < packages.size(); i++) {
    auto obj = packages[i].toObject();
    if (obj["name"].toString() == pack) {
      obj["enabled"] = false;
      packages[i] = obj;
      break;
    }
  }
  if (!disabled_packs.contains(pack))
    disabled_packs << pack;
}

void PackMan::removePack(const QString &pack) {
  for (int i = 0; i < packages.size(); i++) {
    auto obj = packages[i].toObject();
    if (obj["name"].toString() == pack) {
      packages.removeAt(i);
      break;
    }
  }
  QDir d(QString("packages/%1").arg(pack));
  d.removeRecursively();
  savePackagesJson();
}

void PackMan::loadSummary(const QString &jsonData, bool useThread) {
  auto f = [=, this]() {
    for (int i = 0; i < packages.size(); i++) {
      auto obj = packages[i].toObject();
      obj["enabled"] = false;
      packages[i] = obj;
      if (!disabled_packs.contains(obj["name"].toString()))
        disabled_packs << obj["name"].toString();
    }

    auto doc = QJsonDocument::fromJson(jsonData.toUtf8());
    auto arr = doc.array();
    for (auto e : arr) {
      auto obj = e.toObject();
      auto name = obj["name"].toString();
      auto url = obj["url"].toString();
      auto hash = obj["hash"].toString();
      int err = 0;

#ifndef FK_SERVER_ONLY
      Backend->notifyUI("SetDownloadingPackage", name);
#endif

      QString packPath = QString("packages/%1").arg(name);
      if (!QDir(packPath).exists()) {
        err = clone(url);
        if (err != 0) {
#ifndef FK_SERVER_ONLY
          QString msg;
          if (err != 100) {
            auto error = git_error_last();
            msg = QString("Error: %1").arg(error ? error->message : "Unknown");
          } else {
            msg = "Workspace is dirty.";
          }
          Backend->notifyUI("PackageDownloadError", msg);
#endif
          continue;
        }
      }

      enablePack(name);

      // 先检查工作区状态，如果脏就重置
      err = status(name);
      if (err == 100) {
        qInfo() << "Workspace is dirty, resetting:" << name;
        err = resetWorkspace(name);
        if (err != 0) {
          qCritical() << "Failed to reset workspace:" << name;
        }
      }

      if (head(name) != hash) {
        err = updatePack(name, hash);
        if (err != 0) {
#ifndef FK_SERVER_ONLY
          QString msg;
          if (err != 100) {
            auto error = git_error_last();
            msg = QString("Error: %1").arg(error ? error->message : "Unknown");
          } else {
            msg = "Workspace is dirty.";
          }
          Backend->notifyUI("PackageDownloadError", msg);
#endif
          continue;
        }
      }

      updatePackageInJson(name, url, hash, true);
    }

    savePackagesJson();
  };

  if (useThread) {
    auto thread = QThread::create(f);
    thread->start();
    connect(thread, &QThread::finished, [=]() {
      thread->deleteLater();
#ifndef FK_SERVER_ONLY
      Backend->notifyUI("DownloadComplete", "");
#endif
    });
  } else {
    f();
  }
}

int PackMan::updatePack(const QString &pack, const QString &hash) {
  int err;
  err = status(pack);
  if (err == 100) {
    // 工作区脏，自动重置
    qInfo() << "Workspace is dirty, resetting:" << pack;
    err = resetWorkspace(pack);
    if (err != 0) {
      qCritical() << "Failed to reset workspace:" << pack;
      return err;
    }
  } else if (err != 0) {
    return err;
  }

  err = hasCommit(pack, hash);
  if (err != 0) {
    err = pull(pack);
    if (err < 0)
      return err;
  }
  err = checkout(pack, hash);
  if (err < 0)
    return err;
  return 0;
}

#define GIT_FAIL                                                               \
  const git_error *e = git_error_last();                                       \
  qCritical("Error %d/%d: %s\n", err, e ? e->klass : 0, e ? e->message : "Unknown")

#define GIT_CHK_CLEAN  \
  if (err < 0) {     \
    GIT_FAIL;          \
    goto clean;        \
  }

static int transfer_progress_cb(const git_indexer_progress *stats, void *payload) {
  Q_UNUSED(payload);
#ifndef FK_SERVER_ONLY
  if (Backend != nullptr) {
    Backend->notifyUI("PackageTransferProgress", QJsonObject {
      { "received_objects", qint64(stats->received_objects) },
      { "total_objects", qint64(stats->total_objects) },
      { "indexed_objects", qint64(stats->indexed_objects) },
      { "received_bytes", qint64(stats->received_bytes) },
      { "indexed_deltas", qint64(stats->indexed_deltas) },
      { "total_deltas", qint64(stats->total_deltas) },
    });
  }
#endif
  return 0;
}

int PackMan::clone(const QString &u) {
  git_repository *repo = NULL;
  auto url = u;
  while (url.endsWith('/')) {
    url.chop(1);
  }
  QString fileName = QUrl(url).fileName();
  if (fileName.endsWith(".git"))
    fileName.chop(4);
  fileName = QStringLiteral("packages/") + fileName;

  git_clone_options opt;
  git_clone_init_options(&opt, GIT_CLONE_OPTIONS_VERSION);
  opt.fetch_opts.proxy_opts.version = 1;
  opt.fetch_opts.callbacks.transfer_progress = transfer_progress_cb;
  int err = git_clone(&repo, url.toUtf8(), fileName.toUtf8(), &opt);
  if (err < 0) {
    GIT_FAIL;
  }
  git_repository_free(repo);
  return err;
}

int PackMan::pull(const QString &name) {
  git_repository *repo = NULL;
  int err;
  git_remote *remote = NULL;
  auto path = QString("packages/%1").arg(name).toUtf8();
  git_fetch_options opt;
  git_fetch_init_options(&opt, GIT_FETCH_OPTIONS_VERSION);
  opt.proxy_opts.version = 1;
  opt.callbacks.transfer_progress = transfer_progress_cb;

  git_checkout_options opt2 = GIT_CHECKOUT_OPTIONS_INIT;
  opt2.checkout_strategy = GIT_CHECKOUT_FORCE;

  err = git_repository_open(&repo, path);
  GIT_CHK_CLEAN;

  err = git_remote_lookup(&remote, repo, "origin");
  GIT_CHK_CLEAN;

  err = git_remote_fetch(remote, NULL, &opt, NULL);
  GIT_CHK_CLEAN;

  err = git_repository_set_head(repo, "FETCH_HEAD");
  GIT_CHK_CLEAN;

  err = git_checkout_head(repo, &opt2);
  GIT_CHK_CLEAN;

clean:
  git_remote_free(remote);
  git_repository_free(repo);
  return err;
}

int PackMan::hasCommit(const QString &name, const QString &hash) {
  git_repository *repo = NULL;
  int err;
  git_oid oid = {0};
  git_commit *commit = NULL;

  auto path = QString("packages/%1").arg(name).toUtf8();
  auto sha = hash.toLatin1();
  err = git_repository_open(&repo, path);
  GIT_CHK_CLEAN;
  err = git_oid_fromstr(&oid, sha);
  GIT_CHK_CLEAN;
  err = git_commit_lookup(&commit, repo, &oid);

clean:
  git_commit_free(commit);
  git_repository_free(repo);
  return err;
}

int PackMan::checkout(const QString &name, const QString &hash) {
  git_repository *repo = NULL;
  int err;
  git_oid oid = {0};
  git_checkout_options opt = GIT_CHECKOUT_OPTIONS_INIT;
  opt.checkout_strategy = GIT_CHECKOUT_FORCE;
  auto path = QString("packages/%1").arg(name).toUtf8();
  auto sha = hash.toLatin1();
  err = git_repository_open(&repo, path);
  GIT_CHK_CLEAN;
  err = git_oid_fromstr(&oid, sha);
  GIT_CHK_CLEAN;
  err = git_repository_set_head_detached(repo, &oid);
  GIT_CHK_CLEAN;
  err = git_checkout_head(repo, &opt);
  GIT_CHK_CLEAN;

clean:
  git_repository_free(repo);
  return err;
}

int PackMan::resetWorkspace(const QString &name) {
  git_repository *repo = NULL;
  int err;
  git_checkout_options opt = GIT_CHECKOUT_OPTIONS_INIT;
  opt.checkout_strategy = GIT_CHECKOUT_FORCE;
  auto path = QString("packages/%1").arg(name).toUtf8();

  err = git_repository_open(&repo, path);
  GIT_CHK_CLEAN;

  err = git_checkout_head(repo, &opt);
  GIT_CHK_CLEAN;

clean:
  git_repository_free(repo);
  return err;
}

int PackMan::status(const QString &name) {
  git_repository *repo = NULL;
  int err;
  git_status_list *status_list = NULL;
  size_t i, maxi;
  const git_status_entry *s;
  auto path = QString("packages/%1").arg(name).toUtf8();
  err = git_repository_open(&repo, path);
  GIT_CHK_CLEAN;
  err = git_status_list_new(&status_list, repo, NULL);
  GIT_CHK_CLEAN;
  maxi = git_status_list_entrycount(status_list);
  for (i = 0; i < maxi; ++i) {
    s = git_status_byindex(status_list, i);
    if (s->status != GIT_STATUS_CURRENT && s->status != GIT_STATUS_IGNORED) {
      git_status_list_free(status_list);
      git_repository_free(repo);
      qCritical("Workspace is dirty.");
      return 100;
    }
  }

clean:
  git_status_list_free(status_list);
  git_repository_free(repo);
  return err;
}

QString PackMan::head(const QString &name) {
  git_repository *repo = NULL;
  int err;
  git_object *obj = NULL;
  const git_oid *oid;
  char buf[42] = {0};
  auto path = QString("packages/%1").arg(name).toUtf8();
  err = git_repository_open(&repo, path);
  GIT_CHK_CLEAN;
  err = git_revparse_single(&obj, repo, "HEAD");
  GIT_CHK_CLEAN;

  oid = git_object_id(obj);
  git_oid_tostr(buf, 41, oid);
  git_object_free(obj);
  git_repository_free(repo);
  return QString(buf);

clean:
  git_object_free(obj);
  git_repository_free(repo);
  return QString("0000000000000000000000000000000000000000");
}

static bool is_head_newer_than_commit(const char *repo_path, const char *commit_hash) {
  git_repository *repo = NULL;
  git_commit *given_commit = NULL;
  git_commit *head_commit = NULL;
  bool result = false;

  git_libgit2_init();

  if (git_repository_open(&repo, repo_path) != 0) {
    goto cleanup;
  }

  git_oid given_oid;
  if (git_oid_fromstr(&given_oid, commit_hash) != 0) {
    goto cleanup;
  }

  if (git_commit_lookup(&given_commit, repo, &given_oid) != 0) {
    goto cleanup;
  }

  git_oid head_oid;
  if (git_reference_name_to_id(&head_oid, repo, "HEAD") != 0) {
    goto cleanup;
  }

  if (git_commit_lookup(&head_commit, repo, &head_oid) != 0) {
    goto cleanup;
  }

  if (git_graph_descendant_of(repo, &head_oid, &given_oid) == 1) {
    result = true;
  }

  if (strncmp((char*)head_oid.id, (char*)given_oid.id, 20) == 0) {
    result = true;
  }

cleanup:
  git_commit_free(given_commit);
  git_commit_free(head_commit);
  git_repository_free(repo);
  git_libgit2_shutdown();

  return result;
}

static const char *min_commit = "b57d89fa4c1a1ae5a0711b97598747b8cbc7428e";

bool PackMan::shouldUseCore() {
  if (!QFile::exists("packages/herokill-core")) return false;
  if (disabled_packs.contains("herokill-core")) return false;
  bool ret = is_head_newer_than_commit("packages/herokill-core", min_commit);
  return ret;
}

#undef GIT_FAIL
#undef GIT_CHK_CLEAN
