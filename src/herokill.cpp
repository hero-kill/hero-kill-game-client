// SPDX-License-Identifier: GPL-3.0-or-later

#include "client/client.h"
#include "client/update_client.h"
#include "core/util.h"
#include "core/c-wrapper.h"
#include "core/packman.h"
using namespace fkShell;

#if defined(Q_OS_WIN32)
#include "applink.c"
#endif

#ifndef FK_SERVER_ONLY
 #include <QFileDialog>
 #include <QScreen>
 #include <QSplashScreen>
 #include <QtQuick/QQuickWindow>
 #include <QSurfaceFormat>
 #ifndef Q_OS_ANDROID
  #include <QQuickStyle>
 #else
  // #include <QNativeInterface>
 #endif
 #include "ui/qmlbackend.h"
 #include "network/data_service_proxy.h"
#endif

#include <QTextStream>

#if defined(Q_OS_ANDROID)
static bool copyPath(const QString &srcFilePath, const QString &tgtFilePath) {
  QFileInfo srcFileInfo(srcFilePath);
  if (srcFileInfo.isDir()) {
    QDir targetDir(tgtFilePath);
    if (!targetDir.exists()) {
      targetDir.cdUp();
      if (!targetDir.mkdir(QFileInfo(tgtFilePath).fileName()))
        return false;
    }
    QDir sourceDir(srcFilePath);
    QStringList fileNames =
        sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot |
                            QDir::Hidden | QDir::System);
    for (const QString &fileName : fileNames) {
      const QString newSrcFilePath = srcFilePath + QLatin1Char('/') + fileName;
      const QString newTgtFilePath = tgtFilePath + QLatin1Char('/') + fileName;
      if (!copyPath(newSrcFilePath, newTgtFilePath))
        return false;
    }
  } else {
    QFile::remove(tgtFilePath);
    if (!QFile::copy(srcFilePath, tgtFilePath))
      return false;
  }
  return true;
}
#endif

