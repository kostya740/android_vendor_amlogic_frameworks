/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  @author   Tellen Yu
 *  @version  1.0
 *  @date     2014/10/22
 *  @par function description:
 *  - 1 control system sysfs proc env & property
 */

#define LOG_TAG "SystemControl"
//#define LOG_NDEBUG 0

#include <fcntl.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/PermissionCache.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <private/android_filesystem_config.h>
#include <pthread.h>

#include "SystemControl.h"
#include "ubootenv.h"

namespace android {

void SystemControl::instantiate(const char *cfgpath) {
    android::status_t ret = defaultServiceManager()->addService(
            String16("system_control"), new SystemControl(cfgpath));

    if (ret != android::OK) {
        ALOGE("Couldn't register system control service!");
    }
    ALOGI("instantiate add system_control service result:%d", ret);
}

SystemControl::SystemControl(const char *path)
    : mLogLevel(LOG_LEVEL_DEFAULT) {

    bootenv_init();

    pSysWrite = new SysWrite();

    pDisplayMode = new DisplayMode(path);
    pDisplayMode->init();
}

SystemControl::~SystemControl() {
    delete pSysWrite;
    delete pDisplayMode;
}

int SystemControl::permissionCheck() {
    // codes that require permission check
    IPCThreadState* ipc = IPCThreadState::self();
    const int pid = ipc->getCallingPid();
    const int uid = ipc->getCallingUid();

    if ((uid != AID_GRAPHICS) && (uid != AID_MEDIA) &&
            !PermissionCache::checkPermission(String16("droidlogic.permission.SYSTEM_CONTROL"), pid, uid)) {
        ALOGE("Permission Denial: "
                "can't use system control service pid=%d, uid=%d", pid, uid);
        return PERMISSION_DENIED;
    }

    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("system_control service permissionCheck pid=%d, uid=%d", pid, uid);
    }

    return NO_ERROR;
}

//read write property and sysfs
bool SystemControl::getProperty(const String16& key, String16& value) {
    char buf[PROPERTY_VALUE_MAX] = {0};
    bool ret = pSysWrite->getProperty(String8(key).string(), buf);
    value.setTo(String16(buf));

    return ret;
}

bool SystemControl::getPropertyString(const String16& key, String16& value, String16& def) {
    char buf[PROPERTY_VALUE_MAX] = {0};
    bool ret = pSysWrite->getPropertyString(String8(key).string(), (char *)buf, String8(def).string());
    value.setTo(String16(buf));
    return ret;
}

int32_t SystemControl::getPropertyInt(const String16& key, int32_t def) {
    return pSysWrite->getPropertyInt(String8(key).string(), def);
}

int64_t SystemControl::getPropertyLong(const String16& key, int64_t def) {
    return pSysWrite->getPropertyLong(String8(key).string(), def);
}

bool SystemControl::getPropertyBoolean(const String16& key, bool def) {
    return pSysWrite->getPropertyBoolean(String8(key).string(), def);
}

void SystemControl::setProperty(const String16& key, const String16& value) {
    if (NO_ERROR == permissionCheck()) {
        pSysWrite->setProperty(String8(key).string(), String8(value).string());
        traceValue(String16("setProperty"), key, value);
    }
}

bool SystemControl::readSysfs(const String16& path, String16& value) {
    if (NO_ERROR == permissionCheck()) {
        traceValue(String16("readSysfs"), path, value);

        char buf[MAX_STR_LEN] = {0};
        bool ret = pSysWrite->readSysfs(String8(path).string(), buf);
        value.setTo(String16(buf));
        return ret;
    }

    return false;
}

bool SystemControl::writeSysfs(const String16& path, const String16& value) {
    if (NO_ERROR == permissionCheck()) {
        traceValue(String16("writeSysfs"), path, value);

        return pSysWrite->writeSysfs(String8(path).string(), String8(value).string());
    }

    return false;
}

//set or get uboot env
bool SystemControl::getBootEnv(const String16& key, String16& value) {
    const char* p_value = bootenv_get(String8(key).string());
	if (p_value) {
        value.setTo(String16(p_value));
        return true;
	}
    return false;
}

void SystemControl::setBootEnv(const String16& key, const String16& value) {
    if (NO_ERROR == permissionCheck()) {
        bootenv_update(String8(key).string(), String8(value).string());
        traceValue(String16("setBootEnv"), key, value);
    }
}

