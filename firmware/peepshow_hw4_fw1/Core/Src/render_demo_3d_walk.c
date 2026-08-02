/*
 * render_demo.c (demo variant): Room + Pillar, 4-tone ordered dithering
 *
 * Fixes:
 * - Pillar is OPAQUE (writes both black+white in its fill)
 * - Pillar is NOT see-through internally: backfaces are culled, and
 *   remaining faces are drawn far-to-near.
 *
 * Controls:
 * - Joystick X: turn (yaw only)
 * - Joystick Y: forward/back only (axis-gated so turning doesn't move)
 * - LIS X tilt: strafe
 */

#include "render_demo.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "main.h"
#include "th_mode.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define UI_BAR_H_PIXELS       (14U)
#define EDGE_THICKNESS_PIXELS (2U)

/* Room dimensions */
#define ROOM_HALF_X   (4.0f)
#define ROOM_HALF_Z   (6.0f)
#define ROOM_HEIGHT_Y (2.2f)

/* Pillar dimensions */
#define PILLAR_HALF_X (0.55f)
#define PILLAR_HALF_Z (0.55f)
#define PILLAR_TOP_Y  (2.0f)

typedef struct
{
  float x;
  float y;
  float z;
} vec3_t;

typedef struct
{
  int16_t x;
  int16_t y;
} pt2_t;

typedef struct
{
  uint8_t initialized;
  uint8_t room_enabled;

  uint16_t width;
  uint16_t height;
  uint16_t ui_bar_h;
  uint16_t game_y0;
  uint16_t game_y1;

  float cam_x;
  float cam_y;
  float cam_z;
  float yaw;
  float pitch;

  uint32_t fps;
  uint32_t fps_ms_acc;
  uint32_t fps_frames;
  uint32_t boot_ms;
  uint32_t last_frame_ms;
} render_demo_state_t;

static render_demo_state_t s_demo;

/* ---- Geometry: Room (axis-aligned box) ---- */

static const vec3_t kRoomVerts[8] =
{
  {-ROOM_HALF_X, 0.0f,          -ROOM_HALF_Z}, {+ROOM_HALF_X, 0.0f,          -ROOM_HALF_Z},
  {+ROOM_HALF_X, ROOM_HEIGHT_Y, -ROOM_HALF_Z}, {-ROOM_HALF_X, ROOM_HEIGHT_Y, -ROOM_HALF_Z},
  {-ROOM_HALF_X, 0.0f,          +ROOM_HALF_Z}, {+ROOM_HALF_X, 0.0f,          +ROOM_HALF_Z},
  {+ROOM_HALF_X, ROOM_HEIGHT_Y, +ROOM_HALF_Z}, {-ROOM_HALF_X, ROOM_HEIGHT_Y, +ROOM_HALF_Z}
};

static const uint8_t kRoomEdges[12][2] =
{
  {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U},
  {4U, 5U}, {5U, 6U}, {6U, 7U}, {7U, 4U},
  {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}
};

static const uint8_t kRoomFaces[6][4] =
{
  /* -Z wall */ {0U, 1U, 2U, 3U},
  /* +Z wall */ {4U, 5U, 6U, 7U},
  /* -X wall */ {0U, 3U, 7U, 4U},
  /* +X wall */ {1U, 5U, 6U, 2U},
  /* floor */   {0U, 1U, 5U, 4U},
  /* ceiling */ {3U, 2U, 6U, 7U}
};

/* ---- Geometry: Pillar (axis-aligned box at world origin) ---- */

static const vec3_t kPillarVerts[8] =
{
  {-PILLAR_HALF_X, 0.0f,          -PILLAR_HALF_Z}, {+PILLAR_HALF_X, 0.0f,          -PILLAR_HALF_Z},
  {+PILLAR_HALF_X, PILLAR_TOP_Y,  -PILLAR_HALF_Z}, {-PILLAR_HALF_X, PILLAR_TOP_Y,  -PILLAR_HALF_Z},
  {-PILLAR_HALF_X, 0.0f,          +PILLAR_HALF_Z}, {+PILLAR_HALF_X, 0.0f,          +PILLAR_HALF_Z},
  {+PILLAR_HALF_X, PILLAR_TOP_Y,  +PILLAR_HALF_Z}, {-PILLAR_HALF_X, PILLAR_TOP_Y,  +PILLAR_HALF_Z}
};

static const uint8_t kPillarEdges[12][2] =
{
  {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U},
  {4U, 5U}, {5U, 6U}, {6U, 7U}, {7U, 4U},
  {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}
};

static const uint8_t kPillarFaces[6][4] =
{
  /* -Z */     {0U, 1U, 2U, 3U},
  /* +Z */     {4U, 5U, 6U, 7U},
  /* -X */     {0U, 3U, 7U, 4U},
  /* +X */     {1U, 5U, 6U, 2U},
  /* bottom */ {0U, 1U, 5U, 4U},
  /* top */    {3U, 2U, 6U, 7U}
};

/* ---- Basic helpers ---- */

static int32_t RenderDemo_Round(float x)
{
  return (int32_t)(x >= 0.0f ? (x + 0.5f) : (x - 0.5f));
}

static float RenderDemo_AbsF(float x)
{
  return (x >= 0.0f) ? x : -x;
}

static float RenderDemo_ClampF(float x, float lo, float hi)
{
  if (x < lo)
  {
    return lo;
  }
  if (x > hi)
  {
    return hi;
  }
  return x;
}