static void installFkAssets(const QString &src, const QString &dest) {
  QFile f(dest + "/fk_ver");
  if (f.exists() && f.open(QIODevice::ReadOnly)) {
    auto ver = f.readLine().simplified();
    if (ver == FK_VERSION) {
      return;
    }
  }
#ifdef Q_OS_ANDROID
  copyPath(src, dest);
#elif defined(Q_OS_LINUX)
  system(QString("cp -r %1 %2/..").arg(src).arg(dest).toUtf8());
#endif
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <stdlib.h>
#include <unistd.h>
static void prepareForLinux() {
  // 如果用户执行的是 /usr/bin/HeroKill，那么这意味着是被包管理器安装
  // 的，所以我们就需要把资源文件都复制到 ~/.local 中，并且切换当前目录
  // TODO: AppImage
  char buf[256] = {0};
  int len = readlink("/proc/self/exe", buf, 256);
  const char *home = getenv("HOME");
  if (!strcmp(buf, "/usr/bin/HeroKill")) {
    system("mkdir -p ~/.local/share/HeroKill");
    installFkAssets("/usr/share/HeroKill", QString("%1/.local/share/HeroKill").arg(home));
    chdir(home);
    chdir(".local/share/HeroKill");
  } else if (!strcmp(buf, "/usr/local/bin/HeroKill")) {
    system("mkdir -p ~/.local/share/HeroKill");
    installFkAssets("/usr/local/share/HeroKill", QString("%1/.local/share/HeroKill").arg(home));
    chdir(home);
    chdir(".local/share/HeroKill");
  }
}
#endif

static std::unique_ptr<QFile> log_file(nullptr);

void fkMsgHandler(QtMsgType type, const QMessageLogContext &context,
                  const QString &msg) {
  auto date = QDate::currentDate();

  std::unique_ptr<QTextStream> ofs(nullptr);
  ofs.reset(new QTextStream(log_file.get()));

  printf("\r");

  auto threadName = QThread::currentThread()->objectName();
  QString levelMark = "D";
  const char *levelMarkNoColor = "D";

  switch (type) {
  case QtDebugMsg:
    break;
  case QtInfoMsg:
    levelMark = Color("I", Green);
    levelMarkNoColor = "I";
    break;
  case QtWarningMsg:
    levelMark = Color("W", Yellow, Bold);
    levelMarkNoColor = "W";
    break;
  case QtCriticalMsg:
    levelMark = Color("C", Red, Bold);
    levelMarkNoColor = "C";
#ifndef FK_SERVER_ONLY
    if (Backend != nullptr) {
      Backend->notifyUI("ErrorDialog",
          QString("⛔ %1/Error occured!\n  %2").arg(threadName).arg(msg));
    }
#endif
    break;
  case QtFatalMsg:
    levelMark = Color("E", Red, Bold);
    levelMarkNoColor = "E";
    break;
  }

  auto dateStr = QString::asprintf("%02d/%02d", date.month(), date.day());
  auto timeStr = QTime::currentTime().toString("hh:mm:ss");
#ifndef Q_OS_WIN32
  QTextStream out(stdout);
  out << dateStr << " " << timeStr << " " << threadName <<
    "[" << levelMark << "] " << msg << Qt::endl;
#else
  // 略win区，你赢了
  // 但至少win肯定支持wchar_t，%ls放心用
  printf("%ls %ls %ls[%ls] %ls\r\n", qUtf16Printable(dateStr),
         qUtf16Printable(timeStr), qUtf16Printable(threadName),
         qUtf16Printable(levelMark), qUtf16Printable(msg));
#endif
  *ofs << dateStr << " " << timeStr << " " << threadName <<
    "[" << levelMarkNoColor << "] " << msg << Qt::endl;

}

#ifndef FK_SERVER_ONLY
static QQmlApplicationEngine *engine = nullptr;
#endif

static void cleanUpGlobalStates() {
#ifndef FK_SERVER_ONLY
  if (engine) delete engine;
  if (Backend) delete Backend;
#endif

  if (ClientInstance) delete ClientInstance;
  // if (ServerInstance) delete ServerInstance;
  if (Pacman) delete Pacman;

  qInstallMessageHandler(nullptr);
  qApp->deleteLater();
}

static int runSkillTest(const QString &val, const QString &filepath) {
  auto L = new Lua;
  L->eval("__os = os; __io = io; __package = package; __dofile = dofile"); // 保存一下

  QString script;

  QString originalPath = QDir::currentPath();
  QString coreRoot = originalPath + "/packages/herokill-core";
  QDir::setCurrent(coreRoot);
  QByteArray coreLua = (coreRoot + "/lua/herokill.lua").toUtf8();
  int ret = 1;
  if (!L->dofile(coreLua.constData())) goto RET;
  QDir::setCurrent(originalPath);

  if (val == "") {
    if (filepath != "") {
      QString fp = filepath;
      fp.replace("\\", "\\\\");
      script = QStringLiteral(
        R"(
        local skels = { __dofile('%1') }
        skels = table.map(skels, function(skel)
          local skill = Skill:new(skel.name)
          return string.format('Test%s', skill.name)
        end)
        return lu.LuaUnit.run(table.unpack(skels))
      )").arg(fp);
    } else {
      script = QStringLiteral("return lu.LuaUnit.run()");
    }
  } else {
    auto splitted = QStringList();
    for (auto s : val.split(',')) {
      splitted << ("\"Test" + s + '"');
    }
    script = QStringLiteral("return lu.LuaUnit.run( %1 )").arg(splitted.join(", "));
  }

  if (!L->dofile("test/lua/cpp_run_skill.lua")) goto RET;
  ret = L->eval(script).toInt();

RET:
  delete L;
  return ret;
}

// HeroKill 的程序主入口。整个程序就是从这里开始执行的。
int herokill_main(int argc, char *argv[]) {
  // 初始化一下各种杂项信息
  QThread::currentThread()->setObjectName("Main");

  qInstallMessageHandler(fkMsgHandler);
  QCoreApplication *app;
  QCoreApplication::setApplicationName("HeroKill");
  QCoreApplication::setApplicationVersion(FK_VERSION);

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
  prepareForLinux();
#endif

  if (!log_file) {
    log_file.reset(new QFile("herokill.server.log"));
    if (!log_file->open(QIODevice::WriteOnly | QIODevice::Text)) {
      qFatal("Cannot open info.log");
    }
  }

  // 分析命令行，如果有 -s 或者 --server 就在命令行直接开服务器
  QCommandLineParser parser;
  parser.setApplicationDescription("HeroKill client");
  parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
  parser.addVersionOption();
  parser.addOption({{"h", "help"}, "display help information"});
  parser.addOption({"testskills", "run test case of skills", "testskills"});
  parser.addOption({"testfile", "run test case of a skill file", "testfile"});
  QStringList cliOptions;
  for (int i = 0; i < argc; i++)
    cliOptions << argv[i];

  parser.parse(cliOptions);
  if (parser.isSet("version")) {
    parser.showVersion();
    return 0;
  } else if (parser.isSet("help")) {
    parser.showHelp();
    return 0;
  } else if (parser.isSet("testskills")) {
    auto val = parser.value("testskills");
    return runSkillTest(val, "");
  } else if (parser.isSet("testfile")) {
    auto val = parser.value("testfile");
    return runSkillTest("", val);
  }

  app = new QApplication(argc, argv);
  app->connect(app, &QCoreApplication::aboutToQuit, cleanUpGlobalStates);
#ifdef DESKTOP_BUILD
  ((QApplication *)app)->setWindowIcon(QIcon("image/icon.png"));
#endif

#ifdef Q_OS_WIN32
  // 设置 QML 使用 OpenGL 渲染
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#elif defined(Q_OS_ANDROID)
  // 设置 QML 使用 OpenGL 渲染
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
  // 在此基础上再指定使用OpenGL ES
  {
    auto fmt = QSurfaceFormat::defaultFormat();
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    QSurfaceFormat::setDefaultFormat(fmt);
  }
#endif

#define SHOW_SPLASH_MSG(msg)                                                   \
  splash.showMessage(msg, Qt::AlignHCenter | Qt::AlignBottom);

#ifdef Q_OS_ANDROID
  // 投降喵，设为android根本无效
  // 直接改用Android原生Mediaplayer了，不用你Qt家的
  // qputenv("QT_MEDIA_BACKEND", "android");

  // 禁用accessible（可能会流畅点但没有证据 纯抓瞎
  qputenv("QT_ANDROID_DISABLE_ACCESSIBILITY", "1");

  qputenv("ANDROID_OPENSSL_SUFFIX", "_3");

  // 安卓：从Qt 6.8起需要别的办法拿activity
  // 参考文献 https://forum.qt.io/topic/159350/qt-6-8-0-replacement-for-qtnative-activity
  QJniObject::callStaticMethod<void>(
    "org/notify/HeroKill/Helper", "SetActivity",
    "(Landroid/app/Activity;)V", QNativeInterface::QAndroidApplication::context().object()
  );

  // 安卓：获取系统语言需要使用Java才行
  QString localeName = QJniObject::callStaticObjectMethod("org/notify/HeroKill/Helper", "GetLocaleCode", "()Ljava/lang/String;").toString();

  // 安卓：先切换到我们安装程序的那个外部存储目录去
  QJniObject::callStaticMethod<void>("org/notify/HeroKill/Helper", "InitView", "()V");
  QDir::setCurrent("/storage/emulated/0/Android/data/org.notify.HeroKill/files");

  // 切目录后重新设置log文件路径
  log_file.reset(new QFile("herokill.server.log"));
  if (!log_file->open(QIODevice::WriteOnly | QIODevice::Text)) {
    qFatal("Cannot open info.log");
  }

  // 然后显示欢迎界面，并在需要时复制资源素材等
  QScreen *screen = qobject_cast<QApplication *>(app)->primaryScreen();
  QRect screenGeometry = screen->geometry();
  int screenWidth = screenGeometry.width();
  int screenHeight = screenGeometry.height();
  QSplashScreen splash(QPixmap("assets:/res/image/splash.png")
                           .scaled(screenWidth, screenHeight));
  splash.showFullScreen();
  SHOW_SPLASH_MSG("Copying resources...");
  installFkAssets("assets:/res", QDir::currentPath());
#else
  // 不是安卓，使用QLocale获得系统语言
  QLocale l = QLocale::system();
  auto localeName = l.name();

  // 不是安卓，那么直接启动欢迎界面，也就是不复制东西了
  QSplashScreen splash(QPixmap("image/splash.png"));
  splash.show();
#endif

  SHOW_SPLASH_MSG("Loading qml files...");
  engine = new QQmlApplicationEngine;

#ifndef Q_OS_ANDROID
  QQuickStyle::setStyle("Material");
#endif


  Backend = new QmlBackend;
  Backend->setEngine(engine);

  DataService = new DataServiceProxy;

  Pacman = new PackMan;

  // 创建符号链接，让 packages/standard 指向 packages/herokill-core/standard
  QFile::link("herokill-core/standard", "packages/standard");
  QFile::link("herokill-core/standard_cards", "packages/standard_cards");


  // 向 Qml 中先定义几个全局变量
  auto root = engine->rootContext();
  root->setContextProperty("FkVersion", FK_VERSION);
  root->setContextProperty("Backend", Backend);
  root->setContextProperty("DataService", DataService);
  root->setContextProperty("Pacman", Pacman);
  root->setContextProperty("ModBackend", nullptr);
  root->setContextProperty("SysLocale", localeName);

  // 创建 UpdateClient 用于预登录更新检查
  auto updateClient = new UpdateClient(engine);
  root->setContextProperty("UpdateClient", updateClient);

#ifdef QT_DEBUG
  bool debugging = true;
#else
  bool debugging = false;
#endif
  engine->rootContext()->setContextProperty("Debugging", debugging);

  QString system;
#if defined(Q_OS_ANDROID)
  system = QStringLiteral("Android");
#elif defined(Q_OS_WIN32)
  qputenv("QT_MEDIA_BACKEND", "windows");
  system = QStringLiteral("Win");
  ::system("chcp 65001");
#elif defined(Q_OS_LINUX)
  system = QStringLiteral("Linux");
#else
  system = QStringLiteral("Other");
#endif
  root->setContextProperty("OS", system);

  root->setContextProperty(
      "AppPath", QUrl::fromLocalFile(QDir::currentPath()));

  // 加载GUI了，如果core有的话用core的
  engine->addImportPath("packages/herokill-core");
  engine->load("packages/herokill-core/Fk/main.qml");

  // qml 报错了就直接退出吧
  if (engine->rootObjects().isEmpty())
    return -1;

  // 关闭欢迎界面，然后进入Qt主循环
  splash.close();
  int ret = app->exec();

  return ret;
}
