#include "TMAG_joy.h"
#include "main.h"
#include "knobs_autogen.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float fclamp(float v, float lo, float hi)
{ return (v < lo) ? lo : (v > hi) ? hi : v; }

static inline uint8_t qnext(uint8_t x)
{ return (uint8_t)((x + 1u) & (TMAGJOY_QSIZE - 1u)); }

#ifndef TMAG_JOY_DEFAULT_DEADZONE
#define TMAG_JOY_DEFAULT_DEADZONE 0.30f
#endif

static struct {
    float cx, cy;
    float sx, sy;
    float cphi, sphi;
    float deadzone;
    uint8_t invert_x;
    uint8_t invert_y;
} s_cal = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, TMAG_JOY_DEFAULT_DEADZONE, 0u, 0u };

static uint8_t s_use_hz = 1u;
static uint8_t s_in_neutral = 1u;
static float s_dz_in = 0.34f;
static float s_dz_out = 0.38f;
static uint8_t s_abs_dz_en = 0u;
static float s_abs_dz_mT = 12.0f;
static float s_dir_bias_rad = 0.0f;
static TMAGJoy_Dir s_digital_latched = TMAGJOY_NEUTRAL;

static void joy_reset(void)
{
    s_cal.cx = 0.0f;
    s_cal.cy = 0.0f;
    s_cal.sx = 1.0f;
    s_cal.sy = 1.0f;
    s_cal.cphi = 1.0f;
    s_cal.sphi = 0.0f;
    s_cal.deadzone = TMAG_JOY_DEFAULT_DEADZONE;
    s_cal.invert_x = 0u;
    s_cal.invert_y = 0u;
    s_use_hz = 1u;
    s_in_neutral = 1u;
    s_dz_in = 0.34f;
    s_dz_out = 0.38f;
    s_abs_dz_en = 0u;
    s_abs_dz_mT = 12.0f;
    s_dir_bias_rad = 0.0f;
    s_digital_latched = TMAGJOY_NEUTRAL;
}

static TMAGJoy_Dir dir8_from_angle(float ang)
{
    const float step = 3.14159265f / 4.0f;
    float a = ang + s_dir_bias_rad;
    int sector = (int)floorf((a + step * 0.5f) / step) & 7;
    static const TMAGJoy_Dir lut[8] = {
        TMAGJOY_RIGHT, TMAGJOY_UPRIGHT, TMAGJOY_UP, TMAGJOY_UPLEFT,
        TMAGJOY_LEFT, TMAGJOY_DOWNLEFT, TMAGJOY_DOWN, TMAGJOY_DOWNRIGHT
    };
    return lut[sector];
}

static TMAGJoy_Dir joy_read(float *norm_x, float *norm_y, float *radius_abs_mT)
{
    float x, y;
    if (TMAG5273_read_mT(&x, &y, NULL) != 0) return TMAGJOY_NEUTRAL;
    float dx = x - s_cal.cx;
    float dy = y - s_cal.cy;
    float r_abs = sqrtf(dx * dx + dy * dy);
    if (radius_abs_mT) *radius_abs_mT = r_abs;

    if (s_abs_dz_en && r_abs < s_abs_dz_mT) {
        if (norm_x) *norm_x = 0.0f;
        if (norm_y) *norm_y = 0.0f;
        return TMAGJOY_NEUTRAL;
    }

    float rx = s_cal.cphi * dx - s_cal.sphi * dy;
    float ry = s_cal.sphi * dx + s_cal.cphi * dy;
    if (s_cal.invert_x) rx = -rx;
    if (s_cal.invert_y) ry = -ry;
    float ux = rx / s_cal.sx;
    float uy = ry / s_cal.sy;
    float rN = sqrtf(ux * ux + uy * uy);

    if (s_use_hz) {
        if (s_in_neutral) {
            if (rN > s_dz_out) {
                s_in_neutral = 0u;
            } else {
                if (norm_x) *norm_x = 0.0f;
                if (norm_y) *norm_y = 0.0f;
                return TMAGJOY_NEUTRAL;
            }
        } else {
            if (rN < s_dz_in) {
                s_in_neutral = 1u;
                if (norm_x) *norm_x = 0.0f;
                if (norm_y) *norm_y = 0.0f;
                return TMAGJOY_NEUTRAL;
            }
        }
    } else {
        if (rN < s_cal.deadzone) {
            if (norm_x) *norm_x = 0.0f;
            if (norm_y) *norm_y = 0.0f;
            return TMAGJOY_NEUTRAL;
        }
    }

    if (norm_x) *norm_x = ux;
    if (norm_y) *norm_y = uy;
    return dir8_from_angle(atan2f(uy, ux));
}

int TMAGJoy_ReadCalibratedRaw(TMAGJoy *joy, float *nx, float *ny, float *r_abs_mT)
{
    (void)joy;
    float x, y;
    if (TMAG5273_read_mT(&x, &y, NULL) != 0) {
        if (nx) *nx = 0.0f;
        if (ny) *ny = 0.0f;
        if (r_abs_mT) *r_abs_mT = 0.0f;
        return -1;
    }

    float dx = x - s_cal.cx;
    float dy = y - s_cal.cy;
    float r_abs = sqrtf(dx * dx + dy * dy);
    if (r_abs_mT) *r_abs_mT = r_abs;

    if ((s_abs_dz_en != 0u) && (r_abs < s_abs_dz_mT)) {
        if (nx) *nx = 0.0f;
        if (ny) *ny = 0.0f;
        return 0;
    }

    float rx = s_cal.cphi * dx - s_cal.sphi * dy;
    float ry = s_cal.sphi * dx + s_cal.cphi * dy;
    if (s_cal.invert_x) rx = -rx;
    if (s_cal.invert_y) ry = -ry;

    float sx = s_cal.sx;
    float sy = s_cal.sy;
    if (sx < 1e-3f) sx = 1.0f;
    if (sy < 1e-3f) sy = 1.0f;
    if (nx) *nx = rx / sx;
    if (ny) *ny = ry / sy;
    return 0;
}

