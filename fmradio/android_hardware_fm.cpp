/*
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *            notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *            notice, this list of conditions and the following disclaimer in the
 *            documentation and/or other materials provided with the distribution.
 *        * Neither the name of Code Aurora nor
 *            the names of its contributors may be used to endorse or promote
 *            products derived from this software without specific prior written
 *            permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "fmradio"

#include "jni.h"
#include "nativehelper/JNIHelp.h"
#include "utils/Log.h"
#include "utils/misc.h"
#include "android_runtime/AndroidRuntime.h"
#include <cutils/properties.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>

// ioctl command
#define FM_IOCTL_BASE     'R'
#define FM_IOCTL_ENABLE      _IOW(FM_IOCTL_BASE, 0, int)
#define FM_IOCTL_GET_ENABLE  _IOW(FM_IOCTL_BASE, 1, int)
#define FM_IOCTL_SET_TUNE    _IOW(FM_IOCTL_BASE, 2, int)
#define FM_IOCTL_GET_FREQ    _IOW(FM_IOCTL_BASE, 3, int)
#define FM_IOCTL_SEARCH      _IOW(FM_IOCTL_BASE, 4, int[4])
#define FM_IOCTL_STOP_SEARCH _IOW(FM_IOCTL_BASE, 5, int)
#define FM_IOCTL_SET_VOLUME  _IOW(FM_IOCTL_BASE, 7, int)
#define FM_IOCTL_GET_VOLUME  _IOW(FM_IOCTL_BASE, 8, int)

// operation result
#define FM_JNI_SUCCESS  0L
#define FM_JNI_FAILURE  -1L

// search direction
#define SEARCH_DOWN 0
#define SEARCH_UP   1

// operation command
#define V4L2_CID_PRIVATE_BASE           0x8000000
#define V4L2_CID_PRIVATE_TAVARUA_STATE  (V4L2_CID_PRIVATE_BASE + 4)

#define V4L2_CTRL_CLASS_USER            0x980000
#define V4L2_CID_BASE                   (V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_AUDIO_VOLUME           (V4L2_CID_BASE + 5)
#define V4L2_CID_AUDIO_MUTE             (V4L2_CID_BASE + 9)


using namespace android;

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_acquireFdNative
        (JNIEnv* env, jobject thiz, jstring path)
{
    int fd;
    int i;
    char value = 0;
    int init_success = 0;
    jboolean isCopy;
    const char* radio_path = env->GetStringUTFChars(path, &isCopy);
    LOGE("(acquire)radio_path=%s \n", radio_path);
    if(radio_path == NULL){
        return FM_JNI_FAILURE;
    }

    fd = open(radio_path, O_RDONLY);
    LOGE("(acquire)fd=%d errno=%d \n", fd, errno);

    if(isCopy == JNI_TRUE){
        env->ReleaseStringUTFChars(path, radio_path);
    }
    if(fd < 0){
        return FM_JNI_FAILURE;
    }

    return fd;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_closeFdNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    close(fd);
    return FM_JNI_SUCCESS;
}

/********************************************************************
 * Current JNI
 *******************************************************************/

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_getFreqNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int freq = -1;
    int err = ioctl(fd, FM_IOCTL_GET_FREQ, &freq);
    LOGE("(getFreq)result=%d errno=%d freq=%d\n", err, errno, freq);
    if(err < 0){
      return FM_JNI_FAILURE;
    }
    return freq;
}

/*native interface */
static jint android_hardware_fmradio_FmReceiverJNI_setFreqNative
    (JNIEnv * env, jobject thiz, jint fd, jint freq)
{
    int err = ioctl(fd, FM_IOCTL_SET_TUNE, &freq);
    LOGE("(setFreq)freq=%d result=%d errno=%d\n", freq, err, errno);
    if(err < 0){
        return FM_JNI_FAILURE;
    }
    return FM_JNI_SUCCESS;
}

static int gVolume = 0;

static jint android_hardware_fmradio_FmReceiverJNI_setControlNative
    (JNIEnv * env, jobject thiz, jint fd, jint id, jint value)
{
    int err = -1;
    switch(id) {
        case V4L2_CID_PRIVATE_TAVARUA_STATE:
        {
            err = ioctl(fd, FM_IOCTL_ENABLE, &value);
        }
        break;

        case V4L2_CID_AUDIO_VOLUME:
        {
            gVolume = value;
            err = ioctl(fd, FM_IOCTL_SET_VOLUME, &value);
        }
        break;

        case V4L2_CID_AUDIO_MUTE:
        {
            if (value == 0) {
                err = ioctl(fd, FM_IOCTL_SET_VOLUME, &gVolume);
            } else {
                int volume = -1;
                err = ioctl(fd, FM_IOCTL_GET_VOLUME, &volume);
                if (err >= 0) {
                    gVolume = volume;
                    volume = 0;
                    err = ioctl(fd, FM_IOCTL_SET_VOLUME, &volume);
                }
            }
        }
        break;
    }

    LOGE("(setControl)operation=%x value=%d result=%d errno=%d", id, value, err, errno);
    if (err < 0) {
        return FM_JNI_FAILURE;
    }

    return FM_JNI_SUCCESS;
}

