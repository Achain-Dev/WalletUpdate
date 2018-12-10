#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QtCore/QCoreApplication>

#include "JlCompress.h"
#include "qt_windows.h"

#include "macro.h"

QStringList ignore_file = { UPDATE_NAME };
QString dir_path = "Achain";

struct fileInfo
{
    fileInfo(QString filename, QString version, QString hash) :
        filename(filename), version(version), hash(hash){}
    fileInfo(){}
    QString filename;
    QString version;
    QString hash;
};

QString getFileMd5(QFile *file)
{
    if (!file){
        return nullptr;
    }

    file->seek(0);
    QCryptographicHash md(QCryptographicHash::Md5);
    quint64 totalBytes = 0;
    quint64 bytesWritten = 0;
    quint64 bytesToWrite = 0;
    quint64 loadSize = 1024 * 4;
    QByteArray buf;
    totalBytes = file->size();
    bytesToWrite = totalBytes;

    while (1){
        if (bytesToWrite > 0) {
            buf = file->read(qMin(bytesToWrite, loadSize));
            if (buf.size() == 0){
                break;
            }
            md.addData(buf);
            bytesWritten += buf.length();
            bytesToWrite -= buf.length();
            buf.resize(0);
        }
        else {
            break;
        }

        if (bytesWritten == totalBytes) {
            break;
        }
    }
    QString md5(md.result().toHex());
    return md5;
}

#pragma comment(lib, "Version")
QString getFileAttrVersion(QString fullName)
{
    QString result = "";
    char* pData = nullptr;
    do
    {
        DWORD dwLen = GetFileVersionInfoSize(fullName.toStdWString().c_str(), 0);
        if (0 == dwLen)
        {
            break;
        }
        pData = new char[dwLen + 1];
        BOOL bSuccess = GetFileVersionInfo(fullName.toStdWString().c_str(), 0, dwLen, pData);
        if (!bSuccess)
        {
            break;
        }
        struct LANGANDCODEPAGE
        {
            WORD wLanguage;
            WORD wCodePage;
        } *lpTranslate;
        LPVOID lpBuffer = nullptr;
        UINT uLen = 0;
        bSuccess = VerQueryValue(pData, (TEXT("\\VarFileInfo\\Translation")), (LPVOID*)&lpTranslate, &uLen);
        if (!bSuccess)
        {
            break;
        }
        QString str1, str2;
        str1.setNum(lpTranslate->wLanguage, 16);
        str2.setNum(lpTranslate->wCodePage, 16);
        str1 = "000" + str1;
        str2 = "000" + str2;
        QString verPath = "\\StringFileInfo\\" + str1.right(4) + str2.right(4) + "\\FileVersion";
        bSuccess = VerQueryValue(pData, (verPath.toStdWString().c_str()), &lpBuffer, &uLen);
        if (!bSuccess)
        {
            break;
        }
        result = QString::fromUtf16((const unsigned short int *)lpBuffer);
    } while (false);
    if (nullptr != pData)
    {
        delete pData;
    }
    return result;
}

void getFileInfo(QString path, QVector<fileInfo>& file_list)
{
    QDir dir(path);
    foreach(QFileInfo mfi, dir.entryInfoList()){

        qDebug() << "file" << mfi.absoluteFilePath();
        if (mfi.isFile()) {
            QString version = getFileAttrVersion(mfi.absoluteFilePath());

            QFile file(mfi.absoluteFilePath());
            file.open(QIODevice::ReadOnly);
            QString md5 = getFileMd5(&file);
            file.close();

            QString path;
            if (dir_path.size() > 0) {
                path = QCoreApplication::applicationDirPath() + "/" + dir_path;
            }
            else {
                path = QCoreApplication::applicationDirPath();
            }

            QString filePath;
            if (mfi.filePath().startsWith(path)){
                filePath = mfi.filePath().mid(path.size() + 1);
            }
            if (filePath.isEmpty()) {
                continue;
            }
            if (ignore_file.contains(mfi.fileName())){
                continue;
            }
            file_list.append(fileInfo(filePath, version, md5));

            QString pathDir = filePath.left(filePath.length() - mfi.fileName().length());
            QDir dirZip(QCoreApplication::applicationDirPath() + "/update/" + dir_path + "/" + pathDir);
            if (!dirZip.exists()) {
                dirZip.makeAbsolute();
            }
            JlCompress::compressFile(
                QCoreApplication::applicationDirPath() + "/update/" + dir_path + "/" + filePath + "." + md5 + ".zip",
                mfi.absoluteFilePath());
        }
        else {
            if (mfi.fileName() == "." || mfi.fileName() == "..")
                continue;
            getFileInfo(mfi.absoluteFilePath(), file_list);
        }
    }
}