void TMAGJoy_GetCal(TMAGJoy *joy, TMAGJoy_Cal *out)
{
    (void)joy;
    if (!out) return;
    out->cx = s_cal.cx;
    out->cy = s_cal.cy;
    out->sx = s_cal.sx;
    out->sy = s_cal.sy;
    out->rot_deg = atan2f(s_cal.sphi, s_cal.cphi) * (180.0f / 3.14159265f);
    out->invert_x = s_cal.invert_x;
    out->invert_y = s_cal.invert_y;
}

void TMAGJoy_GetThresholds(TMAGJoy *joy, float *thr_x_mT, float *thr_y_mT)
{
    if (!joy) return;
    if (thr_x_mT) *thr_x_mT = joy->cfg.thr_x_mT;
    if (thr_y_mT) *thr_y_mT = joy->cfg.thr_y_mT;
}

void TMAGJoy_GetAbsDeadzone(TMAGJoy *joy, uint8_t *en, float *mT)
{
    if (!joy) {
        if (en) *en = 0u;
        if (mT) *mT = 0.0f;
        return;
    }
    if (en) *en = joy->cfg.abs_deadzone_en;
    if (mT) *mT = joy->cfg.abs_deadzone_mT;
}


// --- Proportional analog config (software layer) ---
static struct {
    float dz;      // radial deadzone in normalized units (0..0.9)
    float gamma;   // response curve exponent (1.0 = linear; 1.5 softer; 0.7 snappier)
} sJoyCfg = { 0.20f, 1.0f };

static float joy_effective_deadzone_norm(void)
{
    float dz = fclamp(sJoyCfg.dz, 0.0f, 0.90f);

    if (s_abs_dz_en != 0u) {
        float sx = fabsf(s_cal.sx);
        float sy = fabsf(s_cal.sy);
        float span_ref;
        float dz_from_abs;

        if (sx < 1e-3f) sx = 1.0f;
        if (sy < 1e-3f) sy = 1.0f;

        /* Conservative choice: smaller span means larger normalized deadzone. */
        span_ref = fminf(sx, sy);
        if (span_ref < 1e-3f) {
            span_ref = 1.0f;
        }

        dz_from_abs = s_abs_dz_mT / span_ref;
        dz_from_abs = fclamp(dz_from_abs, 0.0f, 0.90f);
        if (dz_from_abs > dz) {
            dz = dz_from_abs;
        }
    }

    return dz;
}

static void joy_apply_radial_shape(float *nx, float *ny)
{
    float x;
    float y;
    float r;
    float dz = joy_effective_deadzone_norm();

    if ((nx == NULL) || (ny == NULL)) {
        return;
    }

    x = *nx;
    y = *ny;
    r = sqrtf((x * x) + (y * y));
    if (r <= dz) {
        *nx = 0.0f;
        *ny = 0.0f;
        return;
    }

    {
        float k = (r - dz) / (1.0f - dz);
        float scale;

        if (k < 0.0f) {
            k = 0.0f;
        }
        if (k > 1.0f) {
            k = 1.0f;
        }
        if (sJoyCfg.gamma != 1.0f) {
            k = powf(k, sJoyCfg.gamma);
        }

        scale = (r > 1e-6f) ? (k / r) : 0.0f;
        *nx = fclamp(x * scale, -1.0f, 1.0f);
        *ny = fclamp(y * scale, -1.0f, 1.0f);
    }
}

static TMAGJoy gJoy;

static TMAGJoy_Dir s_joy_menu_last = TMAGJOY_NEUTRAL;
static uint8_t s_joy_inited = 0u;
static uint8_t s_joy_wait_neutral = 0u;
static uint8_t s_joy_neutral_cnt = 0u;
static uint8_t s_joy_use_irq = 0u;
static uint8_t s_joy_menu_ready = 0u;

#define JOY_NEUTRAL_STABLE_COUNT 3u
#define JOY_NEUTRAL_NORM_THRESH 0.30f
#define JOY_MENU_PRESS_NORM     0.45f
#define JOY_MENU_RELEASE_NORM   0.25f

static void Joy_DisableExtiLine(void);
static void Joy_ConfigExtiPin(uint8_t enable);



// ---------- Defaults & init ----------
static int joy_begin_default(void)
{
    joy_reset();
    int rc = TMAG5273_init_default();
    rc |= TMAG5273_set_magnetic_channels(TMAG5273_CH_XY);
    if (rc == 0) {
        printf("JOY: begin (XY only, dz=%.2f, absDZ=%u @ %.1fmT)\r\n",
               s_cal.deadzone, (unsigned)s_abs_dz_en, s_abs_dz_mT);
    }
    return (rc == 0) ? 0 : -1;
}

