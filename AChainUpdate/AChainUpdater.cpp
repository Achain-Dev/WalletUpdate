
#include "AChainUpdater.h"
#include "JlCompress.h"
#include <qt_windows.h>
#include <QSystemTrayIcon>
#include <QSharedMemory>
#include <QTextStream>
#include <QBuffer>

#include "resource.h"
#include "macro.h"

AChainUpdater::AChainUpdater() :
isupdate(false), isindex(false), update_system(false), md5_check_error_cnt(0),
app_path(QCoreApplication::applicationDirPath()), _app_data_path(""), lang("cn"), timerWaitUpgrade(NULL)
{
}

AChainUpdater::~AChainUpdater()
{
    if (timerWaitUpgrade) {
        timerWaitUpgrade->stop();
        delete timerWaitUpgrade;
        timerWaitUpgrade = NULL;
    }
}

bool AChainUpdater::createUpdateFile()
{
    QString midDir = list_info_.version.isEmpty() ? "/" : "/" + list_info_.version + "/";
    QDir dir(_app_data_path + midDir);
    if (!dir.exists()){
        dir.mkpath(dir.absolutePath());
    }

    QString subPath = download_url_.path().right(download_url_.path().size() - base_url.path().size());
    qDebug() << "createUpdateFile() " << subPath;
    qDebug() << download_url_.path();
    qDebug() << base_url.path();
    QFileInfo fileInfo(download_url_.path().right(download_url_.path().size() - base_url.path().size()));
    if (!dir.mkpath(_app_data_path + midDir + fileInfo.path())) {
        return false;
    }

    QString fileName = _app_data_path + midDir + subPath;
    //qDebug() << fileName;
    if (QFile::exists(fileName)){
        QFile::remove(fileName);
    }

    download_file_ = new QFile(fileName);
    if (!download_file_->open(QIODevice::ReadWrite)){
        download_file_->close();
        delete download_file_;
        download_file_ = NULL;
        return false;
    }
    return true;
}

bool AChainUpdater::unzipUpdateFile()
{
    QString midDir = list_info_.version.isEmpty() ? "/" : "/" + list_info_.version + "/";

    int md5_zip_len = 1 + 32 + 1 + 3;
    QString zip_filename = download_url_.fileName();
    QString unzip_filename = zip_filename.left(zip_filename.length() - md5_zip_len);

    qDebug() << download_url_.path().right(download_url_.path().size() - base_url.path().size());
    QString file_path = _app_data_path + midDir + 
        download_url_.path().right(download_url_.path().size() - base_url.path().size());
    qDebug() << file_path;
    file_path = file_path.left(file_path.length() - zip_filename.length());
    qDebug() << file_path;

    //close download file of *.zip, then open unzip file
    download_file_->flush();
    download_file_->close();
    delete download_file_;
    download_file_ = NULL;

    QString result = JlCompress::extractFile(file_path + zip_filename, unzip_filename, file_path + unzip_filename);
    QFile::remove(file_path + zip_filename);

    //open unzip file
    if (QFile::exists(file_path + unzip_filename)) {
        download_file_ = new QFile(file_path + unzip_filename);
        if (!download_file_->open(/*QIODevice::ReadWrite*/QIODevice::ReadOnly)) {
            download_file_->close();
            delete download_file_;
            download_file_ = NULL;
            return false;
        }
        return true;
    }

    return false;
}

void AChainUpdater::getAppDataPath()
{
#ifdef WIN32
    _app_data_path = QDir::homePath() + "/AppData/Roaming/" + UPDATE_DIR;
#endif
}

void AChainUpdater::startUpdate()
{
    base_url = QUrl(UPDATE_BASE_URL);
    download_url_ = QUrl(QString(UPDATE_BASE_URL) + UPDATE_CONFIG_FILE);

    if (_app_data_path.size() <= 0) {
        getAppDataPath();
    }

    getVersion();

    if (!createUpdateFile()){
        emit sysFinished();
        return;
    }
    fileRequest(download_url_);
}