static char *RenderDemo_U32ToDec(char *dst, uint32_t v)
{
  char tmp[11];
  int32_t n = 0;

  do
  {
    tmp[n++] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v != 0U);

  while (n-- > 0)
  {
    *dst++ = tmp[n];
  }

  *dst = '\0';
  return dst;
}

static float RenderDemo_LisRawToNorm(int32_t raw)
{
  const float scale = 1.0f / 16000.0f;
  const int32_t deadband = 1200;

  if ((raw > -deadband) && (raw < deadband))
  {
    return 0.0f;
  }

  return RenderDemo_ClampF(((float)raw) * scale, -1.0f, 1.0f);
}

/* ---- Doom-ish movement ---- */

static void RenderDemo_UpdateRoomFromSensor(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms)
{
  const float joy_deadzone = 0.06f;
  const float yaw_rate = 1.7f;
  const float move_speed = 1.10f;
  const float strafe_speed = 0.90f;

  const float eye_h = 1.05f;
  const float player_radius = 0.28f;

  const float dt_s = (dt_ms > 0U) ? ((float)dt_ms * 0.001f) : 0.0f;

  float joy_x = 0.0f;
  float joy_y = 0.0f;
  float strafe = 0.0f;

  if ((sensor_snapshot != NULL) && ((sensor_snapshot->valid_mask & APP_SENSOR_SNAPSHOT_VALID_JOY) != 0UL))
  {
    joy_x = RenderDemo_ClampF(sensor_snapshot->joy_nx, -1.0f, 1.0f);
    joy_y = RenderDemo_ClampF(sensor_snapshot->joy_ny, -1.0f, 1.0f);
  }

  if ((sensor_snapshot != NULL) &&
      ((sensor_snapshot->valid_mask & APP_SENSOR_SNAPSHOT_VALID_LIS) != 0UL) &&
      (sensor_snapshot->lis_sample_count > 0UL))
  {
    strafe = RenderDemo_LisRawToNorm((int32_t)sensor_snapshot->lis_x_raw);
  }

  if (RenderDemo_AbsF(joy_x) < joy_deadzone)
  {
    joy_x = 0.0f;
  }
  if (RenderDemo_AbsF(joy_y) < joy_deadzone)
  {
    joy_y = 0.0f;
  }

  /* Axis gate: when you intend to turn, do not move forward/back. */
  if (RenderDemo_AbsF(joy_x) > RenderDemo_AbsF(joy_y))
  {
    joy_y = 0.0f;
  }

  s_demo.yaw += (joy_x * yaw_rate * dt_s);

  s_demo.pitch = 0.0f;
  s_demo.cam_y = eye_h;

  {
    const float cy = cosf(s_demo.yaw);
    const float sy = sinf(s_demo.yaw);

    const float fwd_x = sy;
    const float fwd_z = cy;

    const float right_x = cy;
    const float right_z = -sy;

    const float dx = (joy_y * move_speed * dt_s * fwd_x) + (strafe * strafe_speed * dt_s * right_x);
    const float dz = (joy_y * move_speed * dt_s * fwd_z) + (strafe * strafe_speed * dt_s * right_z);

    const float min_x = -ROOM_HALF_X + player_radius;
    const float max_x = +ROOM_HALF_X - player_radius;
    const float min_z = -ROOM_HALF_Z + player_radius;
    const float max_z = +ROOM_HALF_Z - player_radius;

    {
      float nx = s_demo.cam_x + dx;
      if (nx < min_x) nx = min_x;
      if (nx > max_x) nx = max_x;
      s_demo.cam_x = nx;
    }

    {
      float nz = s_demo.cam_z + dz;
      if (nz < min_z) nz = min_z;
      if (nz > max_z) nz = max_z;
      s_demo.cam_z = nz;
    }
  }
}

/* ---- View transform + projection ---- */

static void RenderDemo_WorldToView(const vec3_t *w, vec3_t *v)
{
  const float x = w->x - s_demo.cam_x;
  const float y = w->y - s_demo.cam_y;
  const float z = w->z - s_demo.cam_z;

  const float cy = cosf(-s_demo.yaw);
  const float sy = sinf(-s_demo.yaw);
  const float cp = cosf(-s_demo.pitch);
  const float sp = sinf(-s_demo.pitch);

  const float xx = (x * cy) + (z * sy);
  const float zz = (-x * sy) + (z * cy);

  v->x = xx;
  v->y = (y * cp) - (zz * sp);
  v->z = (y * sp) + (zz * cp);
}

static void RenderDemo_Project(const vec3_t *v, pt2_t *p)
{
  const float f = 92.0f;
  const float cx = (float)(s_demo.width / 2U);
  const float cy = (float)((s_demo.game_y0 + s_demo.game_y1) / 2U);

  const float invz = 1.0f / v->z;
  const float px = cx + (f * v->x * invz);
  const float py = cy - (f * v->y * invz);

  p->x = (int16_t)RenderDemo_Round(px);
  p->y = (int16_t)RenderDemo_Round(py);
}

/* ---- Near-plane clipping ---- */

