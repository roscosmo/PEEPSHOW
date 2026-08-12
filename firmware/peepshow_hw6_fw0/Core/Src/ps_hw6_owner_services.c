#include "ps_hw6_owner_services.h"

#include <string.h>

#include "ps_dev_audio.h"
#include "ps_dev_ls013b7dh05.h"
#include "main.h"
#include "knobs_autogen.h"
#include "ps_ui_router.h"
#include "ps_dev_adp5360.h"
#include "ps_hw_i2c3.h"

#define PS_HW6_OWNER_PHASE_INIT             (0x6700UL)
#define PS_HW6_OWNER_PHASE_POWER            (0x6701UL)
#define PS_HW6_OWNER_PHASE_DISPLAY          (0x6702UL)
#define PS_HW6_OWNER_PHASE_AUDIO            (0x6703UL)
#define PS_HW6_OWNER_PHASE_COMPLETE         (0x67FFUL)

#define PS_HW6_PMIC_ADDRESS_7BIT            (0x46U)

#define PS_HW6_DISPLAY_PATTERN_ID           (0x54455354UL)
#define PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS   (250U)
#define PS_HW6_DISPLAY_UI_PATTERN_ID         (0x55495047UL)
#define PS_HW6_DISPLAY_TEXT_SCALE            (2U)
#define PS_HW6_DISPLAY_UI_LOGICAL_WIDTH      PS_DEV_LS013B7DH05_HEIGHT
#define PS_HW6_DISPLAY_UI_LOGICAL_HEIGHT     PS_DEV_LS013B7DH05_WIDTH

#define PS_HW6_AUDIO_SAMPLE_RATE_HZ         (16000UL)
#define PS_HW6_AUDIO_TONE_HZ                (1000UL)
#define PS_HW6_AUDIO_DURATION_MS            (750UL)
#define PS_HW6_AUDIO_AMPLITUDE              (3000)
#define PS_HW6_AUDIO_BUFFER_FRAMES          (1024U)
#define PS_HW6_AUDIO_BUFFER_HALFWORDS       (PS_HW6_AUDIO_BUFFER_FRAMES * 2U)
#define PS_HW6_AUDIO_AMP_SETTLE_TICKS       (2UL)
#define PS_HW6_AUDIO_DURATION_TICKS \
  ((PS_HW6_AUDIO_DURATION_MS * TX_TIMER_TICKS_PER_SECOND + 999UL) / 1000UL)

extern I2C_HandleTypeDef hi2c3;
extern RTC_HandleTypeDef hrtc;
extern SAI_HandleTypeDef hsai_BlockA1;
extern DMA_HandleTypeDef handle_GPDMA1_Channel3;
extern SPI_HandleTypeDef hspi3;
extern DMA_HandleTypeDef handle_LPDMA1_Channel0;

volatile PS_HW6_OwnerProbe g_ps_hw6_owner_probe;

static ps_dev_adp5360_t ps_hw6_pmic;
static ps_dev_audio_t ps_hw6_audio;
static ps_dev_ls013b7dh05_t ps_hw6_display;
static uint8_t ps_hw6_display_framebuffer[PS_DEV_LS013B7DH05_BUFFER_SIZE];
static uint32_t ps_hw6_display_ui_rotate_ccw;
static int16_t ps_hw6_audio_buffer[PS_HW6_AUDIO_BUFFER_HALFWORDS]
  __attribute__((aligned(4)));

static const int16_t ps_hw6_sine_16[16] =
{
  0, 1148, 2121, 2772, 3000, 2772, 2121, 1148,
  0, -1148, -2121, -2772, -3000, -2772, -2121, -1148
};

static void PS_HW6_UpdateDisplayDriverProbe(void)
{
  g_ps_hw6_owner_probe.display_driver_api_version =
    ps_hw6_display.api_version;
  g_ps_hw6_owner_probe.display_driver_state = ps_hw6_display.state;
  g_ps_hw6_owner_probe.display_driver_operation_count =
    ps_hw6_display.operation_count;
  g_ps_hw6_owner_probe.display_driver_last_status =
    ps_hw6_display.last_status;
}

static void PS_HW6_UpdateAudioDriverProbe(void)
{
  g_ps_hw6_owner_probe.audio_driver_api_version = ps_hw6_audio.api_version;
  g_ps_hw6_owner_probe.audio_driver_state = ps_hw6_audio.state;
  g_ps_hw6_owner_probe.audio_driver_operation_count =
    ps_hw6_audio.operation_count;
  g_ps_hw6_owner_probe.audio_driver_last_status = ps_hw6_audio.last_status;
}

static uint32_t PS_HW6_DisplaySetBlack(uint16_t x, uint16_t y)
{
  uint16_t panel_x = x;
  uint16_t panel_y = y;
  uint32_t index;
  uint8_t mask;

  if (ps_hw6_display_ui_rotate_ccw != 0UL)
  {
    if ((x >= PS_HW6_DISPLAY_UI_LOGICAL_WIDTH) ||
        (y >= PS_HW6_DISPLAY_UI_LOGICAL_HEIGHT))
    {
      return 0UL;
    }
    panel_x = y;
    panel_y = (uint16_t)(PS_HW6_DISPLAY_UI_LOGICAL_WIDTH - 1U - x);
  }

  if ((panel_x >= PS_DEV_LS013B7DH05_WIDTH) ||
      (panel_y >= PS_DEV_LS013B7DH05_HEIGHT))
  {
    return 0UL;
  }

  index = ((uint32_t)panel_y * PS_DEV_LS013B7DH05_LINE_WIDTH) +
          ((uint32_t)panel_x >> 3U);
  mask = (uint8_t)(1U << (panel_x & 7U));
  if ((ps_hw6_display_framebuffer[index] & mask) == 0U)
  {
    return 0UL;
  }

  ps_hw6_display_framebuffer[index] &= (uint8_t)~mask;
  return 1UL;
}
static uint32_t PS_HW6_DisplayHorizontalLine(uint16_t x0,
                                             uint16_t x1,
                                             uint16_t y)
{
  uint32_t count = 0UL;
  uint16_t x;

  for (x = x0; x <= x1; ++x)
  {
    count += PS_HW6_DisplaySetBlack(x, y);
  }
  return count;
}