void AChainUpdater::fileRequest(const QUrl& url){
    QNetworkRequest request(url);
    if (url.url().startsWith("https")) {
        QSslConfiguration config;
        config.setPeerVerifyMode(QSslSocket::VerifyNone);
        config.setProtocol(QSsl::TlsV1_0);
        request.setSslConfiguration(config);
    }
    qDebug() << "fileRequest() " << url.path();
    net_reply_ = net_manager_.get(request);
    connect(net_reply_, &QNetworkReply::finished, this, &AChainUpdater::httpFinished);
    connect(net_reply_, &QNetworkReply::readyRead, this, &AChainUpdater::httpReadyRead);
}

void AChainUpdater::httpReadyRead()
{
    QByteArray data;
    if (download_file_){
        data = net_reply_->readAll();
        download_file_->write(data);
        //qDebug() << "read:" << download_file_->fileName() << data.size();
    }
}

void AChainUpdater::json2UpdateInfo(QJsonObject& obj, updateInfo& update_info)
{
    update_info.update_url = obj.value("update_url").toString();
    update_info.update_version = obj.value("update_version").toString();
    update_info.update_system_md5 = obj.value("update_system_md5").toString();
    update_info.update_system_name = obj.value("update_system_name").toString();
    update_info.update_index_file = obj.value("update_index_file").toString();
}

void AChainUpdater::json2FileListInfo(QJsonObject& obj, fileListInfo& list_info)
{
    list_info.update_url = obj.value("update_url").toString();
    list_info.version = obj.value("version").toString();

    QJsonArray arr = obj.value("file_list").toArray();
    int size = arr.size();
    fileInfo file_info;

    for (int i = 0; i < size; i++){
        QJsonValue value = arr.at(i);
        file_info.filename = value.toObject().value("file_name").toString();
        file_info.hash = value.toObject().value("hash").toString();
        file_info.version = value.toObject().value("version").toString();
        list_info.file_list.append(file_info);
    }
}

//resolve json
void AChainUpdater::jsonResolve()
{
    download_file_->seek(0);
    QByteArray data = download_file_->readAll();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    QJsonObject obj;
    if (error.error == QJsonParseError::NoError){
        obj = doc.object();
    }
    else {
        file_count_ = 0;
        emit sysFinished();
        return;
    }

    if (isupdate) {
        //update.json
        json2UpdateInfo(obj, update_info_);
    }
    else if (isindex) {
        //file_list.json
        json2FileListInfo(obj, list_info_);
        file_count_ = list_info_.file_list.size();
        file_update_count_ = 0;
        //qDebug() << file_count_ << file_update_count_ << list_info_.file_list.size();
    }
}

bool AChainUpdater::updateFileCheck()
{
    QString localMD5 = getFileMd5(download_file_);
    QString checkMD5;

    if (update_system){
        checkMD5 = update_info_.update_system_md5;
    }
    else if (!isupdate && !isindex) {
        checkMD5 = list_info_.file_list.at(file_update_count_ - 1).hash;
        qDebug() << "filename" << list_info_.file_list.at(file_update_count_ - 1).filename;
        qDebug() << "localmd5" << localMD5 << "checkMD5" << checkMD5;
    }
    else {
        return true;
    }

    if (localMD5.size() == 0 || checkMD5.size() == 0){
        qDebug() << "localmd5" << localMD5 << "checkMD5" << checkMD5;
        return false;
    }

    if (QString::compare(localMD5, checkMD5, Qt::CaseInsensitive) == 0){
        return true;
    }

    return false;
}

HANDLE AChainUpdater::ReleseResourceExe(WORD wResourceId, const TCHAR *pszFileName)
{
    HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(wResourceId), L"exe");

    if (NULL == hResInfo)
    {
        return false;
    }

    HGLOBAL hgRes = LoadResource(NULL, hResInfo);
    void *pvRes = LockResource(hgRes);
    DWORD cbRes = SizeofResource(NULL, hResInfo);

    DeleteFile(pszFileName);
    HANDLE hFile = CreateFile(pszFileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    DWORD cbWritten;
    WriteFile(hFile, pvRes, cbRes, &cbWritten, 0);
    CloseHandle(hFile);
    FreeResource(hgRes);

    return NULL;
}