void SystemControl::getDroidDisplayInfo(int &type, String16& socType, String16& defaultUI,
        int &fb0w, int &fb0h, int &fb0bits, int &fb0trip,
        int &fb1w, int &fb1h, int &fb1bits, int &fb1trip) {
    if (NO_ERROR == permissionCheck()) {
        char bufType[MAX_STR_LEN] = {0};
        char bufUI[MAX_STR_LEN] = {0};
        pDisplayMode->getDisplayInfo(type, bufType, bufUI);
        socType.setTo(String16(bufType));
        defaultUI.setTo(String16(bufUI));
        pDisplayMode->getFbInfo(fb0w, fb0h, fb0bits, fb0trip, fb1w, fb1h, fb1bits, fb1trip);
    }
}

void SystemControl::loopMountUnmount(int &isMount, String16& path) {
    if (NO_ERROR == permissionCheck()) {
        traceValue(String16("loopMountUnmount"),
            (isMount==1)?String16("mount"):String16("unmount"), path);

        if (isMount == 1) {
            char mountPath[MAX_STR_LEN] = {0};
            const char *pathStr = String8(path).string();
            strncpy(mountPath, pathStr, strlen(pathStr));

            const char *cmd[4] = {"vdc", "loop", "mount", mountPath};
            vdc_loop(4, (char **)cmd);
        } else {
            const char *cmd[3] = {"vdc", "loop", "unmount"};
            vdc_loop(3, (char **)cmd);
        }
    }
}

void SystemControl::setMboxOutputMode(const String16& mode) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("set output mode :%s", String8(mode).string());
    }

    pDisplayMode->setMboxOutputMode(String8(mode).string());
}

int32_t SystemControl::set3DMode(const String16& mode3d) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("set 3d mode :%s", String8(mode3d).string());
    }

    return pDisplayMode->set3DMode(String8(mode3d).string());
}

void SystemControl::init3DSetting(void) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("init3DSetting\n");
    }

    setDisplay3DFormat(FORMAT_3D_OFF);//for osd
    setDisplay3DTo2DFormat(FORMAT_3D_OFF);//for video
}

int32_t SystemControl::getVideo3DFormat(void) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("getVideo3DFormat\n");
    }

    int32_t format = -1;
    int32_t vpp3Dformat = -1;
    int video_fd = open(VIDEO_PATH, O_RDWR);
    if (video_fd < 0) {
        return -1;
    }
    ioctl(video_fd, AMSTREAM_IOC_GET_SOURCE_VIDEO_3D_TYPE, &vpp3Dformat);
    format = get3DFormatByVpp(vpp3Dformat);
    close(video_fd);

    return format;
}

int32_t SystemControl::getDisplay3DTo2DFormat(void) {
    int32_t format = -1;
    unsigned int operation = 0;
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("getDisplay3DTo2DFormat\n");
    }

    int video_fd = open(VIDEO_PATH, O_RDWR);
    if (video_fd < 0) {
        return -1;
    }
    ioctl(video_fd, AMSTREAM_IOC_GET_3D_TYPE, &operation);
    close(video_fd);
    format = get3DFormatByOperation(operation);

    return format;
}

bool SystemControl::setDisplay3DTo2DFormat(int format) {
    bool ret = false;
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("setDisplay3DTo2DFormat format:%d\n", format);
    }

    setDiBypassAll(format);
    int video_fd = open(VIDEO_PATH, O_RDWR);
    if (video_fd < 0) {
        ALOGE("setDisplay3DTo2DFormat video_fd:%d\n", video_fd);
        return false;
    }
    unsigned int operation = get3DOperationByFormat(format);
    ALOGI("setDisplay3DTo2DFormat operation:%d\n", operation);
    if (ioctl(video_fd, AMSTREAM_IOC_SET_3D_TYPE, operation) == 0) {
        ret = true;
    }
    close(video_fd);

    return ret;
}

bool SystemControl::setDisplay3DFormat(int format) {
    bool ret = false;
    String16 di3DformatStr;
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("setDisplay3DFormat format:%d\n", format);
    }

    if (format == FORMAT_3D_AUTO) {
        format = getVideo3DFormat();
    }

    mDisplay3DFormat = format;
    char format3DStr[64] = {0};
    get3DFormatStr(format, format3DStr);
    if (pDisplayMode->set3DMode(format3DStr) == 0) {
        ret = true;
    }

    return ret;
}

int32_t SystemControl::getDisplay3DFormat(void) {
    return mDisplay3DFormat;
}

