#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QSettings>
#include <QDebug>
#include <QProcess>
#include <QTextCodec>
#include <QSystemTrayIcon>
#include <QSharedMemory>
#include <QBuffer>
#include "qt_windows.h"

#include "macro.h"

struct fileInfo
{
    fileInfo(QString filename, QString version, QString hash) :
        filename(filename), version(version), hash(hash){}
    fileInfo(){}
    QString filename;
    QString version;
    QString hash;
};

struct fileListInfo
{
    QString version;
    QString update_url;
    QVector<fileInfo> file_list;
};

fileListInfo update_list_info;
QString _app_data_path;
QString _app_temp_path;

QString _user_app_path;
QString lang;
QString achain_config_path;

bool b_enable_log = false;

void writeLog(QString log)
{
    if (!b_enable_log) {
        return;
    }

    QFile file("./tools.log");
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out << log << endl;
    out.flush();
    file.close();
}

void getAppDataPath()
{
    QStringList environment = QProcess::systemEnvironment();
    QString str;

#ifdef WIN32
    _app_data_path = QDir::homePath() + "/AppData/Roaming/" + UPDATE_DIR;
    _app_temp_path = QDir::homePath() + "/AppData/Roaming/" + TEMP_DIR;
#endif
}

bool getUpdateSys()
{
    QString iniPath = _user_app_path + "/" + VERSION_CONFIG;
    writeLog("iniPath " + iniPath);
    QSettings config(iniPath, QSettings::IniFormat);
    if (config.contains("update")){
        writeLog("contains() update");
    }
    else {
        writeLog("contains() no update");
    }
    return config.value("update", false).toBool();
}

bool copyUpdate()
{
    QDir dir;
    dir.mkdir(_app_temp_path);
    writeLog("copyUpdate() 1");
    if (getUpdateSys()) {
        QString src_file_name = _app_data_path + "/" + UPDATE_TOOL_NAME;
        writeLog("copyUpdate() 3 src_file_name " + src_file_name);
        QFile src_file(src_file_name);
        if (!src_file.exists()) {
            writeLog("copyUpdate() 2");
            return false;
        }
        writeLog("copyUpdate() 3");
        bool ret = false;
        QString dst_file_name = _user_app_path + "/" + UPDATE_TOOL_NAME;
        writeLog("copyUpdate() 3 dst_file_name " + dst_file_name);
        QFile dst_file(dst_file_name);
        if (dst_file.exists()) {
            QFile temp_file(_app_temp_path + "/" + UPDATE_TOOL_NAME);
            if (temp_file.exists()) {
                temp_file.remove();
            }
            ret = dir.rename(dst_file_name, _app_temp_path + "/" + UPDATE_TOOL_NAME);
            dst_file.remove();
        }
        writeLog("copyUpdate() 4");

        if (!src_file.copy(dst_file_name)) {
            writeLog("copyUpdate() 5" + src_file.errorString());
            ret = dir.rename(_app_temp_path + "/" + UPDATE_TOOL_NAME, dst_file_name);
            return false;
        }
    }
    else {
        writeLog("getUpdateSys() false");
    }
    return true;
}

bool getUpdateFileInfo(QString file)
{
    QFile *download_file = new QFile(file);
    if (!download_file->open(QIODevice::ReadOnly)) {
        qDebug() << "open fail" << download_file->errorString();
        return false;
    }
    QByteArray data = download_file->readAll();
    delete download_file;
    download_file = nullptr;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    QJsonObject obj;
    if (error.error == QJsonParseError::NoError){
        obj = doc.object();
    }
    else {
        qDebug() << "QJson fail";
        return false;
    }

    update_list_info.version = obj.value("version").toString();

    QJsonArray arr = obj.value("file_list").toArray();
    int size = arr.size();
    update_list_info.file_list.resize(size);

    for (int i = 0; i < size; i++) {
        QJsonValue value = arr.at(i);
        fileInfo file_info;
        file_info.filename = value.toObject().value("file_name").toString();
        file_info.hash = value.toObject().value("hash").toString();
        file_info.version = value.toObject().value("version").toString();
        update_list_info.file_list.replace(i, file_info);
    }
    return true;
}

bool copyLocalFiles()
{
    int size = update_list_info.file_list.size();
    if (size == 0) {
        return true;
    }
    for (int i = 0; i < size; i++) {
        QString filename = update_list_info.file_list.at(i).filename;
        QString src_file_name = _app_data_path + "/" + update_list_info.version + "/" + filename;
        QFile src_file(src_file_name);
        src_file.open(QIODevice::ReadWrite);

        QString dst_file_name = _user_app_path + "/" + filename;
        QFile dst_file(dst_file_name);
        if (dst_file.exists()) {
            dst_file.remove();
        }
        dst_file.close();

        QFileInfo fileinfo(dst_file_name);
        QDir dir(fileinfo.dir());
        if (!dir.exists()) {
            dir.mkpath(fileinfo.absolutePath());
        }
        if (!src_file.copy(dst_file_name)) {
            qDebug() << "copy fail" << "dst" << dst_file_name;
            qDebug() << "src" << src_file_name;
            qDebug() << "cur path" << QDir::currentPath();
            qDebug() << src_file.errorString();
            src_file.close();
            return false;
        }
        src_file.close();
    }
    return true;
}