static uint8_t RenderDemo_ClipSegmentNear(vec3_t *a, vec3_t *b, float near_z)
{
  const uint8_t a_in = (a->z >= near_z) ? 1U : 0U;
  const uint8_t b_in = (b->z >= near_z) ? 1U : 0U;

  if ((a_in == 0U) && (b_in == 0U))
  {
    return 0U;
  }

  if (a_in != b_in)
  {
    const float t = (near_z - a->z) / (b->z - a->z);
    vec3_t i;
    i.x = a->x + t * (b->x - a->x);
    i.y = a->y + t * (b->y - a->y);
    i.z = near_z;

    if (a_in == 0U)
    {
      *a = i;
    }
    else
    {
      *b = i;
    }
  }

  return 1U;
}

static uint8_t RenderDemo_ClipTriNear(const vec3_t *in0, const vec3_t *in1, const vec3_t *in2,
                                     float near_z,
                                     vec3_t out0[3],
                                     vec3_t out1[3])
{
  const uint8_t in_a = (in0->z >= near_z) ? 1U : 0U;
  const uint8_t in_b = (in1->z >= near_z) ? 1U : 0U;
  const uint8_t in_c = (in2->z >= near_z) ? 1U : 0U;
  const uint8_t inside = (uint8_t)(in_a + in_b + in_c);

  if (inside == 0U)
  {
    return 0U;
  }
  if (inside == 3U)
  {
    out0[0] = *in0;
    out0[1] = *in1;
    out0[2] = *in2;
    return 1U;
  }

#define INTERSECT(P, Q, OUT) \
  do { \
    const float t = (near_z - (P).z) / ((Q).z - (P).z); \
    (OUT).x = (P).x + t * ((Q).x - (P).x); \
    (OUT).y = (P).y + t * ((Q).y - (P).y); \
    (OUT).z = near_z; \
  } while (0)

  if (inside == 1U)
  {
    vec3_t I0, I1;
    const vec3_t *P;
    const vec3_t *Q;
    const vec3_t *R;

    if (in_a != 0U)
    {
      P = in0;
      Q = in1;
      R = in2;
    }
    else if (in_b != 0U)
    {
      P = in1;
      Q = in2;
      R = in0;
    }
    else
    {
      P = in2;
      Q = in0;
      R = in1;
    }

    INTERSECT(*P, *Q, I0);
    INTERSECT(*P, *R, I1);

    out0[0] = *P;
    out0[1] = I0;
    out0[2] = I1;
    return 1U;
  }

  {
    vec3_t I0, I1;
    const vec3_t *P;
    const vec3_t *Q;
    const vec3_t *R;

    if (in_a == 0U)
    {
      P = in0;
      Q = in1;
      R = in2;
    }
    else if (in_b == 0U)
    {
      P = in1;
      Q = in2;
      R = in0;
    }
    else
    {
      P = in2;
      Q = in0;
      R = in1;
    }

    INTERSECT(*Q, *P, I0);
    INTERSECT(*R, *P, I1);

    out0[0] = *Q;
    out0[1] = *R;
    out0[2] = I1;

    out1[0] = *Q;
    out1[1] = I1;
    out1[2] = I0;

    return 2U;
  }

#undef INTERSECT
}

/* ---- 2D line clipping (Cohen–Sutherland) ---- */

#define CS_LEFT   (1U)
#define CS_RIGHT  (2U)
#define CS_TOP    (4U)
#define CS_BOTTOM (8U)

static uint8_t RenderDemo_ComputeOutCode(int32_t x, int32_t y, int32_t xmin, int32_t ymin, int32_t xmax, int32_t ymax)
{
  uint8_t code = 0U;
  if (x < xmin) code |= CS_LEFT;
  if (x > xmax) code |= CS_RIGHT;
  if (y < ymin) code |= CS_TOP;
  if (y > ymax) code |= CS_BOTTOM;
  return code;
}

static uint8_t RenderDemo_ClipLine2D(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1,
                                    int32_t xmin, int32_t ymin, int32_t xmax, int32_t ymax)
{
  uint8_t out0 = RenderDemo_ComputeOutCode(*x0, *y0, xmin, ymin, xmax, ymax);
  uint8_t out1 = RenderDemo_ComputeOutCode(*x1, *y1, xmin, ymin, xmax, ymax);

  while (1)
  {
    if ((out0 | out1) == 0U)
    {
      return 1U;
    }
    if ((out0 & out1) != 0U)
    {
      return 0U;
    }

    {
      uint8_t out = (out0 != 0U) ? out0 : out1;
      int32_t x = 0;
      int32_t y = 0;
      const int32_t dx = (*x1 - *x0);
      const int32_t dy = (*y1 - *y0);

      if ((out & CS_TOP) != 0U)
      {
        y = ymin;
        x = *x0 + (dx * (y - *y0)) / (dy == 0 ? 1 : dy);
      }
      else if ((out & CS_BOTTOM) != 0U)
      {
        y = ymax;
        x = *x0 + (dx * (y - *y0)) / (dy == 0 ? 1 : dy);
      }
      else if ((out & CS_RIGHT) != 0U)
      {
        x = xmax;
        y = *y0 + (dy * (x - *x0)) / (dx == 0 ? 1 : dx);
      }
      else
      {
        x = xmin;
        y = *y0 + (dy * (x - *x0)) / (dx == 0 ? 1 : dx);
      }

      if (out == out0)
      {
        *x0 = x;
        *y0 = y;
        out0 = RenderDemo_ComputeOutCode(*x0, *y0, xmin, ymin, xmax, ymax);
      }
      else
      {
        *x1 = x;
        *y1 = y;
        out1 = RenderDemo_ComputeOutCode(*x1, *y1, xmin, ymin, xmax, ymax);
      }
    }
  }
}