TMAGJoy_Config TMAGJoy_DefaultConfig(void)
{
    TMAGJoy_Config c;

    c.int_port = NULL;
    c.int_pin  = 0;

    c.mode         = TMAG5273_MODE_CONTINUOUS;
    c.sleep_time_n = 0;

    c.irq_source     = TMAGJOY_IRQ_RESULT;
    c.int_pulse_10us = 1;
    c.thr_x_mT = 3.0f;
    c.thr_y_mT = 3.0f;

    c.use_hysteresis  = 1;
    c.dz_norm_in      = 0.25f;
    c.dz_norm_out     = 0.35f;
    c.abs_deadzone_en = 0;
    c.abs_deadzone_mT = 2.0f;
    c.invert_x = 0;
    c.invert_y = 0;
    c.extra_rotate_deg = 0.0f;
    c.dir_bias_deg     = 0.0f;

    c.digital_thresh_norm = ((float)KNOB_SENSOR_JOY_DIGITAL_PRESS_PERMILLE) / 1000.0f;

    return c;
}

int TMAGJoy_Init(TMAGJoy *joy, const TMAGJoy_Config *cfg)
{
    if (!joy || !cfg) return -1;
    memset(joy, 0, sizeof *joy);
    joy->cfg = *cfg;

    if (joy_begin_default() != 0) return -1;

    (void)TMAG5273_set_operating_mode(cfg->mode);
    (void)TMAG5273_set_sleep_time_n(cfg->sleep_time_n);

    if (cfg->irq_source == TMAGJOY_IRQ_RESULT) {
        (void)TMAG5273_config_int(true, false, cfg->int_pulse_10us,
                                  TMAG5273_INT_MODE_INT, false);
    } else {
        (void)TMAG5273_config_int(false, true, cfg->int_pulse_10us,
                                  TMAG5273_INT_MODE_INT, false);
        (void)TMAG5273_set_x_threshold_mT(cfg->thr_x_mT);
        (void)TMAG5273_set_y_threshold_mT(cfg->thr_y_mT);
    }

    if (cfg->use_hysteresis) {
        TMAGJoy_SetHysteresis(joy, 1u, cfg->dz_norm_in, cfg->dz_norm_out);
    } else {
        TMAGJoy_SetHysteresis(joy, 0u, 0.0f, 0.0f);
        TMAGJoy_SetDeadzoneNorm(joy, cfg->dz_norm_out);
    }
    TMAGJoy_SetAbsDeadzone(joy, cfg->abs_deadzone_en, cfg->abs_deadzone_mT);
    TMAGJoy_SetInvert(joy, cfg->invert_x, cfg->invert_y);
    if (cfg->extra_rotate_deg != 0.0f) {
        TMAGJoy_AddExtraRotation(joy, cfg->extra_rotate_deg);
    }
    if (cfg->dir_bias_deg != 0.0f) {
        TMAGJoy_SetDirBias(joy, cfg->dir_bias_deg);
    }

    (void)TMAG5273_get_device_status(); // clear any latched INT

    return 0;
}

int TMAGJoy_ReinitPreserveCal(TMAGJoy *joy)
{
    if (!joy) return -1;

    TMAGJoy_Cal cal = {0};
    TMAGJoy_GetCal(joy, &cal);
    TMAGJoy_Config cfg = joy->cfg;

    int rc = TMAGJoy_Init(joy, &cfg);
    if (rc != 0) {
        return rc;
    }

    /* Restore the calibrated alignment after hardware reinit. */
    TMAGJoy_SetCenter(joy, cal.cx, cal.cy);
    TMAGJoy_SetRotationDeg(joy, cal.rot_deg);
    TMAGJoy_SetInvert(joy, cal.invert_x, cal.invert_y);
    TMAGJoy_SetSpan(joy, cal.sx, cal.sy);
    return 0;
}

// ---------- Non-blocking calibration: Neutral ----------
void TMAGJoy_CalNeutral_Begin(TMAGJoy *joy, uint32_t window_ms, uint32_t sample_every_ms)
{
    if (!joy) return;
    if (sample_every_ms == 0) sample_every_ms = 10;

    joy->cal_neutral.active          = 1;
    joy->cal_neutral.t_start_ms      = 0;
    joy->cal_neutral.duration_ms     = window_ms ? window_ms : 2000;
    joy->cal_neutral.sample_every_ms = sample_every_ms;
    joy->cal_neutral.last_sample_ms  = 0;
    joy->cal_neutral.n               = 0;
    joy->cal_neutral.sum_x           = 0.0;
    joy->cal_neutral.sum_y           = 0.0;
    joy->cal_neutral.sum_x2          = 0.0;
    joy->cal_neutral.sum_y2          = 0.0;
}