bool SystemControl::setOsd3DFormat(int format) {
    bool ret = false;
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("setOsd3DFormat format:%d\n", format);
    }

    // TODO: needn't implement right now

    return ret;
}

bool SystemControl::switch3DTo2D(int format) {
    bool ret = false;
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("switch3DTo2D format:%d\n", format);
    }

    setDiBypassAll(format);
    int video_fd = open(VIDEO_PATH, O_RDWR);
    if (video_fd < 0) {
        return false;
    }
    unsigned int operation = get3DOperationByFormat(format);
    if (ioctl(video_fd, AMSTREAM_IOC_SET_3D_TYPE, operation) == 0) {
        ret = true;
    }
    close(video_fd);

    return ret;
}

bool SystemControl::switch2DTo3D(int format) {
    bool ret = false;
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("switch2DTo3D format:%d\n", format);
    }

    // TODO: implement later

    return ret;
}

void* SystemControl::detect3DThread(void* data) {
    SystemControl *pThiz = (SystemControl*)data;
    int retry = RETRY_MAX;
    int di3Dformat[5] = {0};
    int times[5] = {0};
    String16 di3DformatStr;
    char format3DStr[64] = {0};

    while (retry > 0) {
        retry--;
        di3Dformat[retry] = pThiz->getVideo3DFormat();
        //ALOGI("[detect3DThread]di3Dformat[%d]:%d\n", retry, di3Dformat[retry]);
        usleep(200000);//200ms
    }

    //get the 3d format which was detected most times
    for (int i = 0; i < RETRY_MAX - 1; i++) {
        for (int j = i + 1; j < RETRY_MAX; j++) {
            if (di3Dformat[i] == di3Dformat[j]) {
                times[i]++;
            }
        }
    }
    int max = times[0];
    int idx = 0;
    for (int i = 0; i < RETRY_MAX - 1; i++) {
        if (times[i] > max) {
            max = times[i];
            idx = i;
        }
    }
    //can't detect 3d format correctly, 3d off
    if (max == 1) {
        idx = 0;
        di3Dformat[0] = 0;
    }

    if (pThiz->mLogLevel > LOG_LEVEL_1) {
        ALOGI("[detect3DThread]after queue di3Dformat[%d]:%d\n", idx, di3Dformat[idx]);
    }

    pThiz->mDisplay3DFormat = di3Dformat[idx];
    pThiz->get3DFormatStr(di3Dformat[idx], format3DStr);
    pThiz->pDisplayMode->set3DMode(format3DStr);
    return NULL;
}

void SystemControl::autoDetect3DForMbox() {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("autoDetect3DForMbox\n");
    }

    pthread_t id;
    int ret = pthread_create(&id, NULL, detect3DThread, this);
    if (ret != 0) {
        ALOGE("[autoDetect3DForMbox:%d]ERROR; pthread_create failed rc=%d\n",__LINE__, ret);
    }
}

void SystemControl::setDiBypassAll(int format) {
    if (FORMAT_3D_OFF == format) {
        writeSysfs(String16(DI_BYPASS_ALL), String16("0"));
    }
    else {
        writeSysfs(String16(DI_BYPASS_ALL), String16("1"));
    }
}

unsigned int SystemControl::get3DOperationByFormat(int format) {
    unsigned int operation = MODE_3D_DISABLE;

    switch (format) {
        case FORMAT_3D_OFF:
            operation = MODE_3D_DISABLE;
            break;
        case FORMAT_3D_AUTO:
            operation = MODE_3D_AUTO;
            break;
        case FORMAT_3D_SIDE_BY_SIDE:
            operation = MODE_3D_LR;
            break;
        case FORMAT_3D_TOP_AND_BOTTOM:
            operation = MODE_3D_TB;
            break;
        case FORMAT_3D_LINE_ALTERNATIVE:
            operation = MODE_3D_LA;
            break;
        case FORMAT_3D_FRAME_ALTERNATIVE:
            operation = MODE_3D_FA;
            break;
        case FORMAT_3D_TO_2D_LEFT_EYE:
            operation = MODE_3D_TO_2D_L;
            break;
        case FORMAT_3D_TO_2D_RIGHT_EYE:
            operation = MODE_3D_TO_2D_R;
            break;
        default:
            operation = MODE_3D_DISABLE;
            break;
    }

    return operation;
}