/* ---- 4-tone ordered dithering (2x2 Bayer) ---- */

static uint8_t RenderDemo_Bayer2x2(uint16_t x, uint16_t y)
{
  const uint8_t ix = (uint8_t)(x & 1U);
  const uint8_t iy = (uint8_t)(y & 1U);

  if (iy == 0U)
  {
    return (ix == 0U) ? 0U : 2U;
  }
  return (ix == 0U) ? 3U : 1U;
}

static void RenderDemo_FillSpanDitheredInk(uint16_t y, int32_t x0, int32_t x1, uint8_t level)
{
  int32_t x;
  int32_t run_start = -1;

  if (x0 > x1)
  {
    const int32_t t = x0;
    x0 = x1;
    x1 = t;
  }

  if ((x1 < 0) || (x0 >= (int32_t)s_demo.width))
  {
    return;
  }

  if (x0 < 0) x0 = 0;
  if (x1 >= (int32_t)s_demo.width) x1 = (int32_t)s_demo.width - 1;

  for (x = x0; x <= x1; ++x)
  {
    const uint8_t thr = RenderDemo_Bayer2x2((uint16_t)x, y);
    const uint8_t ink = (level > thr) ? 1U : 0U;

    if (ink != 0U)
    {
      if (run_start < 0)
      {
        run_start = x;
      }
    }
    else
    {
      if (run_start >= 0)
      {
        renderFillRect((uint16_t)run_start, y, (uint16_t)(x - run_start), 1U,
                       RENDER_LAYER_GAME, RENDER_COLOR_BLACK);
        run_start = -1;
      }
    }
  }

  if (run_start >= 0)
  {
    renderFillRect((uint16_t)run_start, y, (uint16_t)((x1 + 1) - run_start), 1U,
                   RENDER_LAYER_GAME, RENDER_COLOR_BLACK);
  }
}

static void RenderDemo_FillSpanDitheredOpaque(uint16_t y, int32_t x0, int32_t x1, uint8_t level)
{
  int32_t x;
  int32_t run_start;
  uint8_t run_is_black;

  if (x0 > x1)
  {
    const int32_t t = x0;
    x0 = x1;
    x1 = t;
  }

  if ((x1 < 0) || (x0 >= (int32_t)s_demo.width))
  {
    return;
  }

  if (x0 < 0) x0 = 0;
  if (x1 >= (int32_t)s_demo.width) x1 = (int32_t)s_demo.width - 1;

  {
    const uint8_t thr0 = RenderDemo_Bayer2x2((uint16_t)x0, y);
    run_is_black = (level > thr0) ? 1U : 0U;
    run_start = x0;
  }

  for (x = x0 + 1; x <= x1; ++x)
  {
    const uint8_t thr = RenderDemo_Bayer2x2((uint16_t)x, y);
    const uint8_t is_black = (level > thr) ? 1U : 0U;

    if (is_black != run_is_black)
    {
      renderFillRect((uint16_t)run_start, y, (uint16_t)(x - run_start), 1U,
                     RENDER_LAYER_GAME, run_is_black ? RENDER_COLOR_BLACK : RENDER_COLOR_WHITE);
      run_start = x;
      run_is_black = is_black;
    }
  }

  renderFillRect((uint16_t)run_start, y, (uint16_t)((x1 + 1) - run_start), 1U,
                 RENDER_LAYER_GAME, run_is_black ? RENDER_COLOR_BLACK : RENDER_COLOR_WHITE);
}

static void RenderDemo_FillTri_Impl(const pt2_t *p0, const pt2_t *p1, const pt2_t *p2,
                                   uint8_t level,
                                   void (*span_fn)(uint16_t y, int32_t x0, int32_t x1, uint8_t level))
{
  pt2_t v0 = *p0;
  pt2_t v1 = *p1;
  pt2_t v2 = *p2;

  if (v1.y < v0.y)
  {
    pt2_t t = v0;
    v0 = v1;
    v1 = t;
  }
  if (v2.y < v0.y)
  {
    pt2_t t = v0;
    v0 = v2;
    v2 = t;
  }
  if (v2.y < v1.y)
  {
    pt2_t t = v1;
    v1 = v2;
    v2 = t;
  }

  const int32_t y0 = (int32_t)v0.y;
  const int32_t y1 = (int32_t)v1.y;
  const int32_t y2 = (int32_t)v2.y;

  if (y2 == y0)
  {
    return;
  }

#define FP_SHIFT (16)
#define FP_ONE   (1 << FP_SHIFT)

  const int32_t dy02 = (y2 - y0);
  const int32_t dx02 = ((int32_t)v2.x - (int32_t)v0.x);
  const int32_t step02 = (dx02 * FP_ONE) / (dy02 == 0 ? 1 : dy02);

  int32_t x02 = ((int32_t)v0.x * FP_ONE);

  if (y1 > y0)
  {
    const int32_t dy01 = (y1 - y0);
    const int32_t dx01 = ((int32_t)v1.x - (int32_t)v0.x);
    const int32_t step01 = (dx01 * FP_ONE) / (dy01 == 0 ? 1 : dy01);

    int32_t x01 = ((int32_t)v0.x * FP_ONE);

    int32_t y;
    for (y = y0; y < y1; ++y)
    {
      if ((y >= (int32_t)s_demo.game_y0) && (y <= (int32_t)s_demo.game_y1))
      {
        const int32_t xa = x01 >> FP_SHIFT;
        const int32_t xb = x02 >> FP_SHIFT;
        span_fn((uint16_t)y, xa, xb, level);
      }

      x01 += step01;
      x02 += step02;
    }
  }

  if (y2 > y1)
  {
    const int32_t dy12 = (y2 - y1);
    const int32_t dx12 = ((int32_t)v2.x - (int32_t)v1.x);
    const int32_t step12 = (dx12 * FP_ONE) / (dy12 == 0 ? 1 : dy12);

    int32_t x12 = ((int32_t)v1.x * FP_ONE);

    int32_t y;
    for (y = y1; y <= y2; ++y)
    {
      if ((y >= (int32_t)s_demo.game_y0) && (y <= (int32_t)s_demo.game_y1))
      {
        const int32_t xa = x12 >> FP_SHIFT;
        const int32_t xb = x02 >> FP_SHIFT;
        span_fn((uint16_t)y, xa, xb, level);
      }

      x12 += step12;
      x02 += step02;
    }
  }

#undef FP_SHIFT
#undef FP_ONE
}

