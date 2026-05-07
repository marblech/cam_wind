package com.marble.cammon;

/**
 * PTZStatus - parsed PTZ/status values from camera
 */
public class PTZStatus {
    public final float az;
    public final float el;
    public final float irFocus;
    public final float visFocus;

    public PTZStatus(float az, float el, float irFocus, float visFocus) {
        this.az = az;
        this.el = el;
        this.irFocus = irFocus;
        this.visFocus = visFocus;
    }
}