bool AChainUpdater::copyFolderTo(QString oriPath, QString destPath, bool coverFileIfExist)
{
	QDir sourceDir(oriPath);
	QDir targetDir(destPath);

	if (!targetDir.exists()){    /**< 如果目标目录不存在，则进行创建 */
		if (!targetDir.mkdir(targetDir.absolutePath()))
			return false;
	}

	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	foreach(QFileInfo fileInfo, fileInfoList){

		if (fileInfo.fileName() == "." || fileInfo.fileName() == "..")
			continue;

		if (fileInfo.isDir()){    /**< 当为目录时，递归的进行copy */
			if (!copyFolderTo(fileInfo.filePath(), targetDir.filePath(fileInfo.fileName()), coverFileIfExist))
				return false;
		}
		else{            /**< 当允许覆盖操作时，将旧文件进行删除操作 */
			if (coverFileIfExist && targetDir.exists(fileInfo.fileName())){
				targetDir.remove(fileInfo.fileName());
			}

			/// 进行文件copy
			if (!QFile::copy(fileInfo.filePath(), targetDir.filePath(fileInfo.fileName()))){
				return false;
			}
		}
	}

	return true;
}

//void AChainUpdater::runToolAsAdmin(bool isUpdate)
//{
//#ifdef WIN32
//    QString path = _app_data_path + "/" + UPDATE_INSTALL_TOOL;
//    std::string install_tool(path.toStdString());
//    SHELLEXECUTEINFOA shExecInfo;
//    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
//    shExecInfo.fMask = NULL;
//    shExecInfo.hwnd = NULL;
//    shExecInfo.lpVerb = "runas";
//    shExecInfo.lpFile = install_tool.c_str();
//
//    //QString params = "\"" + app_path + " " + lang;
//    QString params = "\"" + app_path + "\"   \"" + achain_config_path + "\"";
//    //QString params = "\"" + app_path + "\"";
//    if (isUpdate) {
//        params = params + " update";
//    }
//    //shExecInfo.lpParameters = params.toUtf8().constData();
//    char szParams[1024];
//    strncpy(szParams, params.toUtf8().constData(), sizeof(szParams));
//    shExecInfo.lpParameters = szParams;
//
//    //writeLog("params " + params);
//    //writeLog("paramst " + QString::fromUtf8(shExecInfo.lpParameters));
//
//    shExecInfo.lpDirectory = NULL;
//    shExecInfo.nShow = SW_MAXIMIZE;
//    shExecInfo.hInstApp = NULL;
//    ShellExecuteExA(&shExecInfo);
//    //emit sysFinished();
//#endif
//}
void AChainUpdater::runToolAsAdmin(bool isUpdate)
{
    QString path = _app_data_path + "/" + UPDATE_INSTALL_TOOL;
    std::wstring str_app_data_path = path.toStdWString();

    std::wstring str_path_file = QCoreApplication::applicationDirPath().toStdWString();
    std::wstring config_path = achain_config_path.toStdWString();

    std::wstring params = L"\"" + str_path_file + L"\" \"" + config_path + L"\"";
    if (isUpdate) {
        params = params + L" update";
    }

    ShellExecuteW(NULL, L"runas", str_app_data_path.c_str(), params.c_str(), NULL, SW_SHOWNORMAL);
}