bool TMAGJoy_CalNeutral_Step(TMAGJoy *joy, uint32_t now_ms, float *progress_0to1)
{
    if (!joy || !joy->cal_neutral.active) {
        if (progress_0to1) *progress_0to1 = 0.0f;
        return true;
    }

    if (joy->cal_neutral.t_start_ms == 0) {
        joy->cal_neutral.t_start_ms   = now_ms;
        joy->cal_neutral.last_sample_ms = now_ms;
    }

    if ((now_ms - joy->cal_neutral.last_sample_ms) >= joy->cal_neutral.sample_every_ms) {
        uint32_t slots = (now_ms - joy->cal_neutral.last_sample_ms) / joy->cal_neutral.sample_every_ms;
        if (slots > 16u) slots = 16u;
        while (slots > 0u) {
            float x, y;
            if (TMAG5273_read_mT(&x, &y, NULL) == 0) {
                joy->cal_neutral.sum_x += x;
                joy->cal_neutral.sum_y += y;
                joy->cal_neutral.sum_x2 += ((double)x * (double)x);
                joy->cal_neutral.sum_y2 += ((double)y * (double)y);
                joy->cal_neutral.n++;
            }
            joy->cal_neutral.last_sample_ms += joy->cal_neutral.sample_every_ms;
            slots--;
        }
    }

    const uint32_t elapsed = now_ms - joy->cal_neutral.t_start_ms;
    float p = (joy->cal_neutral.duration_ms > 0)
            ? (float)elapsed / (float)joy->cal_neutral.duration_ms : 1.0f;
    if (progress_0to1) *progress_0to1 = fclamp(p, 0.0f, 1.0f);

    if (elapsed >= joy->cal_neutral.duration_ms) {
        if (joy->cal_neutral.n > 0u) {
            const float cx = (float)(joy->cal_neutral.sum_x / (double)joy->cal_neutral.n);
            const float cy = (float)(joy->cal_neutral.sum_y / (double)joy->cal_neutral.n);
            const double n = (double)joy->cal_neutral.n;
            const double ex2 = joy->cal_neutral.sum_x2 / n;
            const double ey2 = joy->cal_neutral.sum_y2 / n;
            double var = (ex2 - ((double)cx * (double)cx)) + (ey2 - ((double)cy * (double)cy));
            float rms;
            float dz_scale = ((float)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_SCALE_PERMILLE) / 1000.0f;
            float dz_min = ((float)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MIN_MT_X10) / 10.0f;
            float dz_max = ((float)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MAX_MT_X10) / 10.0f;
            float dz;

            if (var < 0.0) {
                var = 0.0;
            }
            rms = (var > 0.0) ? sqrtf((float)var) : 0.0f;
            dz = rms * dz_scale;
            if (dz < dz_min) dz = dz_min;
            if (dz > dz_max) dz = dz_max;

            TMAGJoy_SetCenter(joy, cx, cy);
            TMAGJoy_SetAbsDeadzone(joy, 1u, dz);
        } else {
            (void)TMAGJoy_ZeroHere(joy);
        }
        joy->cal_neutral.active = 0;
        return true;
    }
    return false;
}

// ---------- Non-blocking calibration: Extents / ellipse ----------
void TMAGJoy_CalExtents_Begin(TMAGJoy *joy, uint32_t duration_ms, uint32_t sample_every_ms)
{
    if (!joy) return;
    if (sample_every_ms == 0) sample_every_ms = 20;

    joy->cal_ext.active          = 1;
    joy->cal_ext.t_start_ms      = 0;
    joy->cal_ext.duration_ms     = duration_ms ? duration_ms : 5000;
    joy->cal_ext.sample_every_ms = sample_every_ms;
    joy->cal_ext.last_sample_ms  = 0;

    joy->cal_ext.xmin =  1e9f; joy->cal_ext.xmax = -1e9f;
    joy->cal_ext.ymin =  1e9f; joy->cal_ext.ymax = -1e9f;

    joy->cal_ext.sumx = joy->cal_ext.sumy = 0.0;
    joy->cal_ext.sumxx = joy->cal_ext.sumyy = joy->cal_ext.sumxy = 0.0;
    joy->cal_ext.n = 0;
}

bool TMAGJoy_CalExtents_Step(TMAGJoy *joy, uint32_t now_ms, float *progress_0to1)
{
    if (!joy || !joy->cal_ext.active) {
        if (progress_0to1) *progress_0to1 = 0.0f;
        return true;
    }

    if (joy->cal_ext.t_start_ms == 0) {
        joy->cal_ext.t_start_ms   = now_ms;
        joy->cal_ext.last_sample_ms = now_ms;
    }

    if ((now_ms - joy->cal_ext.last_sample_ms) >= joy->cal_ext.sample_every_ms) {
        uint32_t slots = (now_ms - joy->cal_ext.last_sample_ms) / joy->cal_ext.sample_every_ms;
        if (slots > 16u) slots = 16u;
        while (slots > 0u) {
            float x, y;
            if (TMAG5273_read_mT(&x, &y, NULL) == 0) {
                float dx = x - s_cal.cx;
                float dy = y - s_cal.cy;
                float rx = s_cal.cphi * dx - s_cal.sphi * dy;
                float ry = s_cal.sphi * dx + s_cal.cphi * dy;
                if (s_cal.invert_x) rx = -rx;
                if (s_cal.invert_y) ry = -ry;

                if (rx < joy->cal_ext.xmin) joy->cal_ext.xmin = rx;
                if (rx > joy->cal_ext.xmax) joy->cal_ext.xmax = rx;
                if (ry < joy->cal_ext.ymin) joy->cal_ext.ymin = ry;
                if (ry > joy->cal_ext.ymax) joy->cal_ext.ymax = ry;

                joy->cal_ext.sumx  += rx;
                joy->cal_ext.sumy  += ry;
                joy->cal_ext.sumxx += (double)rx * (double)rx;
                joy->cal_ext.sumyy += (double)ry * (double)ry;
                joy->cal_ext.sumxy += (double)rx * (double)ry;
                joy->cal_ext.n++;
            }
            joy->cal_ext.last_sample_ms += joy->cal_ext.sample_every_ms;
            slots--;
        }
    }

    const uint32_t elapsed = now_ms - joy->cal_ext.t_start_ms;
    float p = (joy->cal_ext.duration_ms > 0)
            ? (float)elapsed / (float)joy->cal_ext.duration_ms : 1.0f;
    if (progress_0to1) *progress_0to1 = fclamp(p, 0.0f, 1.0f);

    if (elapsed >= joy->cal_ext.duration_ms) {
        if (joy->cal_ext.n > 10u) {
            const float sx_sweep = fmaxf(fabsf(joy->cal_ext.xmin), fabsf(joy->cal_ext.xmax));
            const float sy_sweep = fmaxf(fabsf(joy->cal_ext.ymin), fabsf(joy->cal_ext.ymax));
            const float sx = fmaxf(s_cal.sx, sx_sweep);
            const float sy = fmaxf(s_cal.sy, sy_sweep);
            TMAGJoy_SetSpan(joy, sx, sy);
        }
        joy->cal_ext.active = 0;
        return true;
    }
    return false;
}