static uint32_t PS_HW6_DisplayVerticalLine(uint16_t x,
                                           uint16_t y0,
                                           uint16_t y1)
{
  uint32_t count = 0UL;
  uint16_t y;

  for (y = y0; y <= y1; ++y)
  {
    count += PS_HW6_DisplaySetBlack(x, y);
  }
  return count;
}

static uint32_t PS_HW6_DisplayFilledRect(uint16_t x0,
                                         uint16_t y0,
                                         uint16_t width,
                                         uint16_t height)
{
  uint32_t count = 0UL;
  uint16_t y;

  for (y = y0; y < (uint16_t)(y0 + height); ++y)
  {
    count += PS_HW6_DisplayHorizontalLine(x0,
                                          (uint16_t)(x0 + width - 1U),
                                          y);
  }
  return count;
}

static uint32_t PS_HW6_DisplayFramebufferHash(void)
{
  uint32_t hash = 2166136261UL;
  uint16_t i;

  for (i = 0U; i < PS_DEV_LS013B7DH05_BUFFER_SIZE; ++i)
  {
    hash ^= ps_hw6_display_framebuffer[i];
    hash *= 16777619UL;
  }
  return hash;
}

static uint32_t PS_HW6_DisplayGlyphRows(char glyph, uint8_t rows[7])
{
  static const uint8_t blank[7] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  static const uint8_t glyphs[36][7] =
  {
    {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U},
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x11U, 0x1EU},
    {0x0EU, 0x11U, 0x10U, 0x10U, 0x10U, 0x11U, 0x0EU},
    {0x1EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x1EU},
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x1FU},
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x10U},
    {0x0EU, 0x11U, 0x10U, 0x17U, 0x11U, 0x11U, 0x0FU},
    {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U},
    {0x0EU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU},
    {0x01U, 0x01U, 0x01U, 0x01U, 0x11U, 0x11U, 0x0EU},
    {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x11U},
    {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU},
    {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x11U},
    {0x11U, 0x19U, 0x15U, 0x13U, 0x11U, 0x11U, 0x11U},
    {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU},
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x10U},
    {0x0EU, 0x11U, 0x11U, 0x11U, 0x15U, 0x12U, 0x0DU},
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x11U},
    {0x0FU, 0x10U, 0x10U, 0x0EU, 0x01U, 0x01U, 0x1EU},
    {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U},
    {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU},
    {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0AU, 0x04U},
    {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x15U, 0x0AU},
    {0x11U, 0x11U, 0x0AU, 0x04U, 0x0AU, 0x11U, 0x11U},
    {0x11U, 0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x04U},
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x1FU},
    {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU},
    {0x04U, 0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU},
    {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU},
    {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU},
    {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U},
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x01U, 0x01U, 0x1EU},
    {0x07U, 0x08U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU},
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U},
    {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU},
    {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x02U, 0x1CU}
  };
  const uint8_t *src = blank;
  uint16_t i;

  if ((glyph >= 'A') && (glyph <= 'Z'))
  {
    src = glyphs[(uint32_t)(glyph - 'A')];
  }
  else if ((glyph >= '0') && (glyph <= '9'))
  {
    src = glyphs[26U + (uint32_t)(glyph - '0')];
  }
  else if (glyph != ' ')
  {
    return 0UL;
  }

  for (i = 0U; i < 7U; ++i)
  {
    rows[i] = src[i];
  }
  return 1UL;
}

static uint32_t PS_HW6_DisplayDrawGlyph(uint16_t x,
                                        uint16_t y,
                                        char glyph,
                                        uint16_t scale)
{
  uint8_t rows[7];
  uint32_t count = 0UL;
  uint16_t row;
  uint16_t col;
  uint16_t sx;
  uint16_t sy;

  if (PS_HW6_DisplayGlyphRows(glyph, rows) == 0UL)
  {
    return 0UL;
  }

  for (row = 0U; row < 7U; ++row)
  {
    for (col = 0U; col < 5U; ++col)
    {
      if ((rows[row] & (uint8_t)(1U << (4U - col))) != 0U)
      {
        for (sy = 0U; sy < scale; ++sy)
        {
          for (sx = 0U; sx < scale; ++sx)
          {
            count += PS_HW6_DisplaySetBlack(
              (uint16_t)(x + (col * scale) + sx),
              (uint16_t)(y + (row * scale) + sy));
          }
        }
      }
    }
  }
  return count;
}

static uint16_t PS_HW6_DisplayTextWidth(const char *text, uint16_t scale)
{
  uint16_t count = 0U;

  while ((text != NULL) && (text[count] != '\0'))
  {
    ++count;
  }
  return (uint16_t)(count * 6U * scale);
}

static uint32_t PS_HW6_DisplayDrawText(uint16_t x,
                                       uint16_t y,
                                       const char *text,
                                       uint16_t scale)
{
  uint32_t black_pixels = 0UL;
  uint16_t index = 0U;

  while ((text != NULL) && (text[index] != '\0'))
  {
    black_pixels += PS_HW6_DisplayDrawGlyph(
      (uint16_t)(x + (index * 6U * scale)), y, text[index], scale);
    ++index;
  }
  return black_pixels;
}

static uint32_t PS_HW6_DisplayDrawCenteredText(uint16_t y,
                                               const char *text,
                                               uint16_t scale)
{
  uint16_t width = PS_HW6_DisplayTextWidth(text, scale);
  uint16_t x = 0U;

  if (width < PS_HW6_DISPLAY_UI_LOGICAL_WIDTH)
  {
    x = (uint16_t)((PS_HW6_DISPLAY_UI_LOGICAL_WIDTH - width) / 2U);
  }
  return PS_HW6_DisplayDrawText(x, y, text, scale);
}

static const char *PS_HW6_DisplayShutdownCountdownLine(
  uint32_t countdown_seconds)
{
  if (countdown_seconds == 3UL)
  {
    return "POWER OFF IN 3";
  }
  if (countdown_seconds == 2UL)
  {
    return "POWER OFF IN 2";
  }
  if (countdown_seconds == 1UL)
  {
    return "POWER OFF IN 1";
  }
  return "PREPARING";
}