void AChainUpdater::httpFinished()
{
    isupdate = false;
    isindex = false;
    update_system = false;
    if (net_reply_->error()) {
        download_file_->close();
        emit sysFinished();
        return ;
    }
    else {
        qDebug() << "down finish" << download_file_->fileName();
        qDebug() << "down finish" << download_url_.fileName();
        QFileInfo fileInfo(download_file_->fileName());
        if (QString::compare(download_url_.fileName(), UPDATE_CONFIG_FILE) == 0) {
            isupdate = true;
            jsonResolve();
        }
        else if (download_url_.fileName().contains(UPDATE_TOOL_NAME)) {
            update_system = true;
            downloadUpdate();
            unzipUpdateFile();
        }
        else if (QString::compare(download_url_.fileName(), REMOTE_UPDATE_FILE) == 0) {
            isindex = true;
            jsonResolve();
        }
        else {
            file_update_count_++;
            unzipUpdateFile();
        }

        //check download file
        if (updateFileCheck() == false) {
            //re-download
            md5_check_error_cnt++;
            if (md5_check_error_cnt > MAX_MD5_ERROR_CNT) {
                download_file_->close();
                delete download_file_;
                download_file_ = NULL;
                net_reply_->deleteLater();
                net_reply_ = NULL;
                emit sysFinished();
                return;
            }
            //re-download
            if (!isindex && !isupdate) {
                file_update_count_--;
            }
        }

        if (NULL != download_file_)
        {
            download_file_->close();
            delete download_file_;
            download_file_ = NULL;
        }
        net_reply_->deleteLater();
        net_reply_ = NULL;

        if (update_system) { // after download AchainUpdate.exe, start download file_index.json
            QString releasePath = _app_data_path + "/" + UPDATE_INSTALL_TOOL;
            ReleseResourceExe(IDR_TOOL_EXE, releasePath.toStdWString().c_str());

			QString dllPath = _app_data_path + "/" + WINDOWS_DLL_PATH;
			copyFolderTo(WINDOWS_DLL_PATH, dllPath);
            
			runToolAsAdmin(true);
            emit sysFinished();
            return;
        }
        else if (isupdate) {
            QString version = getFileAttrVersion(app_path + "/" + UPDATE_TOOL_NAME);
            if (!version.isEmpty()) {
                if (0 > version.compare(update_info_.update_version, Qt::CaseInsensitive)) {
                    download_url_ = QString(update_info_.update_url + update_info_.update_system_name +
                        "." + update_info_.update_system_md5 + ".zip");
                }
                else {
                    download_url_ = QString(update_info_.update_url + update_info_.update_index_file);
                }
            }
            else {
                QString update_tool_md5(getFilePathMd5(app_path + "/" + UPDATE_TOOL_NAME));
                if (update_tool_md5.size() == 0 || update_tool_md5 != update_info_.update_system_md5) {
                    download_url_ = QString(update_info_.update_url + update_info_.update_system_name +
                        "." + update_info_.update_system_md5 + ".zip");
                }
                else {
                    download_url_ = QString(update_info_.update_url + update_info_.update_index_file);
                }
            }

            if (!createUpdateFile()) { // AChainUpdate.exe or file_index.json
                emit sysFinished();
                return;
            }
            fileRequest(download_url_);
        }
        else if (isindex) { // start download first file
            if (file_count_ == 0) {
                emit sysFinished();
                return;
            }
            if (list_info_.version.toULong() <= current_version.toULong()) {
                qDebug() << "version is latest";
                emit sysFinished();
                return;
            }

            findUpdateIndex();
            if (file_update_count_ < file_count_) {
                base_url = QUrl(list_info_.update_url);
                //file_list.json
                download_url_ = QString(list_info_.update_url) + list_info_.file_list.at(file_update_count_).filename +
                    "." + list_info_.file_list.at(file_update_count_).hash + ".zip";
                if (!createUpdateFile()){
                    emit sysFinished();
                    return;
                }
                fileRequest(download_url_);
            }
            else {
                qDebug() << "all files is update";
                saveVersion();
                emit sysFinished();
                return;
            }
        } // isindex
        else { //other file
            //qDebug() << file_update_count_ <<" "<< update_list_info_.size();
            if (file_update_count_ == list_info_.file_list.size() || file_count_ == 0) {
                updateSucc();
                emit sysFinished();
                return;
            }

            findUpdateIndex();
            qDebug() << file_update_count_ << "/" << file_count_;
            if (file_update_count_ < file_count_)
            {
                download_url_ = QString(list_info_.update_url) + list_info_.file_list.at(file_update_count_).filename +
                    "." + list_info_.file_list.at(file_update_count_).hash + ".zip";
                if (!createUpdateFile()) {
                    emit sysFinished();
                    return;
                }
                fileRequest(download_url_);
            }
            else {
                qDebug() << "files is update over";
                updateSucc();
                emit sysFinished();
                return;
            }
        } // else 
    } // net_reply_ ok 
}