// ---------- Convenience ----------
int TMAGJoy_ZeroHere(TMAGJoy *joy)
{
    (void)joy;
    float x, y;
    if (TMAG5273_read_mT(&x, &y, NULL) != 0) return -1;
    s_cal.cx = x;
    s_cal.cy = y;
    s_in_neutral = 1u;
    printf("JOY: zero_here cx=%.2f cy=%.2f\r\n", s_cal.cx, s_cal.cy);
    return 0;
}

void TMAGJoy_SetCenter(TMAGJoy *joy, float cx, float cy)
{
    (void)joy;
    s_cal.cx = cx;
    s_cal.cy = cy;
    s_in_neutral = 1u;
    s_digital_latched = TMAGJOY_NEUTRAL;
}

void TMAGJoy_SetSpan(TMAGJoy *joy, float sx, float sy)
{
    (void)joy;
    if (sx < 1e-3f) sx = 1.0f;
    if (sy < 1e-3f) sy = 1.0f;
    s_cal.sx = sx;
    s_cal.sy = sy;
    s_digital_latched = TMAGJOY_NEUTRAL;
}

void TMAGJoy_SetRotationDeg(TMAGJoy *joy, float deg)
{
    (void)joy;
    const float a = deg * (3.14159265f / 180.0f);
    s_cal.cphi = cosf(a);
    s_cal.sphi = sinf(a);
    s_digital_latched = TMAGJOY_NEUTRAL;
}

int TMAGJoy_SolveCardinals(TMAGJoy *joy,
                           float up_x, float up_y,
                           float right_x, float right_y,
                           float down_x, float down_y,
                           float left_x, float left_y,
                           float *fit_error_out)
{
    const float min_mag_mT = 0.6f;
    const float min_span_mT = 1.0f;
    const float max_fit_err = 8.0f;
    float vx[4];
    float vy[4];
    float mag[4];
    float rx[4];
    float ry[4];
    float best_phi = 0.0f;
    float best_err = 1.0e30f;
    float sx;
    float sy;
    uint8_t ix;
    uint8_t iy;
    uint8_t best_ix = 0u;
    uint8_t best_iy = 0u;
    uint8_t have_solution = 0u;
    uint32_t i;
    static const float tx[4] = { 1.0f, 0.0f, -1.0f, 0.0f }; /* R,U,L,D */
    static const float ty[4] = { 0.0f, 1.0f, 0.0f, -1.0f };

    if (fit_error_out) *fit_error_out = 0.0f;
    if (!joy) return -1;

    vx[0] = right_x; vy[0] = right_y;
    vx[1] = up_x;    vy[1] = up_y;
    vx[2] = left_x;  vy[2] = left_y;
    vx[3] = down_x;  vy[3] = down_y;

    for (i = 0u; i < 4u; i++) {
        mag[i] = sqrtf(vx[i] * vx[i] + vy[i] * vy[i]);
        if (mag[i] < min_mag_mT) {
            return -2;
        }
    }

    for (ix = 0u; ix < 2u; ix++) {
        for (iy = 0u; iy < 2u; iy++) {
            float sum_c = 0.0f;
            float sum_d = 0.0f;
            float err = 0.0f;
            float phi;
            float c;
            float s;

            for (i = 0u; i < 4u; i++) {
                const float nx = vx[i] / mag[i];
                const float ny = vy[i] / mag[i];
                const float tpx = ix ? -tx[i] : tx[i];
                const float tpy = iy ? -ty[i] : ty[i];
                sum_c += (nx * tpy) - (ny * tpx);
                sum_d += (nx * tpx) + (ny * tpy);
            }

            phi = atan2f(sum_c, sum_d);
            c = cosf(phi);
            s = sinf(phi);

            for (i = 0u; i < 4u; i++) {
                float tr_x = c * vx[i] - s * vy[i];
                float tr_y = s * vx[i] + c * vy[i];
                float nnx;
                float nny;
                float dx;
                float dy;

                if (ix) tr_x = -tr_x;
                if (iy) tr_y = -tr_y;

                nnx = tr_x / mag[i];
                nny = tr_y / mag[i];
                dx = nnx - tx[i];
                dy = nny - ty[i];
                err += (dx * dx) + (dy * dy);
            }

            if ((have_solution == 0u) || (err < best_err)) {
                have_solution = 1u;
                best_err = err;
                best_phi = phi;
                best_ix = ix;
                best_iy = iy;
            }
        }
    }

    if (have_solution == 0u) {
        return -3;
    }
    if (fit_error_out) *fit_error_out = best_err;
    if (best_err > max_fit_err) {
        return -4;
    }

    {
        const float c = cosf(best_phi);
        const float s = sinf(best_phi);

        for (i = 0u; i < 4u; i++) {
            float tr_x = c * vx[i] - s * vy[i];
            float tr_y = s * vx[i] + c * vy[i];
            if (best_ix) tr_x = -tr_x;
            if (best_iy) tr_y = -tr_y;
            rx[i] = tr_x;
            ry[i] = tr_y;
        }
    }

    sx = 0.5f * (fabsf(rx[0]) + fabsf(rx[2]));
    sy = 0.5f * (fabsf(ry[1]) + fabsf(ry[3]));
    if ((sx < min_span_mT) || (sy < min_span_mT)) {
        return -5;
    }

    TMAGJoy_SetRotationDeg(joy, best_phi * (180.0f / 3.14159265f));
    TMAGJoy_SetInvert(joy, best_ix, best_iy);
    TMAGJoy_SetSpan(joy, sx, sy);
    return 0;
}

