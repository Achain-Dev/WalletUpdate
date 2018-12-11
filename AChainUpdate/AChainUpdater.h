#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QVector>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QCryptographicHash>
#include <QtCore/QCoreApplication>
#include <QProcess>
#include <QDesktopServices>
#include <QSettings>
#include <QTimer>

#include "qt_windows.h"
#include "macro.h"

struct updateInfo
{
    QString update_version;
    QString update_url;
    QString update_system_name;
    QString update_system_md5;
    QString update_index_file;
};

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

enum UPDATE_MODE {
    UPDATE,
    DOWNLOAD,
    INSTALL,
};

class AChainUpdater : public QObject
{
    Q_OBJECT

public:
    AChainUpdater();
    ~AChainUpdater();

    //void setUpdateMode();
    void getAppDataPath();
    void updateRunTool();
    void updateUpdateSys();
    void startUpdate();
    bool createUpdateFile();
    bool unzipUpdateFile();
    void fileRequest(const QUrl& url);
    void jsonResolve();
    void json2FileListInfo(QJsonObject& obj, fileListInfo& list_info);
    void json2UpdateInfo(QJsonObject& obj, updateInfo& update_info);
    bool updateFileCheck();
    HANDLE ReleseResourceExe(WORD wResourceId, const TCHAR *pszFileName);
	bool copyFolderTo(QString oriPath, QString destPath, bool coverFileIfExist = true);
    void runToolAsAdmin(bool isUpdate=false);
    void getCurrentFileMD5Info(QString path, QVector<fileInfo>& file_list);
    void saveLocalUpdateFileInfo();
    void fileInfo2Json(QVector<fileInfo> &file_list, QJsonObject& updatejson);

    void getVersion();
    void saveVersion();
    void downloadNoFile();
    void downloadFile();
    void downloadUpdate();
    void downloadNoUpdate();

    QString getFileAttrVersion(QString fullName);
    void findUpdateIndex();
    void writeLog(QString log);
    void setLang(QString lang);
    void setConfigPath(QString configPath);

signals:
    void sysFinished();

private:
    QUrl download_url_;
    QString app_path;
    QString _app_data_path;

    QFile *update_index_;
    QFile *download_file_;

    QNetworkAccessManager net_manager_;
    QNetworkReply *net_reply_;
    QVector <QUrl> url_list_;

    QUrl base_url;

    updateInfo update_info_;
    fileListInfo list_info_;
    QVector<fileInfo> current_list_info_;
    QString current_version;
    QVector<fileInfo> update_list_info_;
    QString update_path;// Update File Path

    int md5_check_error_cnt;
    int file_count_;
    int file_update_count_;

    bool isupdate;
    bool isindex;
    bool update_system;

    QString lang;
    QString achain_config_path;
    QTimer *timerWaitUpgrade; 
    int getUpgradeCount;

private slots:
    void httpReadyRead();
    void httpFinished();
    void waitUpgrade();

private:
    void sendMessage();
    void updateSucc();

    QString getUpgrdeResult();

    void showTrayIcon(QString& hint, QString& text);
    void showTrayIconUpgrade();
    void showTrayIconDownload();

    //void updateDateReadProgress();
};

QString getFilePathMd5(QString filepath);
QString getFileMd5(QFile *file);
void getUpdateFile(QVector<fileInfo>& remote_list, QVector<fileInfo>& local_list,
    QVector<fileInfo>& update_file_list);
void getFileListInfo(const QByteArray& fileInfo, QVector<QUrl>& url_list);
void getFileName(QUrl url);