static void RenderDemo_FillTriDitheredInk(const pt2_t *p0, const pt2_t *p1, const pt2_t *p2, uint8_t level)
{
  RenderDemo_FillTri_Impl(p0, p1, p2, level, RenderDemo_FillSpanDitheredInk);
}

static void RenderDemo_FillTriDitheredOpaque(const pt2_t *p0, const pt2_t *p1, const pt2_t *p2, uint8_t level)
{
  RenderDemo_FillTri_Impl(p0, p1, p2, level, RenderDemo_FillSpanDitheredOpaque);
}

/* ---- Shading ---- */

static uint8_t RenderDemo_ShadeRoomFace(uint8_t face_id)
{
  switch (face_id)
  {
    case 4U: return 2U;
    case 5U: return 1U;
    case 2U: return 3U;
    default: return 2U;
  }
}

static uint8_t RenderDemo_ShadePillarFace(uint8_t face_id)
{
  switch (face_id)
  {
    case 5U: return 2U;
    default: return 3U;
  }
}

/* ---- Vector ops for culling ---- */

static vec3_t RenderDemo_Sub(const vec3_t *a, const vec3_t *b)
{
  vec3_t r;
  r.x = a->x - b->x;
  r.y = a->y - b->y;
  r.z = a->z - b->z;
  return r;
}

static vec3_t RenderDemo_Cross(const vec3_t *a, const vec3_t *b)
{
  vec3_t r;
  r.x = (a->y * b->z) - (a->z * b->y);
  r.y = (a->z * b->x) - (a->x * b->z);
  r.z = (a->x * b->y) - (a->y * b->x);
  return r;
}

/* ---- Box drawing ---- */

static void RenderDemo_DrawBoxWireframe(const vec3_t *verts, const uint8_t edges[12][2])
{
  const float near_z = 0.18f;
  vec3_t v_view[8];
  uint8_t i;

  for (i = 0U; i < 8U; ++i)
  {
    RenderDemo_WorldToView(&verts[i], &v_view[i]);
  }

  for (i = 0U; i < 12U; ++i)
  {
    vec3_t a = v_view[edges[i][0U]];
    vec3_t b = v_view[edges[i][1U]];

    if (RenderDemo_ClipSegmentNear(&a, &b, near_z) == 0U)
    {
      continue;
    }

    {
      pt2_t pa;
      pt2_t pb;
      int32_t x0;
      int32_t y0;
      int32_t x1;
      int32_t y1;

      RenderDemo_Project(&a, &pa);
      RenderDemo_Project(&b, &pb);

      x0 = (int32_t)pa.x;
      y0 = (int32_t)pa.y;
      x1 = (int32_t)pb.x;
      y1 = (int32_t)pb.y;

      if (RenderDemo_ClipLine2D(&x0, &y0, &x1, &y1,
                               0, (int32_t)s_demo.game_y0,
                               (int32_t)s_demo.width - 1, (int32_t)s_demo.game_y1) != 0U)
      {
        renderDrawLine((uint16_t)x0, (uint16_t)y0,
                       (uint16_t)x1, (uint16_t)y1,
                       RENDER_LAYER_GAME, RENDER_COLOR_BLACK, EDGE_THICKNESS_PIXELS);
      }
    }
  }
}