// ---------- Reading ----------
TMAGJoy_Sample TMAGJoy_ReadAnalog(TMAGJoy *joy)
{
    (void)joy;
    TMAGJoy_Sample s = { TMAGJOY_NEUTRAL, 0, 0, 0 };
    float nx = 0.0f, ny = 0.0f, r_abs = 0.0f;

    TMAGJoy_Dir d = joy_read(&nx, &ny, &r_abs);

    joy_apply_radial_shape(&nx, &ny);
    s.dir = (nx == 0.0f && ny == 0.0f) ? TMAGJOY_NEUTRAL : d;

    s.nx = fclamp(nx, -1.0f, 1.0f);
    s.ny = fclamp(ny, -1.0f, 1.0f);
    s.r_abs_mT = r_abs;
    return s;
}

int TMAGJoy_ReadCalibratedShaped(TMAGJoy *joy, float *nx, float *ny, float *r_abs_mT)
{
    float x = 0.0f;
    float y = 0.0f;
    float r_abs = 0.0f;

    if (TMAGJoy_ReadCalibratedRaw(joy, &x, &y, &r_abs) != 0) {
        if (nx) *nx = 0.0f;
        if (ny) *ny = 0.0f;
        if (r_abs_mT) *r_abs_mT = 0.0f;
        return -1;
    }

    joy_apply_radial_shape(&x, &y);

    if (nx) *nx = x;
    if (ny) *ny = y;
    if (r_abs_mT) *r_abs_mT = r_abs;
    return 0;
}


TMAGJoy_Dir TMAGJoy_ReadDigital(TMAGJoy *joy)
{
    float nx = 0.0f, ny = 0.0f, r_abs = 0.0f;
    TMAGJoy_Dir d;
    float rN;
    float press_th;
    float release_th;

    if (!joy) {
        s_digital_latched = TMAGJOY_NEUTRAL;
        return TMAGJOY_NEUTRAL;
    }

    d = joy_read(&nx, &ny, &r_abs);
    (void)r_abs;

    rN = sqrtf(nx * nx + ny * ny);
    press_th = joy->cfg.digital_thresh_norm;
    if (press_th < 0.05f) {
        press_th = 0.05f;
    }
    if (press_th > 0.98f) {
        press_th = 0.98f;
    }

    release_th = press_th * (((float)KNOB_SENSOR_JOY_DIGITAL_RELEASE_PERCENT) / 100.0f);
    if (release_th < 0.05f) {
        release_th = 0.05f;
    }
    if (release_th > (press_th - 0.02f)) {
        release_th = press_th - 0.02f;
    }

    if (s_digital_latched == TMAGJOY_NEUTRAL) {
        if ((d == TMAGJOY_NEUTRAL) || (rN < press_th)) {
            return TMAGJOY_NEUTRAL;
        }
        s_digital_latched = d;
        return d;
    }

    /* If the directional solver already reports neutral, release immediately. */
    if (d == TMAGJOY_NEUTRAL) {
        s_digital_latched = TMAGJOY_NEUTRAL;
        return TMAGJOY_NEUTRAL;
    }

    if (rN <= release_th) {
        s_digital_latched = TMAGJOY_NEUTRAL;
        return TMAGJOY_NEUTRAL;
    }

    if (d != TMAGJOY_NEUTRAL) {
        s_digital_latched = d;
    }
    return s_digital_latched;
}

// ---------- Optional INT queue ----------
void TMAGJoy_OnIRQ(TMAGJoy *joy)
{
    if (!joy) return;
    (void)TMAG5273_get_device_status(); // ack
    TMAGJoy_Sample s = TMAGJoy_ReadAnalog(joy);
    if (s.dir == TMAGJOY_NEUTRAL) return;
    uint8_t next = qnext(joy->q_head);
    if (next != joy->q_tail) { joy->q[joy->q_head] = s; joy->q_head = next; }
}

int TMAGJoy_Pop(TMAGJoy *joy, TMAGJoy_Sample *out)
{
    if (!joy || joy->q_head == joy->q_tail) return 0;
    if (out) *out = joy->q[joy->q_tail];
    joy->q_tail = qnext(joy->q_tail);
    return 1;
}