bool saveOldFileTemp()
{
    QDir dir;
    dir.mkdir(_app_temp_path);

    foreach(fileInfo fi, update_list_info.file_list) {
        QString bak_file_name = _user_app_path + "/" + fi.filename;
        QFile bak_file(bak_file_name);
        if (bak_file.exists()) {

            QFile src_file(bak_file_name);
            src_file.open(QIODevice::ReadWrite);

            QString dst_file_name = _app_temp_path + "/" + fi.filename;
            QFile dst_file(dst_file_name);
            if (dst_file.exists()) {
                dst_file.remove();
            }
            dst_file.close();

            QFileInfo fileinfo(dst_file_name);
            QDir dir(fileinfo.dir());
            if (!dir.exists()) {
                dir.mkpath(fileinfo.absolutePath());
            }
            if (!src_file.copy(dst_file_name)) {
                qDebug() << "copy fail" << "dst" << dst_file_name;
                qDebug() << "bak" << bak_file_name;
                qDebug() << src_file.errorString();
                src_file.close();
                return false;
            }
            src_file.close();
        }
        bak_file.close();
    }
    return true;
}

void resetOldFile()
{
    QDir dir;
    foreach(fileInfo fi, update_list_info.file_list) {
        QString dst_file_name = _user_app_path + "/" + fi.filename;
        QFile dst_file(dst_file_name);
        if (dst_file.exists()) {
            dst_file.remove();
        }
        dst_file.close();
        dir.rename(_app_temp_path + "/" + fi.filename, fi.filename);
    }
}

void deleteRollBack()
{
    QDir dir(_app_temp_path);
    dir.removeRecursively();
}

void deleteUpdate()
{
    QDir dir(_app_data_path);
    dir.removeRecursively();
}
void updateFinish()
{
    QSettings config(_user_app_path + "/" + VERSION_CONFIG, QSettings::IniFormat);
    config.setValue("version", update_list_info.version);
    config.setValue(UPDATE_SYS_MODE_UPDATE, false);
    config.setValue(UPDATE_SYS_MODE_DOWNLOAD, false);
}

void showTrayIcon(QApplication *app)
{
    QSystemTrayIcon *myTrayIcon = new QSystemTrayIcon(app);
    myTrayIcon->setIcon(QIcon(_user_app_path + "/" + "pic2/achain.ico"));
    myTrayIcon->setVisible(true);

    QSettings settings(achain_config_path, QSettings::IniFormat);
    lang = settings.value("/settings/language").toString();
    writeLog("showTrayIcon() lang " + lang);

    QString hint = QStringLiteral("提示");
    QString text = QStringLiteral("钱包升级成功，请稍候，正在启动...");
    if (lang.isEmpty() || 0 == QString::compare(lang, "English", Qt::CaseInsensitive)) {
        hint = QStringLiteral("Hint");
        text = QStringLiteral("wallet update success, please wait, starting ...");
    }

    myTrayIcon->showMessage(hint, text);
    Sleep(7000);
}

bool setUpgradeResult(QString result)
{
    QSharedMemory smem;
    QString key = "AchainUpShareKey";

    QBuffer buffer;
    QDataStream qdsm(&buffer);

    smem.setKey(key);
    if (smem.isAttached()) {
        smem.detach();
    }

    buffer.open(QBuffer::ReadWrite);
    qdsm << result;

    int size = buffer.size();
    if (!smem.create(size)) {
        return false;
    }

    smem.lock();
    char *to = (char *)smem.data();
    const char *from = buffer.data().data();
    memcpy(to, from, qMin(smem.size(), size));
    smem.unlock();

    Sleep(7000);

    return true;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString path = QCoreApplication::applicationDirPath();

    getAppDataPath();

    bool ret = false;
    writeLog("argc " + QString::number(argc));
    if (argc > 1) {
        _user_app_path = QString::fromLocal8Bit(argv[1]);
    }
    else {
        _user_app_path = path;
    }
    writeLog("_user_app_path " + _user_app_path);
    if (argc > 2) {
        achain_config_path = QString::fromLocal8Bit(argv[2]);
    }
    else {
        achain_config_path = "";
    }
    writeLog("achain_config_path " + achain_config_path);
    if (argc > 3) {
        QString mode = QString::fromLocal8Bit(argv[3]);
        writeLog("mode " + mode);
        if ("update" == mode) {
            if (copyUpdate()) {
                writeLog("copyUpdate() true");
                ret = true;
                QString upPath = _user_app_path + "/" + UPDATE_TOOL_NAME;
                QProcess::startDetached(upPath, QStringList() << "update" << achain_config_path);
            }
        }
        app.quit();
        return ret;
    }

    // other files
    QString update_filename = _app_data_path + "/" + UPDATE_LIST_FILE;
    if (!getUpdateFileInfo(update_filename)) {
    }
    else {
        if (saveOldFileTemp()) { // bak success
            if (!copyLocalFiles()) {
                resetOldFile();
            }
            else { // copy success
                ret = true;
                updateFinish();
                deleteUpdate();
                setUpgradeResult("succ");
                QString achainPath = _user_app_path + "/" + SYS_NAME;
                bool result = QProcess::startDetached(achainPath, QStringList(achainPath));
                writeLog("achainPath " + achainPath);
                writeLog(result ? "successed" : "failed");
            }
            deleteRollBack();
            setUpgradeResult("fail");
        }
    }

    app.quit();
    return ret;
}