static void RenderDemo_DrawRoomShadedInk(void)
{
  /* Simple draw: all faces, ink-only; outlines do most of the readability work. */
  const float near_z = 0.18f;
  vec3_t v_view[8];
  uint8_t i;

  for (i = 0U; i < 8U; ++i)
  {
    RenderDemo_WorldToView(&kRoomVerts[i], &v_view[i]);
  }

  for (i = 0U; i < 6U; ++i)
  {
    const uint8_t i0 = kRoomFaces[i][0];
    const uint8_t i1 = kRoomFaces[i][1];
    const uint8_t i2 = kRoomFaces[i][2];
    const uint8_t i3 = kRoomFaces[i][3];

    const vec3_t *A = &v_view[i0];
    const vec3_t *B = &v_view[i1];
    const vec3_t *C = &v_view[i2];
    const vec3_t *D = &v_view[i3];

    const uint8_t shade = RenderDemo_ShadeRoomFace(i);

    {
      vec3_t t0[3];
      vec3_t t1[3];
      uint8_t n;

      n = RenderDemo_ClipTriNear(A, B, C, near_z, t0, t1);
      if (n >= 1U)
      {
        pt2_t p0, p1, p2;
        RenderDemo_Project(&t0[0], &p0);
        RenderDemo_Project(&t0[1], &p1);
        RenderDemo_Project(&t0[2], &p2);
        RenderDemo_FillTriDitheredInk(&p0, &p1, &p2, shade);
      }
      if (n == 2U)
      {
        pt2_t p0, p1, p2;
        RenderDemo_Project(&t1[0], &p0);
        RenderDemo_Project(&t1[1], &p1);
        RenderDemo_Project(&t1[2], &p2);
        RenderDemo_FillTriDitheredInk(&p0, &p1, &p2, shade);
      }

      n = RenderDemo_ClipTriNear(A, C, D, near_z, t0, t1);
      if (n >= 1U)
      {
        pt2_t p0, p1, p2;
        RenderDemo_Project(&t0[0], &p0);
        RenderDemo_Project(&t0[1], &p1);
        RenderDemo_Project(&t0[2], &p2);
        RenderDemo_FillTriDitheredInk(&p0, &p1, &p2, shade);
      }
      if (n == 2U)
      {
        pt2_t p0, p1, p2;
        RenderDemo_Project(&t1[0], &p0);
        RenderDemo_Project(&t1[1], &p1);
        RenderDemo_Project(&t1[2], &p2);
        RenderDemo_FillTriDitheredInk(&p0, &p1, &p2, shade);
      }
    }
  }
}

typedef struct
{
  uint8_t face_id;
  float avg_z;
} face_sort_t;

static void RenderDemo_ComputePillarFaceFront(const vec3_t v_view[8], uint8_t front_out[6])
{
  /*
   * IMPORTANT:
   * Don't infer face orientation from vertex winding.
   * Any inconsistent winding across faces will cause silhouette edges to flicker.
   *
   * Instead, classify front-facing using the known axis-aligned normals for the
   * pillar faces, rotated into view space with the same rotation as WorldToView.
   */
  uint8_t i;

  /* World-space outward normals matching kPillarFaces ordering. */
  static const vec3_t kFaceN[6] =
  {
    { 0.0f,  0.0f, -1.0f}, /* -Z */
    { 0.0f,  0.0f, +1.0f}, /* +Z */
    {-1.0f,  0.0f,  0.0f}, /* -X */
    {+1.0f,  0.0f,  0.0f}, /* +X */
    { 0.0f, -1.0f,  0.0f}, /* bottom (-Y) */
    { 0.0f, +1.0f,  0.0f}, /* top (+Y) */
  };

  (void)v_view;

  for (i = 0U; i < 6U; ++i)
  {
    const vec3_t n_w = kFaceN[i];

    /* Rotate world direction into view space (no translation). */
    const float cy = cosf(-s_demo.yaw);
    const float sy = sinf(-s_demo.yaw);
    const float cp = cosf(-s_demo.pitch);
    const float sp = sinf(-s_demo.pitch);

    const float xx = (n_w.x * cy) + (n_w.z * sy);
    const float zz = (-n_w.x * sy) + (n_w.z * cy);

    const float nz = (n_w.y * sp) + (zz * cp);

    /* View looks along +Z; faces whose normal points toward camera have nz < 0. */
    front_out[i] = (nz < 0.0f) ? 1U : 0U;

    (void)xx;
  }
}