// ---------- Pass-through knobs ----------
void TMAGJoy_SetAbsDeadzone(TMAGJoy *joy, uint8_t en, float mT)
{
    if (joy) {
        joy->cfg.abs_deadzone_en = en ? 1u : 0u;
        joy->cfg.abs_deadzone_mT = mT;
    }
    s_abs_dz_en = en ? 1u : 0u;
    if (mT < 0.0f) mT = 0.0f;
    if (mT > 80.0f) mT = 80.0f;
    s_abs_dz_mT = mT;
}
void TMAGJoy_SetHysteresis(TMAGJoy *joy, uint8_t en, float in_n, float out_n)
{
    (void)joy;
    s_use_hz = en ? 1u : 0u;
    if (en) {
        if (in_n < 0.0f) in_n = 0.0f;
        if (out_n < 0.0f) out_n = 0.0f;
        if (in_n > 0.95f) in_n = 0.95f;
        if (out_n > 0.98f) out_n = 0.98f;
        if (out_n < in_n) out_n = in_n + 0.02f;
        s_dz_in = in_n;
        s_dz_out = out_n;
    }
}
void TMAGJoy_SetInvert(TMAGJoy *joy, uint8_t ix, uint8_t iy)
{
    (void)joy;
    s_cal.invert_x = ix ? 1u : 0u;
    s_cal.invert_y = iy ? 1u : 0u;
    s_digital_latched = TMAGJOY_NEUTRAL;
}
void TMAGJoy_AddExtraRotation(TMAGJoy *joy, float deg)
{
    (void)joy;
    float a = atan2f(s_cal.sphi, s_cal.cphi) + deg * (3.14159265f / 180.0f);
    s_cal.cphi = cosf(a);
    s_cal.sphi = sinf(a);
}
void TMAGJoy_SetDirBias(TMAGJoy *joy, float deg)
{
    (void)joy;
    s_dir_bias_rad = deg * (3.14159265f / 180.0f);
}

void TMAGJoy_SetDeadzoneNorm(TMAGJoy *joy, float dz_norm)
{
    (void)joy;
    // Ensure hardware hysteresis is OFF for proportional analog response
    s_use_hz = 0u;
    // Clamp and apply a simple normalized deadzone (0..~0.9)
    if (dz_norm < 0.0f) dz_norm = 0.0f;
    if (dz_norm > 0.9f) dz_norm = 0.9f;
    s_cal.deadzone = dz_norm;
}


void TMAGJoy_SetRadialDeadzoneNorm(TMAGJoy *joy, float dz_norm)
{
    (void)joy;
    if (dz_norm < 0.0f) dz_norm = 0.0f;
    if (dz_norm > 0.90f) dz_norm = 0.90f;
    sJoyCfg.dz = dz_norm;
}

void TMAGJoy_SetResponseCurve(TMAGJoy *joy, float gamma)
{
    (void)joy;
    if (gamma < 0.2f) gamma = 0.2f;
    if (gamma > 3.0f) gamma = 3.0f;
    sJoyCfg.gamma = gamma;
}

static TMAGJoy_Sample Joy_ReadMenuSample(void)
{
    TMAGJoy_Sample s = { TMAGJOY_NEUTRAL, 0.0f, 0.0f, 0.0f };
    float nx = 0.0f, ny = 0.0f, r_abs = 0.0f;
    TMAGJoy_Dir d = joy_read(&nx, &ny, &r_abs);
    if (d == TMAGJOY_NEUTRAL) return s;
    s.dir = d;
    s.nx = nx;
    s.ny = ny;
    s.r_abs_mT = r_abs;
    return s;
}

void TMAGJoy_SetIntEnabled(uint8_t enable)
{
    if (!s_joy_inited) {
        if (!enable) {
            Joy_DisableExtiLine();
        }
        return;
    }

    uint8_t result_int = 0u;
    uint8_t thr_int = 0u;
    uint8_t mask_intb = 1u;
    if (enable) {
        result_int = (gJoy.cfg.irq_source == TMAGJOY_IRQ_RESULT) ? 1u : 0u;
        thr_int = (gJoy.cfg.irq_source == TMAGJOY_IRQ_THRESHOLD) ? 1u : 0u;
        mask_intb = 0u;
    }

    (void)TMAG5273_config_int(result_int, thr_int, gJoy.cfg.int_pulse_10us,
                              TMAG5273_INT_MODE_INT, mask_intb);
    (void)TMAG5273_get_device_status();
    __HAL_GPIO_EXTI_CLEAR_IT(TMAG5273_INT_Pin);

    if (enable) {
        Joy_ConfigExtiPin(1u);
        SET_BIT(EXTI->IMR1, TMAG5273_INT_Pin);
        SET_BIT(EXTI->EMR1, TMAG5273_INT_Pin);
        HAL_NVIC_EnableIRQ(TMAG5273_INT_EXTI_IRQn);
    } else {
        Joy_DisableExtiLine();
    }
}

static void Joy_DisableExtiLine(void)
{
    HAL_NVIC_DisableIRQ(TMAG5273_INT_EXTI_IRQn);
    CLEAR_BIT(EXTI->IMR1, TMAG5273_INT_Pin);
    CLEAR_BIT(EXTI->EMR1, TMAG5273_INT_Pin);
    __HAL_GPIO_EXTI_CLEAR_IT(TMAG5273_INT_Pin);
    NVIC_ClearPendingIRQ(TMAG5273_INT_EXTI_IRQn);
    Joy_ConfigExtiPin(0u);
}

static void Joy_ConfigExtiPin(uint8_t enable)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = TMAG5273_INT_Pin;
    if (enable) {
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
    } else {
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
    }
    HAL_GPIO_Init(TMAG5273_INT_GPIO_Port, &GPIO_InitStruct);
}