int SystemControl::get3DFormatByOperation(unsigned int operation) {
    int format = -1;

    switch (operation) {
        case MODE_3D_DISABLE:
            format = FORMAT_3D_OFF;
            break;
        case MODE_3D_AUTO:
            format = FORMAT_3D_AUTO;
            break;
        case MODE_3D_LR:
            format = FORMAT_3D_SIDE_BY_SIDE;
            break;
        case MODE_3D_TB:
            format = FORMAT_3D_TOP_AND_BOTTOM;
            break;
        case MODE_3D_LA:
            format = FORMAT_3D_LINE_ALTERNATIVE;
            break;
        case MODE_3D_FA:
            format = FORMAT_3D_FRAME_ALTERNATIVE;
            break;
        case MODE_3D_TO_2D_L:
            format = FORMAT_3D_TO_2D_LEFT_EYE;
            break;
        case MODE_3D_TO_2D_R:
            format = FORMAT_3D_TO_2D_RIGHT_EYE;
            break;
        default:
            format = FORMAT_3D_OFF;
            break;
    }

    return format;
}

int SystemControl::get3DFormatByVpp(int vpp3Dformat) {
    int format = -1;

    switch (vpp3Dformat) {
        case VPP_3D_MODE_NULL:
            format = FORMAT_3D_OFF;
            break;
        case VPP_3D_MODE_LR:
            format = FORMAT_3D_SIDE_BY_SIDE;
            break;
        case VPP_3D_MODE_TB:
            format = FORMAT_3D_TOP_AND_BOTTOM;
            break;
        case VPP_3D_MODE_LA:
            format = FORMAT_3D_LINE_ALTERNATIVE;
            break;
        case VPP_3D_MODE_FA:
            format = FORMAT_3D_FRAME_ALTERNATIVE;
            break;
        default:
            format = FORMAT_3D_OFF;
            break;
    }

    return format;
}

void SystemControl::get3DFormatStr(int format, char *str) {
    switch (format) {
        case FORMAT_3D_OFF:
            strcpy(str, "3doff");
            break;
        case FORMAT_3D_AUTO:
            strcpy(str, "3dauto");
            break;
        case FORMAT_3D_SIDE_BY_SIDE:
            strcpy(str, "3dlr");
            break;
        case FORMAT_3D_TOP_AND_BOTTOM:
            strcpy(str, "3dtb");
            break;
        case FORMAT_3D_LINE_ALTERNATIVE:
            break;
        case FORMAT_3D_FRAME_ALTERNATIVE:
            break;
        default:
            strcpy(str, "3doff");
            break;
    }
}

void SystemControl::setDigitalMode(const String16& mode) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("set Digital mode :%s", String8(mode).string());
    }

    pDisplayMode->setDigitalMode(String8(mode).string());
}

void SystemControl::setListener(const sp<ISystemControlNotify>& listener) {
    pDisplayMode->setListener(listener);
}

void SystemControl::setOsdMouseMode(const String16& mode) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("set osd mouse mode :%s", String8(mode).string());
    }

    pDisplayMode->setOsdMouse(String8(mode).string());
}

void SystemControl::setOsdMousePara(int x, int y, int w, int h) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("set osd mouse parameter x:%d y:%d w:%d h:%d", x, y, w, h);
    }
    pDisplayMode->setOsdMouse(x, y, w, h);
}

void SystemControl::setPosition(int left, int top, int width, int height) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("set position x:%d y:%d w:%d h:%d", left, top, width, height);
    }
    pDisplayMode->setPosition(left, top, width, height);
}

void SystemControl::getPosition(const String16& mode, int &x, int &y, int &w, int &h) {
    int position[4] = { 0, 0, 0, 0 };
    pDisplayMode->getPosition(String8(mode).string(), position);
    x = position[0];
    y = position[1];
    w = position[2];
    h = position[3];
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("get position x:%d y:%d w:%d h:%d", x, y, w, h);
    }
}

void SystemControl::reInit() {
    bootenv_reinit();
}

void SystemControl::instabootResetDisplay() {
    pDisplayMode->reInit();
}


void SystemControl::setNativeWindowRect(int x, int y, int w, int h) {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("set native window rect x:%d y:%d w:%d h:%d", x, y, w, h);
    }
    pDisplayMode->setNativeWindowRect(x, y, w, h);
}

void SystemControl::setVideoPlayingAxis() {
    if (mLogLevel > LOG_LEVEL_1) {
        ALOGI("set video playing axis");
    }
    pDisplayMode->setVideoPlayingAxis();
}