const ULONG_PTR CUSTOM_TYPE = 10000;
const QString c_strTitle = "Achain";
void AChainUpdater::sendMessage()
{
    if (update_list_info_.size() <= 0) {
        return;
    }

    HWND hwnd = NULL;
    LPWSTR path = (LPWSTR)c_strTitle.utf16();
    hwnd = ::FindWindowW(NULL, path);

    if (::IsWindow(hwnd))
    {
        QString filename = QStringLiteral("update");
        QByteArray data = filename.toUtf8();

        COPYDATASTRUCT copydata;
        copydata.dwData = CUSTOM_TYPE;  // 用户定义数据
        copydata.lpData = data.data();  //数据大小
        copydata.cbData = data.size();  // 指向数据的指针

        HWND sender = (HWND)0/*QWidget::*//*effectiveWinId()*/;

        ::SendMessage(hwnd, WM_COPYDATA, reinterpret_cast<WPARAM>(sender), reinterpret_cast<LPARAM>(&copydata));
    }
}

void AChainUpdater::showTrayIcon(QString& hint, QString& text)
{
    QSystemTrayIcon *myTrayIcon = new QSystemTrayIcon(this);
    myTrayIcon->setIcon(QIcon(app_path + "/" + "pic2/achain.ico"));
    myTrayIcon->setVisible(true);

    myTrayIcon->showMessage(hint, text);
    Sleep(7000);
}

void AChainUpdater::showTrayIconDownload()
{
    QSettings settings(achain_config_path, QSettings::IniFormat);
    lang = settings.value("/settings/language").toString();
    //writeLog("showTrayIcon() lang " + lang);
    //writeLog("showTrayIcon() achain_config_path " + achain_config_path);

    QString hint = QStringLiteral("提示");
    QString text = QStringLiteral("发现新版本，关闭后将自动更新");
    if (lang.isEmpty() || 0 == QString::compare(lang, "English", Qt::CaseInsensitive)) {
        hint = QStringLiteral("Hint");
        text = QStringLiteral("new version, auto update after close");
    }
    showTrayIcon(hint, text);
}
void AChainUpdater::showTrayIconUpgrade()
{
    QSettings settings(achain_config_path, QSettings::IniFormat);
    lang = settings.value("/settings/language").toString();
    //writeLog("showTrayIcon() lang " + lang);
    //writeLog("showTrayIcon() achain_config_path " + achain_config_path);

    QString hint = QStringLiteral("提示");
    QString text = QStringLiteral("钱包升级成功，请稍候，正在启动...");
    if (lang.isEmpty() || 0 == QString::compare(lang, "English", Qt::CaseInsensitive)) {
        hint = QStringLiteral("Hint");
        text = QStringLiteral("wallet update success, please wait, starting ...");
    }
    showTrayIcon(hint, text);
}

void AChainUpdater::updateSucc()
{
    saveLocalUpdateFileInfo();

    saveVersion();
    downloadFile();

    //sendMessage();
    showTrayIconDownload();
}

void AChainUpdater::findUpdateIndex()
{
    while (file_update_count_ < file_count_) {
        QString filename = list_info_.file_list.at(file_update_count_).filename;
        QString version = getFileAttrVersion(app_path + "/" + filename);
        if (!version.isEmpty()) {
            if (0 > version.compare(list_info_.file_list.at(file_update_count_).version, Qt::CaseInsensitive)) {
                update_list_info_.append(list_info_.file_list.at(file_update_count_));
                break;
            }
            else {
                file_update_count_++;
            }
        }
        else {
            QString file_md5(getFilePathMd5(app_path + "/" + filename));
            if (file_md5.size() == 0 || file_md5 != list_info_.file_list.at(file_update_count_).hash) {
                update_list_info_.append(list_info_.file_list.at(file_update_count_));
                break;
            }
            else {
                file_update_count_++;
            }
        }
    }
}