static void PS_HW6_DisplayUIStrings(uint32_t page,
                                    uint32_t calibration_page,
                                    uint32_t focus_index,
                                    uint32_t shutdown_state,
                                    uint32_t shutdown_countdown_seconds,
                                    const char **title,
                                    const char **line1,
                                    const char **line2)
{
  *title = "PEEPSHOW";
  *line1 = "HOME";
  *line2 = "MENU A";

  switch (page)
  {
    case PS_UI_ROUTER_PAGE_HOME:
      break;
    case PS_UI_ROUTER_PAGE_MENU:
      *title = "SYSTEM";
      if (focus_index == 0UL)
      {
        *line1 = "SETTINGS";
        *line2 = "CAL INPUT";
      }
      else if (focus_index == 1UL)
      {
        *line1 = "CAL INPUT";
        *line2 = "PACKAGES";
      }
      else
      {
        *line1 = "PACKAGES";
        *line2 = "SETTINGS";
      }
      break;
    case PS_UI_ROUTER_PAGE_SETTINGS:
      *title = "SETTINGS";
      *line1 = "INPUT";
      *line2 = "DISPLAY";
      break;
    case PS_UI_ROUTER_PAGE_CALIBRATION:
      *title = "CALIBRATION";
      if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL)
      {
        *line1 = "STICK CENTER";
        *line2 = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_RIGHT)
      {
        *line1 = "MOVE RIGHT";
        *line2 = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_CIRCLE)
      {
        *line1 = "MAKE CIRCLES";
        *line2 = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_REVIEW)
      {
        *line1 = "REVIEW";
        *line2 = "PRESS A";
      }
      else
      {
        *line1 = "INPUT";
        *line2 = "JOYSTICK A";
      }
      break;
    case PS_UI_ROUTER_PAGE_PACKAGE_BROWSER:
      *title = "USB";
      *line1 = "TRANSFER";
      *line2 = "START A";
      break;
    case PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF:
      *title = "RUNTIME";
      *line1 = "HANDOFF";
      *line2 = "WAIT";
      break;
    case PS_UI_ROUTER_PAGE_ERROR:
      *title = "ERROR";
      *line1 = "SHELL FAULT";
      *line2 = "RECOVER";
      break;
    case PS_UI_ROUTER_PAGE_SHUTDOWN:
      if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_BOOT)
      {
        *title = "LOW BATTERY";
        *line1 = "CHARGE DEVICE";
        *line2 = "NO RUNTIME";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_CHARGE)
      {
        *title = "LOW BATTERY";
        *line1 = "CHARGING";
        *line2 = "PLEASE WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_INIT)
      {
        *title = "FLASH INIT";
        *line1 = "USB STAGING";
        *line2 = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_DONE)
      {
        *title = "FLASH INIT";
        *line1 = "USB STAGING";
        *line2 = "DONE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_ERROR)
      {
        *title = "FLASH INIT";
        *line1 = "USB STAGING";
        *line2 = "ERROR";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_EXPORT)
      {
        *title = "USB MSC";
        *line1 = "EXPORT";
        *line2 = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_ACTIVE)
      {
        *title = "USB MSC";
        *line1 = "ACTIVE";
        *line2 = "EJECT FIRST";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_RECLAIM)
      {
        *title = "USB MSC";
        *line1 = "RECLAIM";
        *line2 = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_DONE)
      {
        *title = "USB MSC";
        *line1 = "RECLAIM";
        *line2 = "DONE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_ERROR)
      {
        *title = "USB MSC";
        *line1 = "ERROR";
        *line2 = "SEE GDB";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_RECOVERY)
      {
        *title = "USB MSC";
        *line1 = "MSC NEEDS";
        *line2 = "FLASH INIT";
      }
      else
      {
        *title = "SHUTDOWN";
        *line1 = PS_HW6_DisplayShutdownCountdownLine(
          shutdown_countdown_seconds);
        *line2 = (shutdown_state == PS_UI_ROUTER_SHUTDOWN_CANCELLED) ?
          "CANCELLED" : "HOLD START";
      }
      break;
    default:
      *title = "BOOT";
      *line1 = "STARTING";
      *line2 = "WAIT";
      break;
  }
}

static void PS_HW6_PrepareDisplayUIPage(uint32_t page,
                                        uint32_t calibration_page,
                                        uint32_t focus_index,
                                        uint32_t shutdown_state,
                                        uint32_t shutdown_countdown_seconds)
{
  const char *title;
  const char *line1;
  const char *line2;
  uint32_t black_pixels = 0UL;

  (void)memset(ps_hw6_display_framebuffer, 0xFF,
               sizeof(ps_hw6_display_framebuffer));
  ps_hw6_display_ui_rotate_ccw = 1UL;
  PS_HW6_DisplayUIStrings(page,
                          calibration_page,
                          focus_index,
                          shutdown_state,
                          shutdown_countdown_seconds,
                          &title,
                          &line1,
                          &line2);

  black_pixels += PS_HW6_DisplayHorizontalLine(
    0U, (uint16_t)(PS_HW6_DISPLAY_UI_LOGICAL_WIDTH - 1U), 0U);
  black_pixels += PS_HW6_DisplayHorizontalLine(
    0U, (uint16_t)(PS_HW6_DISPLAY_UI_LOGICAL_WIDTH - 1U),
    (uint16_t)(PS_HW6_DISPLAY_UI_LOGICAL_HEIGHT - 1U));
  black_pixels += PS_HW6_DisplayVerticalLine(
    0U, 0U, (uint16_t)(PS_HW6_DISPLAY_UI_LOGICAL_HEIGHT - 1U));
  black_pixels += PS_HW6_DisplayVerticalLine(
    (uint16_t)(PS_HW6_DISPLAY_UI_LOGICAL_WIDTH - 1U), 0U,
    (uint16_t)(PS_HW6_DISPLAY_UI_LOGICAL_HEIGHT - 1U));
  black_pixels += PS_HW6_DisplayHorizontalLine(
    8U, (uint16_t)(PS_HW6_DISPLAY_UI_LOGICAL_WIDTH - 9U), 28U);
  black_pixels += PS_HW6_DisplayDrawCenteredText(
    8U, title, PS_HW6_DISPLAY_TEXT_SCALE);
  black_pixels += PS_HW6_DisplayDrawCenteredText(
    58U, line1, PS_HW6_DISPLAY_TEXT_SCALE);
  black_pixels += PS_HW6_DisplayDrawCenteredText(
    94U, line2, PS_HW6_DISPLAY_TEXT_SCALE);
  black_pixels += PS_HW6_DisplayFilledRect(76U, 132U, 16U, 6U);
  ps_hw6_display_ui_rotate_ccw = 0UL;

  g_ps_hw6_owner_probe.display_width = PS_DEV_LS013B7DH05_WIDTH;
  g_ps_hw6_owner_probe.display_height = PS_DEV_LS013B7DH05_HEIGHT;
  g_ps_hw6_owner_probe.display_pattern_id = PS_HW6_DISPLAY_UI_PATTERN_ID;
  g_ps_hw6_owner_probe.display_framebuffer_hash =
    PS_HW6_DisplayFramebufferHash();
  g_ps_hw6_owner_probe.display_black_pixels = black_pixels;
}
static void PS_HW6_PrepareDisplayPattern(void)
{
  uint32_t black_pixels = 0UL;
  uint32_t hash = 2166136261UL;
  uint16_t i;

  (void)memset(ps_hw6_display_framebuffer, 0xFF,
               sizeof(ps_hw6_display_framebuffer));

  for (i = 0U; i < 2U; ++i)
  {
    black_pixels += PS_HW6_DisplayHorizontalLine(
      i, (uint16_t)(PS_DEV_LS013B7DH05_WIDTH - 1U - i), i);
    black_pixels += PS_HW6_DisplayHorizontalLine(
      i, (uint16_t)(PS_DEV_LS013B7DH05_WIDTH - 1U - i),
      (uint16_t)(PS_DEV_LS013B7DH05_HEIGHT - 1U - i));
    black_pixels += PS_HW6_DisplayVerticalLine(
      i, i, (uint16_t)(PS_DEV_LS013B7DH05_HEIGHT - 1U - i));
    black_pixels += PS_HW6_DisplayVerticalLine(
      (uint16_t)(PS_DEV_LS013B7DH05_WIDTH - 1U - i), i,
      (uint16_t)(PS_DEV_LS013B7DH05_HEIGHT - 1U - i));
  }

  black_pixels += PS_HW6_DisplayHorizontalLine(
    8U, (uint16_t)(PS_DEV_LS013B7DH05_WIDTH - 9U), PS_DEV_LS013B7DH05_HEIGHT / 2U);
  black_pixels += PS_HW6_DisplayVerticalLine(
    PS_DEV_LS013B7DH05_WIDTH / 2U, 8U, (uint16_t)(PS_DEV_LS013B7DH05_HEIGHT - 9U));

  black_pixels += PS_HW6_DisplayFilledRect(8U, 8U, 10U, 10U);
  black_pixels += PS_HW6_DisplayHorizontalLine(
    (uint16_t)(PS_DEV_LS013B7DH05_WIDTH - 19U), (uint16_t)(PS_DEV_LS013B7DH05_WIDTH - 9U), 8U);
  black_pixels += PS_HW6_DisplayVerticalLine(
    (uint16_t)(PS_DEV_LS013B7DH05_WIDTH - 9U), 8U, 18U);
  for (i = 0U; i < 11U; ++i)
  {
    black_pixels += PS_HW6_DisplaySetBlack(
      (uint16_t)(8U + i), (uint16_t)(PS_DEV_LS013B7DH05_HEIGHT - 19U + i));
    black_pixels += PS_HW6_DisplaySetBlack(
      (uint16_t)(18U - i), (uint16_t)(PS_DEV_LS013B7DH05_HEIGHT - 19U + i));
  }
  for (i = 0U; i < 12U; ++i)
  {
    if ((i & 1U) == 0U)
    {
      black_pixels += PS_HW6_DisplayFilledRect(
        (uint16_t)(PS_DEV_LS013B7DH05_WIDTH - 20U + i),
        (uint16_t)(PS_DEV_LS013B7DH05_HEIGHT - 20U), 1U, 12U);
    }
  }

  for (i = 0U; i < PS_DEV_LS013B7DH05_BUFFER_SIZE; ++i)
  {
    hash ^= ps_hw6_display_framebuffer[i];
    hash *= 16777619UL;
  }

  g_ps_hw6_owner_probe.display_width = PS_DEV_LS013B7DH05_WIDTH;
  g_ps_hw6_owner_probe.display_height = PS_DEV_LS013B7DH05_HEIGHT;
  g_ps_hw6_owner_probe.display_pattern_id = PS_HW6_DISPLAY_PATTERN_ID;
  g_ps_hw6_owner_probe.display_framebuffer_hash = hash;
  g_ps_hw6_owner_probe.display_black_pixels = black_pixels;
}

static void PS_HW6_PrepareAudioTone(void)
{
  uint32_t frame;

  for (frame = 0UL; frame < PS_HW6_AUDIO_BUFFER_FRAMES; ++frame)
  {
    int16_t sample = ps_hw6_sine_16[frame & 15UL];
    ps_hw6_audio_buffer[(frame * 2UL) + 0UL] = sample;
    ps_hw6_audio_buffer[(frame * 2UL) + 1UL] = sample;
  }

  g_ps_hw6_owner_probe.audio_sample_rate_hz = PS_HW6_AUDIO_SAMPLE_RATE_HZ;
  g_ps_hw6_owner_probe.audio_tone_hz = PS_HW6_AUDIO_TONE_HZ;
  g_ps_hw6_owner_probe.audio_duration_ms = PS_HW6_AUDIO_DURATION_MS;
  g_ps_hw6_owner_probe.audio_amplitude = PS_HW6_AUDIO_AMPLITUDE;
  g_ps_hw6_owner_probe.audio_buffer_halfwords = PS_HW6_AUDIO_BUFFER_HALFWORDS;
}

UINT PS_HW6_OwnerServices_Init(void)
{
  UINT status;
  uint32_t i;

  (void)memset((void *)&g_ps_hw6_owner_probe, 0,
               sizeof(g_ps_hw6_owner_probe));
  g_ps_hw6_owner_probe.magic = PS_HW6_OWNER_PROBE_MAGIC;
  g_ps_hw6_owner_probe.version = PS_HW6_OWNER_PROBE_VERSION;
  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_INIT;
  g_ps_hw6_owner_probe.power_command_send_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_command_send_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_ack_wait_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_command_send_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_ack_wait_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_init_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_present_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_ack_set_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_ui_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_start_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_stop_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_ack_set_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_mr_shipping_mode_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_fuel_gauge_prepare_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_thermistor_config_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_charger_profile_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_interrupt_config_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_software_shipping_mode_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_software_ship_request_count = 0UL;
  g_ps_hw6_owner_probe.power_software_ship_request_tick = 0UL;

  for (i = 0U; i < PS_HW6_OWNER_POWER_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_register_address[i] =
      ps_dev_adp5360_power_register(i);
    g_ps_hw6_owner_probe.power_register_value[i] = 0UL;
    g_ps_hw6_owner_probe.power_lease_get_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_transfer_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_transfer_error[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_lease_put_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
  }

  for (i = 0U; i < PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_charger_config_address[i] =
      ps_dev_adp5360_charger_config_register(i);
    g_ps_hw6_owner_probe.power_charger_config_value[i] = 0UL;
    g_ps_hw6_owner_probe.power_charger_config_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_charger_config_hal_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_charger_config_hal_error[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
  }

  for (i = 0U; i < PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_interrupt_register_address[i] =
      ps_dev_adp5360_interrupt_register(i);
    g_ps_hw6_owner_probe.power_interrupt_register_value[i] = 0UL;
    g_ps_hw6_owner_probe.power_interrupt_register_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_interrupt_register_hal_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_interrupt_register_hal_error[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
  }

  for (i = 0U; i < PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_interrupt_clear_address[i] =
      ps_dev_adp5360_interrupt_register(i + 2U);
    g_ps_hw6_owner_probe.power_interrupt_clear_value[i] = 0UL;
    g_ps_hw6_owner_probe.power_interrupt_clear_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_interrupt_clear_hal_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_interrupt_clear_hal_error[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
  }

  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  PS_HW6_PrepareDisplayPattern();
  PS_HW6_PrepareAudioTone();
  g_ps_hw6_owner_probe.display_driver_init_status =
    (uint32_t)ps_dev_ls013b7dh05_init(&ps_hw6_display, &hspi3);
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.audio_driver_init_status =
    (uint32_t)ps_dev_audio_init(&ps_hw6_audio,
                                &hsai_BlockA1,
                                &handle_GPDMA1_Channel3,
                                SD_MODE_GPIO_Port,
                                SD_MODE_Pin);
  PS_HW6_UpdateAudioDriverProbe();
  status = PS_HW_I2C3_Init(&hi2c3);
  g_ps_hw6_owner_probe.services_init_status = status;
  if (status == TX_SUCCESS)
  {
    ps_status_t driver_status = ps_dev_adp5360_init(
      &ps_hw6_pmic, PS_HW6_PMIC_ADDRESS_7BIT);
    g_ps_hw6_owner_probe.power_driver_init_status =
      (uint32_t)driver_status;
  }
  else
  {
    g_ps_hw6_owner_probe.power_driver_init_status =
      (uint32_t)PS_STATUS_NOT_INITIALIZED;
  }
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;
  return status;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_EnableMrShippingMode(void)
{
  ps_status_t status;

  status = ps_dev_adp5360_enable_mr_shipping_mode(&ps_hw6_pmic);
  g_ps_hw6_owner_probe.power_driver_mr_shipping_mode_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_PrepareFuelGauge(void)
{
  ps_status_t status;

  status = ps_dev_adp5360_prepare_fuel_gauge(&ps_hw6_pmic);
  g_ps_hw6_owner_probe.power_driver_fuel_gauge_prepare_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigureThermistor(void)
{
  ps_status_t status;

  status = ps_dev_adp5360_configure_thermistor(
    &ps_hw6_pmic,
    (uint8_t)KNOB_POWER_CHARGER_THERMISTOR_CONTROL);
  g_ps_hw6_owner_probe.power_driver_thermistor_config_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigureChargerProfile(void)
{
  ps_dev_adp5360_charger_profile_t profile;
  ps_status_t status;

  profile.vbus_ilim = (uint8_t)KNOB_POWER_CHARGER_VBUS_ILIM;
  profile.termination_setting =
    (uint8_t)KNOB_POWER_CHARGER_TERMINATION_SETTING;
  profile.current_setting = (uint8_t)KNOB_POWER_CHARGER_CURRENT_SETTING;
  profile.function_setting = (uint8_t)KNOB_POWER_CHARGER_FUNCTION_SETTING;
  profile.thermistor_control =
    (uint8_t)KNOB_POWER_CHARGER_THERMISTOR_CONTROL;

  status = ps_dev_adp5360_configure_charger_profile(&ps_hw6_pmic, &profile);
  g_ps_hw6_owner_probe.power_driver_charger_profile_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigurePmicInterrupts(void)
{
  ps_dev_adp5360_interrupt_profile_t profile;
  ps_status_t status;

  profile.enable1 = (uint8_t)KNOB_POWER_PMIC_INTERRUPT_ENABLE1;
  profile.enable2 = (uint8_t)KNOB_POWER_PMIC_INTERRUPT_ENABLE2;

  status = ps_dev_adp5360_configure_interrupts(&ps_hw6_pmic, &profile);
  g_ps_hw6_owner_probe.power_driver_interrupt_config_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_EnterSoftwareShipmentMode(void)
{
  ps_status_t status;

  g_ps_hw6_owner_probe.power_software_ship_request_count++;
  g_ps_hw6_owner_probe.power_software_ship_request_tick =
    (uint32_t)tx_time_get();

  status = ps_dev_adp5360_enter_shipment_mode(&ps_hw6_pmic);
  g_ps_hw6_owner_probe.power_driver_software_shipping_mode_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_RunSnapshot(void)
{
  static ps_dev_adp5360_power_snapshot_t snapshot;
  ps_status_t status;
  uint32_t index;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_POWER;
  g_ps_hw6_owner_probe.power_complete = 0UL;
  g_ps_hw6_owner_probe.power_success = 0UL;

  status = ps_dev_adp5360_read_power_snapshot(&ps_hw6_pmic, &snapshot);
  for (index = 0U; index < PS_HW6_OWNER_POWER_REGISTER_COUNT; ++index)
  {
    g_ps_hw6_owner_probe.power_register_address[index] =
      snapshot.register_address[index];
    g_ps_hw6_owner_probe.power_register_value[index] =
      snapshot.register_value[index];
    g_ps_hw6_owner_probe.power_lease_get_status[index] =
      snapshot.acquire_tx_status;
    g_ps_hw6_owner_probe.power_transfer_status[index] =
      snapshot.register_hal_status[index];
    g_ps_hw6_owner_probe.power_transfer_error[index] =
      snapshot.register_hal_error[index];
    g_ps_hw6_owner_probe.power_lease_put_status[index] =
      snapshot.release_tx_status;
  }

  (void)ps_hw_i2c3_diagnostics(
    (uint32_t *)&g_ps_hw6_owner_probe.power_i2c_state_after,
    (uint32_t *)&g_ps_hw6_owner_probe.power_i2c_error_after);
  g_ps_hw6_owner_probe.power_driver_api_version = ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status = ps_hw6_pmic.last_status;
  g_ps_hw6_owner_probe.power_driver_function_ready_mask =
    snapshot.function_ready_mask;
  g_ps_hw6_owner_probe.power_driver_read_ok_mask = snapshot.read_ok_mask;
  g_ps_hw6_owner_probe.power_driver_expected_match_mask =
    snapshot.expected_match_mask;
  g_ps_hw6_owner_probe.power_identity_match = snapshot.identity_match;
  g_ps_hw6_owner_probe.power_fault_clear = snapshot.fault_clear;
  g_ps_hw6_owner_probe.power_rails_ready = snapshot.rails_ready;
  g_ps_hw6_owner_probe.power_charger_status1 = snapshot.charger_status1;
  g_ps_hw6_owner_probe.power_charger_status2 = snapshot.charger_status2;
  g_ps_hw6_owner_probe.power_charger_status1_status =
    snapshot.charger_status1_status;
  g_ps_hw6_owner_probe.power_charger_status2_status =
    snapshot.charger_status2_status;
  g_ps_hw6_owner_probe.power_charger_status1_hal_status =
    snapshot.charger_status1_hal_status;
  g_ps_hw6_owner_probe.power_charger_status1_hal_error =
    snapshot.charger_status1_hal_error;
  g_ps_hw6_owner_probe.power_charger_status2_hal_status =
    snapshot.charger_status2_hal_status;
  g_ps_hw6_owner_probe.power_charger_status2_hal_error =
    snapshot.charger_status2_hal_error;
  g_ps_hw6_owner_probe.power_charger_thermistor_control =
    snapshot.charger_thermistor_control;
  g_ps_hw6_owner_probe.power_charger_thermistor_control_status =
    snapshot.charger_thermistor_control_status;
  g_ps_hw6_owner_probe.power_charger_thermistor_control_hal_status =
    snapshot.charger_thermistor_control_hal_status;
  g_ps_hw6_owner_probe.power_charger_thermistor_control_hal_error =
    snapshot.charger_thermistor_control_hal_error;
  g_ps_hw6_owner_probe.power_charger_monitor_read_ok_mask =
    snapshot.charger_monitor_read_ok_mask;
  g_ps_hw6_owner_probe.power_charger_config_read_ok_mask =
    snapshot.charger_config_read_ok_mask;
  for (index = 0U; index < PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT;
       ++index)
  {
    g_ps_hw6_owner_probe.power_charger_config_address[index] =
      snapshot.charger_config_address[index];
    g_ps_hw6_owner_probe.power_charger_config_value[index] =
      snapshot.charger_config_value[index];
    g_ps_hw6_owner_probe.power_charger_config_status[index] =
      (uint32_t)snapshot.charger_config_status[index];
    g_ps_hw6_owner_probe.power_charger_config_hal_status[index] =
      snapshot.charger_config_hal_status[index];
    g_ps_hw6_owner_probe.power_charger_config_hal_error[index] =
      snapshot.charger_config_hal_error[index];
  }
  g_ps_hw6_owner_probe.power_interrupt_read_ok_mask =
    snapshot.interrupt_read_ok_mask;
  for (index = 0U; index < PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT;
       ++index)
  {
    g_ps_hw6_owner_probe.power_interrupt_register_address[index] =
      snapshot.interrupt_register_address[index];
    g_ps_hw6_owner_probe.power_interrupt_register_value[index] =
      snapshot.interrupt_register_value[index];
    g_ps_hw6_owner_probe.power_interrupt_register_status[index] =
      (uint32_t)snapshot.interrupt_register_status[index];
    g_ps_hw6_owner_probe.power_interrupt_register_hal_status[index] =
      snapshot.interrupt_register_hal_status[index];
    g_ps_hw6_owner_probe.power_interrupt_register_hal_error[index] =
      snapshot.interrupt_register_hal_error[index];
  }
  g_ps_hw6_owner_probe.power_interrupt_clear_ok_mask =
    snapshot.interrupt_clear_ok_mask;
  for (index = 0U; index < PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT;
       ++index)
  {
    g_ps_hw6_owner_probe.power_interrupt_clear_address[index] =
      snapshot.interrupt_clear_address[index];
    g_ps_hw6_owner_probe.power_interrupt_clear_value[index] =
      snapshot.interrupt_clear_value[index];
    g_ps_hw6_owner_probe.power_interrupt_clear_status[index] =
      (uint32_t)snapshot.interrupt_clear_status[index];
    g_ps_hw6_owner_probe.power_interrupt_clear_hal_status[index] =
      snapshot.interrupt_clear_hal_status[index];
    g_ps_hw6_owner_probe.power_interrupt_clear_hal_error[index] =
      snapshot.interrupt_clear_hal_error[index];
  }
  g_ps_hw6_owner_probe.power_charger_mode = snapshot.charger_mode;
  g_ps_hw6_owner_probe.power_charger_status = snapshot.charger_status;
  g_ps_hw6_owner_probe.power_charger_charge_type =
    snapshot.charger_charge_type;
  g_ps_hw6_owner_probe.power_charger_health = snapshot.charger_health;
  g_ps_hw6_owner_probe.power_battery_status = snapshot.battery_status;
  g_ps_hw6_owner_probe.power_battery_thermal_status =
    snapshot.battery_thermal_status;
  g_ps_hw6_owner_probe.power_battery_present = snapshot.battery_present;
  g_ps_hw6_owner_probe.power_vbus_ok = snapshot.vbus_ok;
  g_ps_hw6_owner_probe.power_mcu_vbus_present =
    (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_hw6_owner_probe.power_vbus_agree =
    (g_ps_hw6_owner_probe.power_vbus_ok ==
     g_ps_hw6_owner_probe.power_mcu_vbus_present) ? 1UL : 0UL;
  if (g_ps_hw6_owner_probe.power_vbus_agree == 0UL)
  {
    g_ps_hw6_owner_probe.power_vbus_disagree_count++;
    g_ps_hw6_owner_probe.power_vbus_last_disagree_tick =
      (uint32_t)tx_time_get();
  }
  g_ps_hw6_owner_probe.power_battery_ok = snapshot.battery_ok;
  g_ps_hw6_owner_probe.power_charge_complete = snapshot.charge_complete;
  for (index = 0U; index < PS_DEV_ADP5360_FUEL_REGISTER_COUNT; ++index)
  {
    g_ps_hw6_owner_probe.power_fuel_register_address[index] =
      snapshot.fuel_register_address[index];
    g_ps_hw6_owner_probe.power_fuel_register_value[index] =
      snapshot.fuel_register_value[index];
    g_ps_hw6_owner_probe.power_fuel_register_status[index] =
      (uint32_t)snapshot.fuel_register_status[index];
    g_ps_hw6_owner_probe.power_fuel_register_hal_status[index] =
      snapshot.fuel_register_hal_status[index];
    g_ps_hw6_owner_probe.power_fuel_register_hal_error[index] =
      snapshot.fuel_register_hal_error[index];
  }
  g_ps_hw6_owner_probe.power_fuel_read_ok_mask = snapshot.fuel_read_ok_mask;
  g_ps_hw6_owner_probe.power_fuel_soc_percent = snapshot.fuel_soc_percent;
  g_ps_hw6_owner_probe.power_fuel_vbat_mv = snapshot.fuel_vbat_mv;
  g_ps_hw6_owner_probe.power_fuel_vbat_h = snapshot.fuel_vbat_h;
  g_ps_hw6_owner_probe.power_fuel_vbat_l = snapshot.fuel_vbat_l;
  g_ps_hw6_owner_probe.power_regulator_read_ok_mask =
    snapshot.regulator_read_ok_mask;
  g_ps_hw6_owner_probe.power_regulator_buck_cfg =
    snapshot.regulator_buck_cfg;
  g_ps_hw6_owner_probe.power_regulator_buck_output =
    snapshot.regulator_buck_output;
  g_ps_hw6_owner_probe.power_regulator_buckbst_cfg =
    snapshot.regulator_buckbst_cfg;
  g_ps_hw6_owner_probe.power_regulator_buckbst_output =
    snapshot.regulator_buckbst_output;
  g_ps_hw6_owner_probe.power_regulator_vout1_ok =
    snapshot.regulator_vout1_ok;
  g_ps_hw6_owner_probe.power_regulator_vout2_ok =
    snapshot.regulator_vout2_ok;
  g_ps_hw6_owner_probe.power_regulator_battery_ok =
    snapshot.regulator_battery_ok;
  g_ps_hw6_owner_probe.power_complete = 1UL;
  g_ps_hw6_owner_probe.power_success = (status == PS_STATUS_OK) ? 1UL : 0UL;
  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_RunPattern(void)
{
  ps_dev_ls013b7dh05_present_result_t result;
  ps_status_t driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  g_ps_hw6_owner_probe.display_rtc_state = HAL_RTC_GetState(&hrtc);
  g_ps_hw6_owner_probe.display_rtc_cr = hrtc.Instance->CR;
  g_ps_hw6_owner_probe.display_spi_state_before = HAL_SPI_GetState(&hspi3);

  driver_status = ps_dev_ls013b7dh05_present_full_dma(
    &ps_hw6_display,
    ps_hw6_display_framebuffer,
    PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS,
    &handle_LPDMA1_Channel0,
    &result);

  g_ps_hw6_owner_probe.display_init_status = result.init_hal_status;
  g_ps_hw6_owner_probe.display_present_status = result.present_hal_status;
  g_ps_hw6_owner_probe.display_dma_done = result.dma_done;
  g_ps_hw6_owner_probe.display_spi_state_after = result.spi_state_after;
  g_ps_hw6_owner_probe.display_spi_error_after = result.spi_error_after;
  g_ps_hw6_owner_probe.display_dma_state_after = result.dma_state_after;
  g_ps_hw6_owner_probe.display_dma_error_after = result.dma_error_after;
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((driver_status == PS_STATUS_OK) &&
      (g_ps_hw6_owner_probe.display_dma_done != 0UL) &&
      (g_ps_hw6_owner_probe.display_rtc_state == HAL_RTC_STATE_READY) &&
      ((g_ps_hw6_owner_probe.display_rtc_cr & RTC_CR_COE) != 0UL) &&
      (g_ps_hw6_owner_probe.display_spi_error_after == HAL_SPI_ERROR_NONE) &&
      (g_ps_hw6_owner_probe.display_dma_error_after == HAL_DMA_ERROR_NONE))
  {
    g_ps_hw6_owner_probe.display_success = 1UL;
    return HAL_OK;
  }

  return HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_ClearBootHold(void)
{
  uint32_t clear_hal_status = (uint32_t)HAL_ERROR;
  ps_status_t driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  g_ps_hw6_owner_probe.display_ui_request_count++;
  g_ps_hw6_owner_probe.display_ui_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
  g_ps_hw6_owner_probe.display_ui_calibration_page = PS_UI_ROUTER_CAL_NONE;
  g_ps_hw6_owner_probe.display_ui_focus_index = 0UL;
  g_ps_hw6_owner_probe.display_ui_shutdown_state =
    PS_UI_ROUTER_SHUTDOWN_NONE;
  g_ps_hw6_owner_probe.display_ui_shutdown_countdown_seconds = 0UL;
  g_ps_hw6_owner_probe.display_rtc_state = HAL_RTC_GetState(&hrtc);
  g_ps_hw6_owner_probe.display_rtc_cr = hrtc.Instance->CR;
  g_ps_hw6_owner_probe.display_spi_state_before = HAL_SPI_GetState(&hspi3);

  (void)memset(ps_hw6_display_framebuffer, 0xFF,
               sizeof(ps_hw6_display_framebuffer));
  ps_hw6_display_ui_rotate_ccw = 1UL;
  g_ps_hw6_owner_probe.display_framebuffer_hash =
    PS_HW6_DisplayFramebufferHash();
  g_ps_hw6_owner_probe.display_black_pixels = 0UL;

  driver_status = ps_dev_ls013b7dh05_clear(&ps_hw6_display,
                                           &clear_hal_status);
  g_ps_hw6_owner_probe.display_init_status = clear_hal_status;
  g_ps_hw6_owner_probe.display_present_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_dma_done = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  g_ps_hw6_owner_probe.display_spi_state_after = HAL_SPI_GetState(&hspi3);
  g_ps_hw6_owner_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_hw6_owner_probe.display_dma_state_after =
    HAL_DMA_GetState(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_dma_error_after =
    HAL_DMA_GetError(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_ui_status = (uint32_t)driver_status;
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((driver_status == PS_STATUS_OK) &&
      (g_ps_hw6_owner_probe.display_rtc_state == HAL_RTC_STATE_READY) &&
      ((g_ps_hw6_owner_probe.display_rtc_cr & RTC_CR_COE) != 0UL) &&
      (g_ps_hw6_owner_probe.display_spi_error_after == HAL_SPI_ERROR_NONE))
  {
    g_ps_hw6_owner_probe.display_success = 1UL;
    return HAL_OK;
  }

  return HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_RenderUI(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds)
{
  ps_dev_ls013b7dh05_present_result_t result;
  ps_status_t driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  g_ps_hw6_owner_probe.display_ui_request_count++;
  g_ps_hw6_owner_probe.display_ui_page = page;
  g_ps_hw6_owner_probe.display_ui_calibration_page = calibration_page;
  g_ps_hw6_owner_probe.display_ui_focus_index = focus_index;
  g_ps_hw6_owner_probe.display_ui_shutdown_state = shutdown_state;
  g_ps_hw6_owner_probe.display_ui_shutdown_countdown_seconds =
    shutdown_countdown_seconds;
  g_ps_hw6_owner_probe.display_rtc_state = HAL_RTC_GetState(&hrtc);
  g_ps_hw6_owner_probe.display_rtc_cr = hrtc.Instance->CR;
  g_ps_hw6_owner_probe.display_spi_state_before = HAL_SPI_GetState(&hspi3);

  PS_HW6_PrepareDisplayUIPage(page,
                              calibration_page,
                              focus_index,
                              shutdown_state,
                              shutdown_countdown_seconds);
  driver_status = ps_dev_ls013b7dh05_present_full_dma(
    &ps_hw6_display,
    ps_hw6_display_framebuffer,
    PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS,
    &handle_LPDMA1_Channel0,
    &result);

  g_ps_hw6_owner_probe.display_init_status = result.init_hal_status;
  g_ps_hw6_owner_probe.display_present_status = result.present_hal_status;
  g_ps_hw6_owner_probe.display_dma_done = result.dma_done;
  g_ps_hw6_owner_probe.display_spi_state_after = result.spi_state_after;
  g_ps_hw6_owner_probe.display_spi_error_after = result.spi_error_after;
  g_ps_hw6_owner_probe.display_dma_state_after = result.dma_state_after;
  g_ps_hw6_owner_probe.display_dma_error_after = result.dma_error_after;
  g_ps_hw6_owner_probe.display_ui_status = (uint32_t)driver_status;
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((driver_status == PS_STATUS_OK) &&
      (g_ps_hw6_owner_probe.display_dma_done != 0UL) &&
      (g_ps_hw6_owner_probe.display_rtc_state == HAL_RTC_STATE_READY) &&
      ((g_ps_hw6_owner_probe.display_rtc_cr & RTC_CR_COE) != 0UL) &&
      (g_ps_hw6_owner_probe.display_spi_error_after == HAL_SPI_ERROR_NONE) &&
      (g_ps_hw6_owner_probe.display_dma_error_after == HAL_DMA_ERROR_NONE))
  {
    g_ps_hw6_owner_probe.display_ui_render_count++;
    g_ps_hw6_owner_probe.display_success = 1UL;
    return HAL_OK;
  }

  return HAL_ERROR;
}
HAL_StatusTypeDef PS_HW6_AudioOwner_VerifyIdle(void)
{
  ps_dev_audio_play_result_t result;
  ps_status_t driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_AUDIO;
  g_ps_hw6_owner_probe.audio_complete = 0UL;
  g_ps_hw6_owner_probe.audio_success = 0UL;

  driver_status = ps_dev_audio_verify_idle(&ps_hw6_audio, &result);
  g_ps_hw6_owner_probe.audio_sd_state_after = result.sd_state_after;
  g_ps_hw6_owner_probe.audio_sai_state_after = result.sai_state_after;
  g_ps_hw6_owner_probe.audio_sai_error_after = result.sai_error_after;
  g_ps_hw6_owner_probe.audio_dma_state_after = result.dma_state_after;
  g_ps_hw6_owner_probe.audio_dma_error_after = result.dma_error_after;
  PS_HW6_UpdateAudioDriverProbe();
  g_ps_hw6_owner_probe.audio_complete = 1UL;
  if (driver_status == PS_STATUS_OK)
  {
    g_ps_hw6_owner_probe.audio_success = 1UL;
    return HAL_OK;
  }
  return HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_AudioOwner_RunTone(void)
{
  ps_dev_audio_play_result_t result;
  ps_status_t driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_AUDIO;
  g_ps_hw6_owner_probe.audio_complete = 0UL;
  g_ps_hw6_owner_probe.audio_success = 0UL;

  driver_status = ps_dev_audio_play_dma(
    &ps_hw6_audio,
    ps_hw6_audio_buffer,
    PS_HW6_AUDIO_BUFFER_HALFWORDS,
    PS_HW6_AUDIO_AMP_SETTLE_TICKS,
    PS_HW6_AUDIO_DURATION_TICKS,
    4096000UL,
    &result);

  g_ps_hw6_owner_probe.audio_sai_kernel_hz = result.sai_kernel_hz;
  g_ps_hw6_owner_probe.audio_sd_state_before = result.sd_state_before;
  g_ps_hw6_owner_probe.audio_sd_state_enabled = result.sd_state_enabled;
  g_ps_hw6_owner_probe.audio_start_status = result.start_hal_status;
  g_ps_hw6_owner_probe.audio_stop_status = result.stop_hal_status;
  g_ps_hw6_owner_probe.audio_sd_state_after = result.sd_state_after;
  g_ps_hw6_owner_probe.audio_sai_state_after = result.sai_state_after;
  g_ps_hw6_owner_probe.audio_sai_error_after = result.sai_error_after;
  g_ps_hw6_owner_probe.audio_dma_state_after = result.dma_state_after;
  g_ps_hw6_owner_probe.audio_dma_error_after = result.dma_error_after;
  PS_HW6_UpdateAudioDriverProbe();
  g_ps_hw6_owner_probe.audio_complete = 1UL;

  if (driver_status == PS_STATUS_OK)
  {
    g_ps_hw6_owner_probe.audio_success = 1UL;
    return HAL_OK;
  }

  return HAL_ERROR;
}

void PS_HW6_OwnerServices_MarkComplete(void)
{
  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_COMPLETE;
}
