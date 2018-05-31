/*
 * Copyright (C) 2009 The Android Open Source Project
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
 */

package com.droidlogic.app;

import android.content.Context;
import android.util.Log;

public class DisplayPositionManager {
    private final static String TAG = "DisplayPositionManager";
    private final static boolean DEBUG = false;
    private Context mContext = null;
    private SystemControlManager mSystenControl = null;
    SystemControlManager.DisplayInfo mDisplayInfo;
    private OutputModeManager mOutputModeManager = null;

    private final static int MAX_Height = 100;
    private final static int MIN_Height = 80;
    private static int screen_rate = MAX_Height;

    private String mCurrentLeftString = null;
    private String mCurrentTopString = null;
    private String mCurrentWidthString = null;
    private String mCurrentHeightString = null;

    private int mCurrentLeft = 0;
    private int mCurrentTop = 0;
    private int mCurrentWidth = 0;
    private int mCurrentHeight = 0;
    private int mCurrentRate = MAX_Height;
    private int mCurrentRight = 0;
    private int mCurrentBottom = 0;

    private int mPreLeft = 0;
    private int mPreTop = 0;
    private int mPreRight = 0;
    private int mPreBottom = 0;
    private int mPreWidth = 0;
    private int mPreHeight = 0;

    private  String mCurrentMode = null;

    private int mMaxRight = 0;
    private int mMaxBottom=0;
    private int offsetStep = 2;  // because 20% is too large ,so we divide a value to smooth the view

    public DisplayPositionManager(Context context) {
        mContext = context;
        mSystenControl = new SystemControlManager(mContext);
        mOutputModeManager = new OutputModeManager(mContext);
        mDisplayInfo = mSystenControl.getDisplayInfo();
        initPostion();
    }

    public void initPostion() {
        mCurrentMode = mOutputModeManager.getCurrentOutputMode();
        initStep(mCurrentMode);
        initCurrentPostion();
        screen_rate = getInitialRateValue();
    }

    private void initCurrentPostion() {
        int [] position = mOutputModeManager.getPosition(mCurrentMode);
        mPreLeft = mCurrentLeft = position[0];
        mPreRight = mCurrentTop  = position[1];
        mPreWidth = mCurrentWidth = position[2];
        mPreHeight = mCurrentHeight= position[3];
    }

    public int getInitialRateValue() {
        mCurrentMode = mOutputModeManager.getCurrentOutputMode();
        initStep(mCurrentMode);
        int m = (100*2*offsetStep)*mPreLeft ;
        if (m == 0) {
            return 100;
        }
        int rate =  100 - m/(mMaxRight+1) - 1;
        return rate;
    }

    public int getCurrentRateValue(){
        return screen_rate;
    }

    private void zoom(int step) {
        screen_rate = screen_rate + step;
        if (screen_rate >MAX_Height) {
            screen_rate = MAX_Height;
        }else if (screen_rate <MIN_Height) {
            screen_rate = MIN_Height ;
        }
        zoomByPercent(screen_rate);
    }

    public void zoomIn(){
        zoom(1);
    }

    public void zoomOut(){
        zoom(-1);
    }

    public void saveDisplayPosition() {
        if ( !isScreenPositionChanged())
            return;

        mOutputModeManager.savePosition(mCurrentLeft, mCurrentTop, mCurrentWidth, mCurrentHeight);
    }

    private void writeFile(String file, String value) {
        mSystenControl.writeSysFs(file, value);
    }