#pragma comment(lib, "Version")
QString AChainUpdater::getFileAttrVersion(QString fullName)
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

// not use
void AChainUpdater::getCurrentFileMD5Info(QString path, QVector<fileInfo>& file_list)
{
    QDir dir(path);
    foreach(QFileInfo mfi, dir.entryInfoList()){
        if (mfi.path() == (QCoreApplication::applicationDirPath() + "/" + "update")) {
            continue;
        }
        if (mfi.isFile()) {
            //qDebug() << "File:" << mfi.filePath();
            QString version = getFileAttrVersion(mfi.absoluteFilePath());

            QFile file(mfi.absoluteFilePath());
            QString md5;
            if (file.open(QIODevice::ReadOnly)){
                md5 = getFileMd5(&file);
            }
            else {
                qDebug() << "File MD5:" << file.errorString();
                //TODO handle error
            }
            file.close();

            QString path = QCoreApplication::applicationDirPath();
            QString filePath;
            if (mfi.filePath().startsWith(path)){
                filePath = mfi.filePath().mid(path.size() + 1);
            }
			if (mfi.fileName() == QString(GEN_TOOL_NAME)){
                continue;
            }
            qDebug() << "local file" << filePath;
            file_list.append(fileInfo(filePath, version, md5));
        }
        else {
            if (mfi.fileName() == "." || mfi.fileName() == "..")
                continue;
            getCurrentFileMD5Info(mfi.absoluteFilePath(), file_list);
        }
    }
}

void AChainUpdater::saveLocalUpdateFileInfo()
{
    QJsonObject updatejson;
    fileInfo2Json(update_list_info_, updatejson);

    QJsonDocument doc(updatejson);
    QFile file(_app_data_path + "/" + UPDATE_LIST_FILE);
    if (file.exists()) {
        file.remove();
    }

    file.open(QIODevice::ReadWrite);
    file.write(doc.toJson());
    file.close();
}

QString getFilePathMd5(QString filepath)
{
    QFile file(filepath);
    QString ret;
    if (file.exists()) {
        file.open(QIODevice::ReadOnly);
        ret = getFileMd5(&file);
        file.close();
        return ret;
    }
    else {
        return QString("");
    }
}

