#include "AChainUpdater.h"
#include <QtWidgets/QApplication>
#include <QDebug>
#include "JlCompress.h"
#include "resource.h"


bool checkOnly()
{
    //  创建互斥量
    HANDLE m_hMutex = CreateMutex(NULL, FALSE, L"AchainUp 1.0.4.0");
    //  检查错误代码
    if (GetLastError() == ERROR_ALREADY_EXISTS)  {
        //  如果已有互斥量存在则释放句柄并复位互斥量
        CloseHandle(m_hMutex);
        m_hMutex = NULL;
        //  程序退出
        return  false;
    }
    else {
        return true;
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    if (checkOnly() == false)  return 0;    // 防止程序多次启动

    AChainUpdater updater;
    a.connect(&updater, SIGNAL(sysFinished()), &a, SLOT(quit()));

    if (argc > 2) {
        QString configPath = QString::fromUtf8(argv[2]);
        updater.setConfigPath(configPath);
        //updater.writeLog("main() configPath " + configPath);
    }

    //updater.writeLog("new start");
    if (argc > 1) {
        QString mode = QString::fromUtf8(argv[1]);
        //updater.getAppDataPath();
        //updater.runToolAsAdmin();
        //return 0;
        //updater.writeLog("argv[1] " + mode);
        if (mode == UPDATE_SYS_MODE_DOWNLOAD) {
            //updater.writeLog("download");
            updater.startUpdate();
        }
        else if (mode == UPDATE_SYS_MODE_INSTALL) {
            //updater.writeLog("install");
            updater.updateRunTool();
        }
        else if (mode == UPDATE_SYS_MODE_UPDATE) {
            //updater.writeLog("update 1");
            updater.updateUpdateSys();
        }
    }
    else {
        //updater.writeLog("update 2");
        updater.startUpdate();
    }
    return a.exec();
}