    private void initStep(String mode) {
        if (mode.contains(OutputModeManager.HDMI_480)) {
            mMaxRight = 719;
            mMaxBottom = 479;
        }else if (mode.contains(OutputModeManager.HDMI_576)) {
            mMaxRight = 719;
            mMaxBottom = 575;
        }else if (mode.contains(OutputModeManager.HDMI_720)) {
            mMaxRight = 1279;
            mMaxBottom = 719;
        }else if (mode.contains(OutputModeManager.HDMI_1080)) {
            mMaxRight = 1919;
            mMaxBottom = 1079;
        }else if (mode.contains(OutputModeManager.HDMI_4K2K)) {
            mMaxRight = 3839;
            mMaxBottom = 2159;
        } else if (mode.contains(OutputModeManager.HDMI_SMPTE)) {
            mMaxRight = 4095;
            mMaxBottom = 2159;
        } else if (mode.contains(OutputModeManager.HDMI_640_480)) {
            mMaxRight = 639;
            mMaxBottom = 479;
        } else if (mode.contains(OutputModeManager.HDMI_800_480)) {
            mMaxRight = 799;
            mMaxBottom = 479;
        } else if (mode.contains(OutputModeManager.HDMI_800_600)) {
            mMaxRight = 799;
            mMaxBottom = 599;
        } else if (mode.contains(OutputModeManager.HDMI_1024_600)) {
            mMaxRight = 1023;
            mMaxBottom = 599;
        } else if (mode.contains(OutputModeManager.HDMI_1024_768)) {
            mMaxRight = 1023;
            mMaxBottom = 767;
        } else if (mode.contains(OutputModeManager.HDMI_1280_800)) {
            mMaxRight = 1279;
            mMaxBottom = 799;
        } else if (mode.contains(OutputModeManager.HDMI_1280_1024)) {
            mMaxRight = 1279;
            mMaxBottom = 1023;
        } else if (mode.contains(OutputModeManager.HDMI_1360_768)) {
            mMaxRight = 1359;
            mMaxBottom = 767;
        } else if (mode.contains(OutputModeManager.HDMI_1366_768)) {
            mMaxRight = 1365;
            mMaxBottom = 767;
        } else if (mode.contains(OutputModeManager.HDMI_1440_900)) {
            mMaxRight = 1439;
            mMaxBottom = 899;
        } else if (mode.contains(OutputModeManager.HDMI_1600_900)) {
            mMaxRight = 1599;
            mMaxBottom = 899;
        } else if (mode.contains(OutputModeManager.HDMI_1600_1200)) {
            mMaxRight = 1599;
            mMaxBottom = 1199;
        } else if (mode.contains(OutputModeManager.HDMI_1920_1200)) {
            mMaxRight = 1919;
            mMaxBottom = 1199;
        } else {
            mMaxRight = 1919;
            mMaxBottom = 1079;
        }
    }

    public void zoomByPercent(int percent){

        if (percent > 100 ) {
            percent = 100;
            return ;
        }

        if (percent < 80 ) {
            percent = 80;
            return ;
        }

        mCurrentMode = mOutputModeManager.getCurrentOutputMode();
        initStep(mCurrentMode);

        mCurrentLeft = (100-percent)*(mMaxRight)/(100*2*offsetStep);
        mCurrentTop  = (100-percent)*(mMaxBottom)/(100*2*offsetStep);
        mCurrentRight = mMaxRight - mCurrentLeft;
        mCurrentBottom = mMaxBottom - mCurrentTop;
        mCurrentWidth = mCurrentRight - mCurrentLeft + 1;
        mCurrentHeight = mCurrentBottom - mCurrentTop + 1;

        setPosition(mCurrentLeft, mCurrentTop,mCurrentRight, mCurrentBottom, 0);
    }
    private void setPosition(int l, int t, int r, int b, int mode) {
        String str = "";
        int left =  l;
        int top =  t;
        int right =  r;
        int bottom =  b;
        int width = mCurrentWidth;
        int height = mCurrentHeight;

        if (left < 0) {
            left = 0 ;
        }

        if (top < 0) {
            top = 0 ;
        }
        right = Math.min(right,mMaxRight);
        bottom = Math.min(bottom,mMaxBottom);

        writeFile(OutputModeManager.FB0_WINDOW_AXIS, left + " " + top + " " + right + " " + bottom);

        mOutputModeManager.savePosition(left, top, width, height);
        mOutputModeManager.setOsdMouse(left, top, width, height);
    }

    public boolean isScreenPositionChanged(){
        if (mPreLeft== mCurrentLeft && mPreTop == mCurrentTop
            && mPreWidth == mCurrentWidth && mPreHeight == mCurrentHeight)
            return false;
        else
            return true;
    }

    public void zoomByPosition(int x, int y, int w, int h){
        int right = x+w-1;
        int bottom = y+h-1;
        writeFile(OutputModeManager.FB0_WINDOW_AXIS, x + " " + y + " " + right + " " + bottom);
        mOutputModeManager.savePosition(x, y, w, h);
        mOutputModeManager.setOsdMouse(x, y, w, h);
    }
}