QString getFileMd5(QFile *file)
{
    if (file == nullptr){
        return nullptr;
    }
    QCryptographicHash md(QCryptographicHash::Md5);
    quint64 totalBytes = 0;
    quint64 bytesWritten = 0;
    quint64 bytesToWrite = 0;
    quint64 loadSize = 1024 * 4;
    QByteArray buf;
    totalBytes = file->size();
    bytesToWrite = totalBytes;

    file->seek(0);

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

// not use
void getUpdateFile(QVector<fileInfo>& remote_list, QVector<fileInfo>& local_list, 
    QVector<fileInfo>& update_file_list)
{
    QStringList remote_file;
    update_file_list.clear();
    if (remote_list.size() == 0){
        return;
    }
    for each (fileInfo remote_info in remote_list) {
        bool need_update = true;
        for each (fileInfo local_info in local_list) {
            if (local_info.filename == remote_info.filename
                && local_info.hash == remote_info.hash) {
                need_update = false;
                qDebug() << "need_update false:" << local_info.filename << local_info.hash;
                qDebug() << remote_info.filename << remote_info.hash;
            }
        }
        if (need_update) {
            update_file_list.append(remote_info);
        }
    }
}

void AChainUpdater::fileInfo2Json(QVector<fileInfo> &file_list, QJsonObject& updatejson)
{
    QDateTime current_date_time = QDateTime::currentDateTime();
    QString current_date = current_date_time.toString("yyyyMMddhh");
    QJsonArray filelist_json;
    for each (fileInfo fileinfo in file_list)
    {
        QJsonObject obj;
        obj.insert("file_name", fileinfo.filename);
        obj.insert("version", fileinfo.version);
        obj.insert("hash", fileinfo.hash);
        filelist_json.append(obj);
    }
    updatejson.insert("file_list", filelist_json);
    updatejson.insert("version", list_info_.version);
}

void AChainUpdater::getVersion()
{
    QSettings config(app_path + "/" + VERSION_CONFIG, QSettings::IniFormat);
    current_version = config.value("version", "0").toString();
}

void AChainUpdater::saveVersion()
{
    QSettings config(app_path + "/" + VERSION_CONFIG, QSettings::IniFormat);
    config.setValue("version", list_info_.version);
}

void AChainUpdater::downloadNoFile()
{
    QSettings config(app_path + "/" + VERSION_CONFIG, QSettings::IniFormat);
    config.setValue(UPDATE_SYS_MODE_DOWNLOAD, false);
}

void AChainUpdater::downloadFile()
{
    QSettings config(app_path + "/" + VERSION_CONFIG, QSettings::IniFormat);
    config.setValue(UPDATE_SYS_MODE_DOWNLOAD, true);
}

void AChainUpdater::downloadUpdate()
{
    QSettings config(app_path + "/" + VERSION_CONFIG, QSettings::IniFormat);
    config.setValue(UPDATE_SYS_MODE_UPDATE, true);
}

void AChainUpdater::downloadNoUpdate()
{
    QSettings config(app_path + "/" + VERSION_CONFIG, QSettings::IniFormat);
    config.setValue(UPDATE_SYS_MODE_UPDATE, false);
}

void AChainUpdater::updateUpdateSys()
{
    base_url = QUrl(UPDATE_BASE_URL);
    download_url_ = QUrl(QString(UPDATE_BASE_URL) + REMOTE_UPDATE_FILE);

    if (_app_data_path.size() <= 0) {
        getAppDataPath();
    }

    getVersion();

    if (!createUpdateFile()){
        emit sysFinished();
        return;
    }
    fileRequest(download_url_);
}

QString AChainUpdater::getUpgrdeResult()
{
    QSharedMemory sharemem;
    QString key = "AchainUpShareKey";

    QBuffer buffer;
    QDataStream in(&buffer);

    QString result;

    sharemem.setKey(key);
    if (!sharemem.attach())
    {
        //writeLog("attach() failed");
        return "";
    }

    sharemem.lock();
    buffer.setData((char *)sharemem.constData(), sharemem.size());
    buffer.open(QBuffer::ReadOnly);
    in >> result;
    sharemem.unlock();

    if (result.isEmpty()) {
        getUpgradeCount++;
        //writeLog("sharemem result empty");
    }
    else {
        //writeLog("sharemem result " + result);
    }
    sharemem.detach();

    return result;
}
void AChainUpdater::waitUpgrade()
{
    QString resultStr = getUpgrdeResult();
    if (resultStr == "succ")
    {
        showTrayIconUpgrade();
        emit sysFinished();
    }
    else if (!resultStr.isEmpty() || getUpgradeCount > 120) // 120*2=240 秒 4分钟，还没有读取到结果，就认为失败
    {
        emit sysFinished();
    }
}

void AChainUpdater::updateRunTool()
{
    if (_app_data_path.size() <= 0) {
        getAppDataPath();
    }

    QString releasePath = _app_data_path + "/" + UPDATE_INSTALL_TOOL;
	ReleseResourceExe(IDR_TOOL_EXE, releasePath.toStdWString().c_str());
	
	QString dllPath = _app_data_path + "/" + WINDOWS_DLL_PATH;
	copyFolderTo(WINDOWS_DLL_PATH, dllPath);

    runToolAsAdmin();

    timerWaitUpgrade = new QTimer(this);
    connect(timerWaitUpgrade, SIGNAL(timeout()), this, SLOT(waitUpgrade()));
    timerWaitUpgrade->start(2 * 1000);
}

void AChainUpdater::writeLog(QString log)
{
    QFile file("./update.log");
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out << log << endl;
    out.flush();
    file.close();
}

void AChainUpdater::setLang(QString lang)
{
    this->lang = lang;
}

void AChainUpdater::setConfigPath(QString configPath)
{
    this->achain_config_path = configPath;
}