void TMAGJoy_DisableIntLine(void)
{
    Joy_DisableExtiLine();
}

void TMAGJoy_InitOnce(void)
{
    static uint8_t inited = 0u;
    if (inited) return;
    inited = 1u;

    TMAGJoy_Config cfg = TMAGJoy_DefaultConfig();
    cfg.mode = TMAG5273_MODE_CONTINUOUS;
    cfg.irq_source = TMAGJOY_IRQ_THRESHOLD;
    cfg.int_pulse_10us = 1u;
    cfg.thr_x_mT = 6.0f;
    cfg.thr_y_mT = 6.0f;
    (void)TMAGJoy_Init(&gJoy, &cfg);
    s_joy_inited = 1u;
    TMAGJoy_SetIntEnabled(s_joy_use_irq);
}

TMAGJoy *UI_GetJoy(void)
{
    return &gJoy;
}

void TMAGJoy_SetUseIRQ(uint8_t enable)
{
    s_joy_use_irq = enable ? 1u : 0u;
}

void TMAGJoy_OnSleep(void)
{
    s_joy_menu_last = TMAGJOY_NEUTRAL;
    s_joy_wait_neutral = 0u;
    s_joy_neutral_cnt = 0u;
    TMAGJoy_SetIntEnabled(0u);
}

void TMAGJoy_OnWake(void)
{
    TMAGJoy_SetIntEnabled(s_joy_use_irq);
}

void TMAGJoy_MenuEnable(uint8_t on)
{
    s_joy_menu_ready = on ? 1u : 0u;
    s_joy_wait_neutral = on ? 1u : 0u;
    s_joy_menu_last = TMAGJOY_NEUTRAL;
    s_joy_neutral_cnt = 0u;
}

uint8_t TMAGJoy_MenuReady(void)
{
    return s_joy_menu_ready;
}

TMAGJoy_Dir TMAGJoy_MenuIRQ(uint8_t menu_mode, uint8_t in_sleep)
{
    if (!s_joy_use_irq) return TMAGJOY_NEUTRAL;
    (void)TMAG5273_get_device_status();

    if (!s_joy_menu_ready || in_sleep || !menu_mode) {
        s_joy_menu_last = TMAGJOY_NEUTRAL;
        s_joy_wait_neutral = 0u;
        s_joy_neutral_cnt = 0u;
        return TMAGJOY_NEUTRAL;
    }
    if (s_joy_wait_neutral) return TMAGJOY_NEUTRAL;

    TMAGJoy_Sample s = Joy_ReadMenuSample();
    if (s.dir == TMAGJOY_NEUTRAL) {
        s_joy_menu_last = TMAGJOY_NEUTRAL;
        return TMAGJOY_NEUTRAL;
    }
    if (s_joy_menu_last != TMAGJOY_NEUTRAL) return TMAGJOY_NEUTRAL;

    s_joy_menu_last = s.dir;
    s_joy_wait_neutral = 1u;
    s_joy_neutral_cnt = 0u;
    return s.dir;
}

TMAGJoy_Dir TMAGJoy_MenuPoll(uint8_t menu_mode, uint8_t in_sleep)
{
    if (!s_joy_menu_ready || in_sleep || !menu_mode) {
        s_joy_wait_neutral = 0u;
        s_joy_menu_last = TMAGJOY_NEUTRAL;
        s_joy_neutral_cnt = 0u;
        return TMAGJOY_NEUTRAL;
    }

    TMAGJoy_Sample s = Joy_ReadMenuSample();
    const float rN = sqrtf(s.nx * s.nx + s.ny * s.ny);

    if (s_joy_wait_neutral) {
        if (rN <= JOY_MENU_RELEASE_NORM) {
            s_joy_wait_neutral = 0u;
            s_joy_menu_last = TMAGJOY_NEUTRAL;
            s_joy_neutral_cnt = 0u;
        }
        return TMAGJOY_NEUTRAL;
    }

    if (s.dir == TMAGJOY_NEUTRAL || rN < JOY_MENU_PRESS_NORM) return TMAGJOY_NEUTRAL;

    s_joy_menu_last = s.dir;
    s_joy_wait_neutral = 1u;
    s_joy_neutral_cnt = 0u;
    return s.dir;
}

void TMAGJoy_MenuNeutralStep(uint8_t menu_mode, uint8_t in_sleep)
{
    if (!s_joy_menu_ready || in_sleep || !menu_mode) return;
    if (!s_joy_wait_neutral) return;

    TMAGJoy *joy = UI_GetJoy();
    if (!joy) return;
    TMAGJoy_Sample s = TMAGJoy_ReadAnalog(joy);
    float th = (joy->cfg.abs_deadzone_en != 0u) ? joy->cfg.abs_deadzone_mT : 0.0f;
    if (th < 1.0f) th = 2.0f;
    float th_release = th * 1.15f;
    if (th_release < (th + 0.5f)) th_release = th + 0.5f;
    if (s.r_abs_mT <= th_release) {
        if (++s_joy_neutral_cnt >= JOY_NEUTRAL_STABLE_COUNT) {
            s_joy_wait_neutral = 0u;
            s_joy_menu_last = TMAGJOY_NEUTRAL;
            s_joy_neutral_cnt = 0u;
        }
    } else {
        s_joy_neutral_cnt = 0u;
    }
}
