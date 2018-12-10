#include "AChainUpdater.h"
#include <QtWidgets/QApplication>
#include <QDebug>
#include "JlCompress.h"
#include "resource.h"


bool checkOnly()
{
    //  ����������
    HANDLE m_hMutex = CreateMutex(NULL, FALSE, L"AchainUp 1.0.4.0");
    //  ���������
    if (GetLastError() == ERROR_ALREADY_EXISTS)  {
        //  ������л������������ͷž������λ������
        CloseHandle(m_hMutex);
        m_hMutex = NULL;
        //  �����˳�
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

    if (checkOnly() == false)  return 0;    // ��ֹ����������

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