static jint android_hardware_fmradio_FmReceiverJNI_getControlNative
    (JNIEnv * env, jobject thiz, jint fd, jint id)
{
    return FM_JNI_FAILURE;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_startSearchNative
    (JNIEnv * env, jobject thiz, jint fd, jint freq, jint dir, jint timeout, jint reserve)
{
    int buffer[4] = {0};
    buffer[0] = freq;    //start frequency
    buffer[1] = dir;     //search direction
    buffer[2] = timeout; //timeout
    buffer[3] = 0;       //reserve
    int err = -1;
	int count = 0;
    /*do {
        err = ioctl(fd, FM_IOCTL_SEARCH, buffer);
        if (err < 0 && errno == 11) {
            if (dir == 0) {
                buffer[0] += 1;
            } else {
                buffer[0] -= 1;
            }
        } else {
            break;
        }
    } while (1);*/
	do {
        err = ioctl(fd, FM_IOCTL_SEARCH, buffer);
        if (err < 0 && count<3) {
            if (dir == 0) {
                buffer[0] += 1;
            } else {
                buffer[0] -= 1;
            }
			count++;
			LOGE("err=%d count=%d\n", err, count);
        } else {
            break;
        }
    } while (1);
	
	LOGE("err=%d\n", err);
    LOGE("(seek)freq=%d direction=%d timeout=%d reserve=%d result=%d errno=%d\n", freq, dir, timeout, reserve, err, errno);
    if(err < 0){
        return FM_JNI_FAILURE;
    }
    return FM_JNI_SUCCESS;
}

/* native interface */
static jint android_hardware_fmradio_FmReceiverJNI_cancelSearchNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    int err = ioctl(fd, FM_IOCTL_STOP_SEARCH);
    if(err < 0){
        return FM_JNI_FAILURE;
    }
    return FM_JNI_SUCCESS;
}

/* native interface */
// not support
static jint android_hardware_fmradio_FmReceiverJNI_getRSSINative
    (JNIEnv * env, jobject thiz, jint fd)
{
    return FM_JNI_FAILURE;
}

/* native interface */
// not support
static jint android_hardware_fmradio_FmReceiverJNI_setBandNative
    (JNIEnv * env, jobject thiz, jint fd, jint low, jint high)
{
    return FM_JNI_FAILURE;
}

/* native interface */
// not support
static jint android_hardware_fmradio_FmReceiverJNI_setRegionNative
    (JNIEnv * env, jobject thiz, jint fd, jint region)
{
    return FM_JNI_FAILURE;
}

/* native interface */
// not support
static jint android_hardware_fmradio_FmReceiverJNI_getLowerBandNative
    (JNIEnv * env, jobject thiz, jint fd)
{
    return FM_JNI_FAILURE;
}

// not support
static jint android_hardware_fmradio_FmReceiverJNI_setMonoStereoNative
    (JNIEnv * env, jobject thiz, jint fd, jint val)
{
    return FM_JNI_FAILURE;
}

/* native interface */
// not support
static jint android_hardware_fmradio_FmReceiverJNI_getBufferNative
 (JNIEnv * env, jobject thiz, jint fd, jbyteArray buff, jint index)
{
    return FM_JNI_FAILURE;
}

/* native interface */
// not support
static jint android_hardware_fmradio_FmReceiverJNI_getRawRdsNative
 (JNIEnv * env, jobject thiz, jint fd, jbooleanArray buff, jint count)
{
    return FM_JNI_FAILURE;
}

/*
 * JNI registration.
 */
static JNINativeMethod gMethods[] = {
        /* name, signature, funcPtr */
        { "acquireFdNative", "(Ljava/lang/String;)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_acquireFdNative},
        { "closeFdNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_closeFdNative},
        { "getFreqNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getFreqNative},
        { "setFreqNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setFreqNative},
        { "getControlNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getControlNative},
        { "setControlNative", "(III)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setControlNative},
        { "startSearchNative", "(IIIII)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_startSearchNative},
        { "cancelSearchNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_cancelSearchNative},
        { "getRSSINative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getRSSINative},
        { "setBandNative", "(III)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setBandNative},
        { "setRegionNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setRegionNative},
        { "getLowerBandNative", "(I)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getLowerBandNative},
        { "getBufferNative", "(I[BI)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getBufferNative},
        { "setMonoStereoNative", "(II)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_setMonoStereoNative},
        { "getRawRdsNative", "(I[BI)I",
            (void*)android_hardware_fmradio_FmReceiverJNI_getRawRdsNative},
};

int register_android_hardware_fm_fmradio(JNIEnv* env)
{
	return AndroidRuntime::registerNativeMethods(env, "android/hardware/fmradio/FmReceiverJNI", gMethods, NELEM(gMethods));
}
