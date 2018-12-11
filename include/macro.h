#pragma once

#define GEN_TOOL_NAME "AChainUpdateInfoGenTool.exe"
#define UPDATE_NAME "AchainUp.exe"
#define UPDATE_TOOL_NAME "AchainUp.exe"
#define UPDATE_INSTALL_TOOL "AchainUpTool.exe"

#define UPDATE_SYS_MODE_DOWNLOAD "download"
#define UPDATE_SYS_MODE_UPDATE "update"
#define UPDATE_SYS_MODE_INSTALL "install"

#define VERSION_CONFIG "version.ini"
#define UPDATE_LIST_FILE "update_file_index.json"
#define UPDATE_CONFIG_FILE "config.json"
#define REMOTE_UPDATE_FILE "file_index.json"
#define WINDOWS_DLL_PATH "platforms"

#define MAX_MD5_ERROR_CNT (5)

#if TRUE

#define SYS_NAME "AchainWalletLite.exe"
#define TEMP_DIR "AchainLiteTemp"
#define UPDATE_DIR "AchainLiteUpdate"
#define UPDATE_URL ""
#define UPDATE_BASE_URL ""

#else

#define SYS_NAME "Achain.exe"
#define TEMP_DIR "AchainTemp"
#define UPDATE_DIR "AchainUpdate"
#define UPDATE_URL ""
#define UPDATE_BASE_URL ""

#endif