void SystemControl::traceValue(const String16& type, const String16& key, const String16& value) {
    if (mLogLevel > LOG_LEVEL_0) {
        String16 procName;
        int pid = IPCThreadState::self()->getCallingPid();
        int uid = IPCThreadState::self()->getCallingUid();

        getProcName(pid, procName);

        ALOGI("%s [ %s ] [ %s ] from pid=%d, uid=%d, process name=%s",
            String8(type).string(), String8(key).string(), String8(value).string(),
            pid, uid,
            String8(procName).string());
    }
}

void SystemControl::setLogLevel(int level) {
    if (level > (LOG_LEVEL_TOTAL - 1)) {
        ALOGE("out of range level=%d, max=%d", level, LOG_LEVEL_TOTAL);
        return;
    }

    mLogLevel = level;
    pSysWrite->setLogLevel(level);
    pDisplayMode->setLogLevel(level);
}

int SystemControl::getLogLevel() {
    return mLogLevel;
}

int SystemControl::getProcName(pid_t pid, String16& procName) {
    char proc_path[MAX_STR_LEN];
    char cmdline[64];
    int fd;

    strcpy(cmdline, "unknown");

    sprintf(proc_path, "/proc/%d/cmdline", pid);
    fd = open(proc_path, O_RDONLY);
    if (fd >= 0) {
        int rc = read(fd, cmdline, sizeof(cmdline)-1);
        cmdline[rc] = 0;
        close(fd);

        procName.setTo(String16(cmdline));
        return 0;
    }

    return -1;
}

status_t SystemControl::dump(int fd, const Vector<String16>& args) {
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    if (checkCallingPermission(String16("android.permission.DUMP")) == false) {
        snprintf(buffer, SIZE, "Permission Denial: "
                "can't dump system_control from pid=%d, uid=%d\n",
                IPCThreadState::self()->getCallingPid(),
                IPCThreadState::self()->getCallingUid());
        result.append(buffer);
    } else {
        Mutex::Autolock lock(mLock);

        int len = args.size();
        for (int i = 0; i < len; i ++) {
            String16 debugLevel("-l");
            String16 bootenv("-b");
            String16 display("-d");
            String16 hdcp("-hdcp");
            String16 help("-h");
            if (args[i] == debugLevel) {
                if (i + 1 < len) {
                    String8 levelStr(args[i+1]);
                    int level = atoi(levelStr.string());
                    setLogLevel(level);

                    result.appendFormat(String8::format("Setting log level to %d.\n", level));
                    break;
                }
            }
            else if (args[i] == bootenv) {
                if((i + 3 < len) && (args[i + 1] == String16("set"))){
                    setBootEnv(args[i+2], args[i+3]);

                    result.appendFormat(String8::format("set bootenv key:[%s] value:[%s]\n",
                        String8(args[i+2]).string(), String8(args[i+3]).string()));
                    break;
                }
                else if (((i + 2) <= len) && (args[i + 1] == String16("get"))) {
                    if ((i + 2) == len) {
                        result.appendFormat("get all bootenv\n");
                        bootenv_print();
                    }
                    else {
                        String16 value;
                        getBootEnv(args[i+2], value);

                        result.appendFormat(String8::format("get bootenv key:[%s] value:[%s]\n",
                            String8(args[i+2]).string(), String8(value).string()));
                    }
                    break;
                }
                else {
                    result.appendFormat(
                        "dump bootenv format error!! should use:\n"
                        "dumpsys system_control -b [set |get] key value \n");
                }
            }
            else if (args[i] == display) {
                /*
                String8 displayInfo;
                pDisplayMode->dump(displayInfo);
                result.append(displayInfo);*/

                char buf[4096] = {0};
                pDisplayMode->dump(buf);
                result.append(String8(buf));
                break;
            }
            else if (args[i] == hdcp) {
                pDisplayMode->hdcpSwitch();
                break;
            }
            else if (args[i] == help) {
                result.appendFormat(
                    "system_control service use to control the system sysfs property and boot env \n"
                    "in multi-user mode, normal process will have not system privilege \n"
                    "usage: \n"
                    "dumpsys system_control -l value \n"
                    "dumpsys system_control -b [set |get] key value \n"
                    "-l: debug level \n"
                    "-b: set or get bootenv \n"
                    "-d: dump display mode info \n"
                    "-hdcp: stop hdcp and start hdcp tx \n"
                    "-h: help \n");
            }
        }
    }
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

} // namespace android