static void RenderDemo_DrawPillarShadedOpaque_SortedAll(void)
{
  const float near_z = 0.18f;
  vec3_t v_view[8];
  face_sort_t faces[6];
  uint8_t i;

  for (i = 0U; i < 8U; ++i)
  {
    RenderDemo_WorldToView(&kPillarVerts[i], &v_view[i]);
  }

  /* Sort ALL faces far-to-near so near faces overwrite far faces (opaque fill). */
  for (i = 0U; i < 6U; ++i)
  {
    const uint8_t i0 = kPillarFaces[i][0];
    const uint8_t i1 = kPillarFaces[i][1];
    const uint8_t i2 = kPillarFaces[i][2];
    const uint8_t i3 = kPillarFaces[i][3];

    const vec3_t *A = &v_view[i0];
    const vec3_t *B = &v_view[i1];
    const vec3_t *C = &v_view[i2];
    const vec3_t *D = &v_view[i3];

    const float avgz = 0.25f * (A->z + B->z + C->z + D->z);
    faces[i].face_id = i;
    faces[i].avg_z = avgz;
  }

  for (i = 0U; i < 6U; ++i)
  {
    uint8_t j;
    uint8_t best = i;
    for (j = (uint8_t)(i + 1U); j < 6U; ++j)
    {
      if (faces[j].avg_z > faces[best].avg_z)
      {
        best = j;
      }
    }
    if (best != i)
    {
      face_sort_t t = faces[i];
      faces[i] = faces[best];
      faces[best] = t;
    }
  }

  for (i = 0U; i < 6U; ++i)
  {
    const uint8_t f = faces[i].face_id;
    const uint8_t i0 = kPillarFaces[f][0];
    const uint8_t i1 = kPillarFaces[f][1];
    const uint8_t i2 = kPillarFaces[f][2];
    const uint8_t i3 = kPillarFaces[f][3];

    const vec3_t *A = &v_view[i0];
    const vec3_t *B = &v_view[i1];
    const vec3_t *C = &v_view[i2];
    const vec3_t *D = &v_view[i3];

    const uint8_t shade = RenderDemo_ShadePillarFace(f);

    {
      vec3_t t0[3];
      vec3_t t1[3];
      uint8_t n;

      n = RenderDemo_ClipTriNear(A, B, C, near_z, t0, t1);
      if (n >= 1U)
      {
        pt2_t p0, p1, p2;
        RenderDemo_Project(&t0[0], &p0);
        RenderDemo_Project(&t0[1], &p1);
        RenderDemo_Project(&t0[2], &p2);
        RenderDemo_FillTriDitheredOpaque(&p0, &p1, &p2, shade);
      }
      if (n == 2U)
      {
        pt2_t p0, p1, p2;
        RenderDemo_Project(&t1[0], &p0);
        RenderDemo_Project(&t1[1], &p1);
        RenderDemo_Project(&t1[2], &p2);
        RenderDemo_FillTriDitheredOpaque(&p0, &p1, &p2, shade);
      }

      n = RenderDemo_ClipTriNear(A, C, D, near_z, t0, t1);
      if (n >= 1U)
      {
        pt2_t p0, p1, p2;
        RenderDemo_Project(&t0[0], &p0);
        RenderDemo_Project(&t0[1], &p1);
        RenderDemo_Project(&t0[2], &p2);
        RenderDemo_FillTriDitheredOpaque(&p0, &p1, &p2, shade);
      }
      if (n == 2U)
      {
        pt2_t p0, p1, p2;
        RenderDemo_Project(&t1[0], &p0);
        RenderDemo_Project(&t1[1], &p1);
        RenderDemo_Project(&t1[2], &p2);
        RenderDemo_FillTriDitheredOpaque(&p0, &p1, &p2, shade);
      }
    }
  }
}

static void RenderDemo_DrawPillarSilhouetteOutline(void)
{
  /* Draw only silhouette edges so you don't see the back edges. */
  const float near_z = 0.18f;
  vec3_t v_view[8];
  uint8_t front[6];
  uint8_t i;

  /* Each edge is shared by 2 faces (for a box). Face indices correspond to kPillarFaces ordering. */
  static const uint8_t kEdgeFaces[12][2] =
  {
    {0U, 4U}, /* 0-1 : -Z & bottom */
    {0U, 3U}, /* 1-2 : -Z & +X */
    {0U, 5U}, /* 2-3 : -Z & top */
    {0U, 2U}, /* 3-0 : -Z & -X */

    {1U, 4U}, /* 4-5 : +Z & bottom */
    {1U, 3U}, /* 5-6 : +Z & +X */
    {1U, 5U}, /* 6-7 : +Z & top */
    {1U, 2U}, /* 7-4 : +Z & -X */

    {2U, 4U}, /* 0-4 : -X & bottom */
    {3U, 4U}, /* 1-5 : +X & bottom */
    {3U, 5U}, /* 2-6 : +X & top */
    {2U, 5U}, /* 3-7 : -X & top */
  };

  for (i = 0U; i < 8U; ++i)
  {
    RenderDemo_WorldToView(&kPillarVerts[i], &v_view[i]);
  }

  RenderDemo_ComputePillarFaceFront(v_view, front);

  for (i = 0U; i < 12U; ++i)
  {
    const uint8_t f0 = kEdgeFaces[i][0];
    const uint8_t f1 = kEdgeFaces[i][1];

    /* silhouette edge = one adjacent face front-facing, the other not */
    if (front[f0] == front[f1])
    {
      continue;
    }

    vec3_t a = v_view[kPillarEdges[i][0U]];
    vec3_t b = v_view[kPillarEdges[i][1U]];

    if (RenderDemo_ClipSegmentNear(&a, &b, near_z) == 0U)
    {
      continue;
    }

    {
      pt2_t pa;
      pt2_t pb;
      int32_t x0, y0, x1, y1;

      RenderDemo_Project(&a, &pa);
      RenderDemo_Project(&b, &pb);

      x0 = (int32_t)pa.x;
      y0 = (int32_t)pa.y;
      x1 = (int32_t)pb.x;
      y1 = (int32_t)pb.y;

      if (RenderDemo_ClipLine2D(&x0, &y0, &x1, &y1,
                               0, (int32_t)s_demo.game_y0,
                               (int32_t)s_demo.width - 1, (int32_t)s_demo.game_y1) != 0U)
      {
        renderDrawLine((uint16_t)x0, (uint16_t)y0,
                       (uint16_t)x1, (uint16_t)y1,
                       RENDER_LAYER_GAME, RENDER_COLOR_BLACK, EDGE_THICKNESS_PIXELS);
      }
    }
  }
}

/* ---- UI ---- */

