#ifndef __MACRO_H__
#define __MACRO_H__

#define GEN_TOOL_NAME "AChainUpdateInfoGenTool.exe"
#define UPDATE_NAME "AchainUp.exe"

#if TRUE

#define UPDATE_DIR "AchainLiteUpdate"
#define UPDATE_URL "http://achain-wallet.oss-cn-hongkong.aliyuncs.com/win/update_lite/"
#define UPDATE_BASE_URL "http://achain-wallet.oss-cn-hongkong.aliyuncs.com/win/update_lite/"

#else

#define UPDATE_DIR "AchainUpdate"
#define UPDATE_URL "http://achain-wallet.oss-cn-hongkong.aliyuncs.com/win/update/"
#define UPDATE_BASE_URL "http://achain-wallet.oss-cn-hongkong.aliyuncs.com/win/update/"

#endif

#endif // __MACRO_H__