void fileInfo2Json(QVector<fileInfo> &file_list, QJsonObject& updatejson)
{
    QDateTime current_date_time = QDateTime::currentDateTime();
    QString current_date = current_date_time.toString("yyyyMMddhh");
    QJsonArray filelist_json;
    for each (fileInfo fileinfo in file_list){
        QJsonObject obj;
        obj.insert("file_name", fileinfo.filename);
        obj.insert("version", fileinfo.version);
        obj.insert("hash", fileinfo.hash);
        filelist_json.append(obj);
    }
    updatejson.insert("file_list", filelist_json);
    updatejson.insert("update_url", QString(UPDATE_URL) + dir_path + "/");
    updatejson.insert("version", current_date);

}

bool DelDir(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    QDir dir(path);
    if (!dir.exists()) {
        return true;
    }
    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot); //设置过滤
    QFileInfoList fileList = dir.entryInfoList(); // 获取所有的文件信息
    foreach(QFileInfo file, fileList){ //遍历文件信息
        if (file.isFile()) { // 是文件，删除
            file.dir().remove(file.fileName());
        }
        else { // 递归删除
            DelDir(file.absoluteFilePath());
        }
    }
    return dir.rmpath(dir.absolutePath()); // 删除文件夹
}

void createFileIndex(QJsonObject& updatejson)
{
    QJsonDocument doc(updatejson);
    QFile file(QCoreApplication::applicationDirPath() + "/update/file_index.json");
    if (file.exists()) {
        file.remove();
    }
    file.open(QIODevice::ReadWrite);
    file.write(doc.toJson());
    file.close();
}

void createUpdateAndConfig(QString updatePath)
{
    QFile file(updatePath);
    if (!file.exists())
    {
        qDebug() << "update.exe path error";
        return;
    }

    file.open(QIODevice::ReadOnly);
    QString md5 = getFileMd5(&file);
    file.close();

    JlCompress::compressFile(
        QCoreApplication::applicationDirPath() + "/update/" + UPDATE_NAME + "." + md5 + ".zip",
        updatePath);

    QString version = getFileAttrVersion(updatePath);

    QJsonObject updatejson;
    updatejson.insert("update_version", version);
    updatejson.insert("update_url", UPDATE_URL);
    updatejson.insert("update_system_name", UPDATE_NAME);
    updatejson.insert("update_system_md5", md5);
    updatejson.insert("update_index_file", "file_index.json");

    QJsonDocument doc(updatejson);
    QFile jsonFile(QCoreApplication::applicationDirPath() + "/update/config.json");
    if (jsonFile.exists()) {
        jsonFile.remove();
    }
    jsonFile.open(QIODevice::ReadWrite);
    jsonFile.write(doc.toJson());
    jsonFile.close();
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    if (argc > 1) {
        dir_path = QString(argv[1]);
        qDebug() << argv[1];
    }

    QJsonObject updatejson;
    QVector<fileInfo> file_list;
    qDebug() << QDir::currentPath();

    DelDir(QCoreApplication::applicationDirPath() + "/update/");

    getFileInfo(QDir::currentPath()+"/" + dir_path, file_list);

    fileInfo2Json(file_list, updatejson);
    createFileIndex(updatejson);

    createUpdateAndConfig(QCoreApplication::applicationDirPath() + "/" + UPDATE_NAME);

    return 0;
}