static void RenderDemo_DrawTopBar(void)
{
  char fps_buf[24];
  char *p = fps_buf;
  uint16_t fps_x = 2U;

  renderFillRect(0U, 0U, s_demo.width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText(4U, 3U, "3D ROOM+PILLAR", RENDER_LAYER_UI, RENDER_COLOR_WHITE);

  *p++ = 'F';
  *p++ = 'P';
  *p++ = 'S';
  *p++ = ':';
  *p++ = ' ';
  p = RenderDemo_U32ToDec(p, s_demo.fps);
  (void)p;

  {
    uint16_t fps_w = 0U;
    const char *s = fps_buf;
    while (*s != '\0')
    {
      fps_w = (uint16_t)(fps_w + FONT8X8_WIDTH + 1U);
      s++;
    }
    if (fps_w > 0U)
    {
      fps_w = (uint16_t)(fps_w - 1U);
    }
    if (s_demo.width > (uint16_t)(fps_w + 2U))
    {
      fps_x = (uint16_t)(s_demo.width - fps_w - 2U);
    }
  }

  renderDrawText(fps_x, 3U, fps_buf, RENDER_LAYER_UI, RENDER_COLOR_WHITE);
}

static void RenderDemo_DrawBottomBar(void)
{
  char up_buf[24];
  char *p = up_buf;
  uint32_t uptime_sec = 0U;
  uint16_t y0;

  if (s_demo.last_frame_ms >= s_demo.boot_ms)
  {
    uptime_sec = (s_demo.last_frame_ms - s_demo.boot_ms) / 1000U;
  }

  y0 = (uint16_t)(s_demo.height - s_demo.ui_bar_h);
  renderFillRect(0U, y0, s_demo.width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  *p++ = 'U';
  *p++ = 'P';
  *p++ = ':';
  *p++ = ' ';
  p = RenderDemo_U32ToDec(p, uptime_sec);
  *p++ = 's';
  *p = '\0';

  renderDrawText(4U, (uint16_t)(y0 + 3U), up_buf, RENDER_LAYER_UI, RENDER_COLOR_WHITE);
  renderDrawText((uint16_t)(s_demo.width > 138U ? (s_demo.width - 138U) : 2U),
                 (uint16_t)(y0 + 3U),
                 "JOY:X=TURN Y=FWD/BACK  LIS:X=STRAFE", RENDER_LAYER_UI, RENDER_COLOR_WHITE);
}

/* ---- Public API ---- */

void RenderDemo_Reset(void)
{
  (void)memset(&s_demo, 0, sizeof(s_demo));
}

void RenderDemo_ToggleBackground(void)
{
  /* API compat: unused */
}

void RenderDemo_ToggleCube(void)
{
  if (s_demo.initialized == 0U)
  {
    return;
  }
  s_demo.room_enabled = (s_demo.room_enabled == 0U) ? 1U : 0U;
}

void RenderDemo_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot)
{
  uint32_t now_ms;
  uint32_t dt_ms;

  if (s_demo.initialized == 0U)
  {
    now_ms = HAL_GetTick();

    s_demo.width = RENDER_WIDTH;
    s_demo.height = RENDER_HEIGHT;
    s_demo.ui_bar_h = (s_demo.height > ((UI_BAR_H_PIXELS * 2U) + 1U)) ? UI_BAR_H_PIXELS : 0U;
    s_demo.game_y0 = s_demo.ui_bar_h;
    s_demo.game_y1 = (s_demo.height > s_demo.ui_bar_h) ? (uint16_t)(s_demo.height - s_demo.ui_bar_h - 1U) : 0U;

    if (s_demo.game_y1 < s_demo.game_y0)
    {
      s_demo.game_y0 = 0U;
      s_demo.game_y1 = (s_demo.height > 0U) ? (uint16_t)(s_demo.height - 1U) : 0U;
    }

    s_demo.room_enabled = 1U;

    /* Start near back wall so the room is in front at yaw=0 */
    s_demo.cam_x = 0.0f;
    s_demo.cam_y = 1.05f;
    s_demo.cam_z = (-ROOM_HALF_Z + 1.2f);
    s_demo.yaw = 0.0f;
    s_demo.pitch = 0.0f;

    s_demo.boot_ms = now_ms;
    s_demo.last_frame_ms = now_ms;
    s_demo.initialized = 1U;
  }

  now_ms = HAL_GetTick();
  dt_ms = (uint32_t)(now_ms - s_demo.last_frame_ms);
  s_demo.last_frame_ms = now_ms;

  s_demo.fps_ms_acc += dt_ms;
  s_demo.fps_frames++;
  if (s_demo.fps_ms_acc >= 1000U)
  {
    if (s_demo.fps_ms_acc > 0U)
    {
      s_demo.fps = (uint32_t)((s_demo.fps_frames * 1000U) / s_demo.fps_ms_acc);
    }
    s_demo.fps_ms_acc = 0U;
    s_demo.fps_frames = 0U;
  }

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(TH_MODE_REALTIME);

  if (s_demo.room_enabled != 0U)
  {
    RenderDemo_UpdateRoomFromSensor(sensor_snapshot, dt_ms);

    /* Room first */
    RenderDemo_DrawRoomShadedInk();
    RenderDemo_DrawBoxWireframe(kRoomVerts, kRoomEdges);

    /* Pillar: sorted opaque fill + silhouette outline */
    RenderDemo_DrawPillarShadedOpaque_SortedAll();
    RenderDemo_DrawPillarSilhouetteOutline();
  }

  if (s_demo.ui_bar_h > 0U)
  {
    RenderDemo_DrawTopBar();
    RenderDemo_DrawBottomBar();
  }
  else
  {
    renderDrawText(2U, 2U, "3D ROOM+PILLAR", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  Render_MarkDirtyAll();
}
