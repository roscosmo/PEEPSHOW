#include "ps_hw6_clock_policy.h"

#include <string.h>

#include "knobs_autogen.h"
#include "main.h"
#include "ps_hw6_trace.h"

extern void SystemClock_Config(void);
extern void PeriphCommonClock_Config(void);

#define PS_HW6_CLOCK_STAGE_IDLE             (0UL)
#define PS_HW6_CLOCK_STAGE_SELECT           (1UL)
#define PS_HW6_CLOCK_STAGE_SYSCLK_IO_HIGH   (2UL)
#define PS_HW6_CLOCK_STAGE_USB_DOMAIN_ON    (3UL)
#define PS_HW6_CLOCK_STAGE_RESTORE_BASE     (4UL)
#define PS_HW6_CLOCK_STAGE_USB_DOMAIN_OFF   (5UL)
#define PS_HW6_CLOCK_STAGE_SYSTICK          (6UL)
#define PS_HW6_CLOCK_STAGE_COMPLETE         (7UL)
#define PS_HW6_CLOCK_STAGE_REQUESTER_UPDATE (8UL)
#define PS_HW6_CLOCK_STAGE_RESOLVE          (9UL)
#define PS_HW6_CLOCK_STAGE_PLL2_DOMAIN_ON  (10UL)
#define PS_HW6_CLOCK_STAGE_PLL2_DOMAIN_OFF (11UL)
#define PS_HW6_CLOCK_STAGE_PLL2_REARM       (12UL)
#define PS_HW6_CLOCK_STAGE_SAI_DOMAIN_ON    (13UL)
#define PS_HW6_CLOCK_STAGE_SAI_DOMAIN_OFF   (14UL)
#define PS_HW6_CLOCK_STAGE_STOP2_VERIFY     (15UL)

#define PS_HW6_CLOCK_PROFILE_BIT(profile) \
  (1UL << ((uint32_t)(profile)))
#define PS_HW6_CLOCK_SUPPORTED_PROFILE_MASK \
  (PS_HW6_CLOCK_PROFILE_BIT(PS_HW6_CLOCK_PROFILE_BOOT_RECOVERY) | \
   PS_HW6_CLOCK_PROFILE_BIT(PS_HW6_CLOCK_PROFILE_REACTIVE_BASE) | \
   PS_HW6_CLOCK_PROFILE_BIT(PS_HW6_CLOCK_PROFILE_IO_HIGH) | \
   PS_HW6_CLOCK_PROFILE_BIT(PS_HW6_CLOCK_PROFILE_STOP_PREP))
#define PS_HW6_CLOCK_SCAFFOLD_PROFILE_MASK \
  (PS_HW6_CLOCK_PROFILE_BIT(PS_HW6_CLOCK_PROFILE_REACTIVE_BURST) | \
   PS_HW6_CLOCK_PROFILE_BIT(PS_HW6_CLOCK_PROFILE_REALTIME_BALANCED))

#define PS_HW6_CLOCK_STOP2_BLOCKER_CAP_MASK \
  (PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE | \
   PS_HW6_CLOCK_CAP_OCTOSPI_ACTIVE | \
   PS_HW6_CLOCK_CAP_SAI_AUDIO_ACTIVE | \
   PS_HW6_CLOCK_CAP_DISPLAY_TRANSFER_ACTIVE | \
   PS_HW6_CLOCK_CAP_REALTIME_DEADLINE_ACTIVE | \
   PS_HW6_CLOCK_CAP_REACTIVE_TRANSACTION_ACTIVE)
#define PS_HW6_CLOCK_PLL2_DOMAIN_MASK \
  (PS_HW6_CLOCK_DOMAIN_PLL2_OCTOSPI | \
   PS_HW6_CLOCK_DOMAIN_PLL2_SAI)

volatile ps_hw6_clock_policy_probe_t g_ps_hw6_clock_policy_probe;

static void PS_HW6_ClockPolicy_SetStaticProbeFields(void)
{
  g_ps_hw6_clock_policy_probe.api_version =
    PS_HW6_CLOCK_POLICY_API_VERSION;
  g_ps_hw6_clock_policy_probe.supported_profile_mask =
    PS_HW6_CLOCK_SUPPORTED_PROFILE_MASK;
  g_ps_hw6_clock_policy_probe.scaffold_profile_mask =
    PS_HW6_CLOCK_SCAFFOLD_PROFILE_MASK;
  g_ps_hw6_clock_policy_probe.pll2_autogate_enabled =
    ((uint32_t)KNOB_POWER_CLOCK_PLL2_AUTOGATE_ENABLE != 0UL) ? 1UL : 0UL;
}

static void PS_HW6_ClockPolicy_ClearRequesterStatus(void)
{
  uint32_t index;

  for (index = 0U; index < PS_HW6_CLOCK_REQUESTER_COUNT; ++index)
  {
    g_ps_hw6_clock_policy_probe.requester_status[index] =
      PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  }
}

static void PS_HW6_ClockPolicy_PrimeProbe(void)
{
  if (g_ps_hw6_clock_policy_probe.api_version !=
      PS_HW6_CLOCK_POLICY_API_VERSION)
  {
    (void)memset((void *)&g_ps_hw6_clock_policy_probe, 0,
                 sizeof(g_ps_hw6_clock_policy_probe));
    PS_HW6_ClockPolicy_SetStaticProbeFields();
    g_ps_hw6_clock_policy_probe.current_profile =
      (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN;
    g_ps_hw6_clock_policy_probe.requested_profile =
      (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN;
    g_ps_hw6_clock_policy_probe.selected_profile =
      (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN;
    g_ps_hw6_clock_policy_probe.last_stage = PS_HW6_CLOCK_STAGE_IDLE;
    g_ps_hw6_clock_policy_probe.last_status =
      PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
    g_ps_hw6_clock_policy_probe.pll2_domain_last_status =
      PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
    g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_status =
      PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
    g_ps_hw6_clock_policy_probe.sai_mux_handoff_status =
      PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
    g_ps_hw6_clock_policy_probe.sai_domain_last_status =
      PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
    g_ps_hw6_clock_policy_probe.stop2_prepare_status =
      PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
    PS_HW6_ClockPolicy_ClearRequesterStatus();
  }
}

static uint32_t PS_HW6_ClockPolicy_CapabilitiesToDomainMask(
  uint32_t capabilities)
{
  uint32_t domain_mask = 0UL;

  if ((capabilities & PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE) != 0UL)
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_USB_DEVICE;
  }
  if ((capabilities & PS_HW6_CLOCK_CAP_OCTOSPI_ACTIVE) != 0UL)
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_PLL2_OCTOSPI;
  }
  if ((capabilities & PS_HW6_CLOCK_CAP_SAI_AUDIO_ACTIVE) != 0UL)
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_PLL2_SAI;
  }
  if ((capabilities & PS_HW6_CLOCK_CAP_DISPLAY_TRANSFER_ACTIVE) != 0UL)
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_DISPLAY_TRANSFER;
  }
  if ((capabilities & PS_HW6_CLOCK_CAP_REALTIME_DEADLINE_ACTIVE) != 0UL)
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_REALTIME_DEADLINE;
  }
  if ((capabilities & PS_HW6_CLOCK_CAP_REACTIVE_TRANSACTION_ACTIVE) != 0UL)
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_REACTIVE_TRANSACTION;
  }
  if ((capabilities & PS_HW6_CLOCK_CAP_LPBAM_DISPLAY_AUTONOMOUS) != 0UL)
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_LPBAM_DISPLAY_AUTONOMOUS;
  }

  return domain_mask;
}

static uint32_t PS_HW6_ClockPolicy_TargetHzForProfile(uint32_t profile)
{
  switch ((PS_HW6_ClockProfile)profile)
  {
    case PS_HW6_CLOCK_PROFILE_BOOT_RECOVERY:
    case PS_HW6_CLOCK_PROFILE_REACTIVE_BASE:
    case PS_HW6_CLOCK_PROFILE_STOP_PREP:
      return (uint32_t)KNOB_POWER_CLOCK_REACTIVE_BASE_HZ;
    case PS_HW6_CLOCK_PROFILE_REACTIVE_BURST:
      return (uint32_t)KNOB_POWER_CLOCK_REACTIVE_BURST_HZ;
    case PS_HW6_CLOCK_PROFILE_REALTIME_BALANCED:
      return (uint32_t)KNOB_POWER_CLOCK_REALTIME_BALANCED_HZ;
    case PS_HW6_CLOCK_PROFILE_IO_HIGH:
      return (uint32_t)KNOB_POWER_CLOCK_IO_HIGH_HZ;
    default:
      return 0UL;
  }
}

static uint32_t PS_HW6_ClockPolicy_ReadbackDomainMask(void)
{
  uint32_t domain_mask = 0UL;

  if ((g_ps_hw6_clock_policy_probe.usb_clock_enabled != 0UL) ||
      (g_ps_hw6_clock_policy_probe.vddusb_enabled != 0UL) ||
      (g_ps_hw6_clock_policy_probe.hsi48_ready != 0UL))
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_USB_DEVICE;
  }
  if ((g_ps_hw6_clock_policy_probe.pll2_ready != 0UL) &&
      (g_ps_hw6_clock_policy_probe.ospi_kernel_hz != 0UL))
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_PLL2_OCTOSPI;
  }
  if ((g_ps_hw6_clock_policy_probe.pll2_ready != 0UL) &&
      (g_ps_hw6_clock_policy_probe.sai_clock_enabled != 0UL) &&
      (g_ps_hw6_clock_policy_probe.sai1_kernel_hz != 0UL))
  {
    domain_mask |= PS_HW6_CLOCK_DOMAIN_PLL2_SAI;
  }

  return domain_mask;
}

static void PS_HW6_ClockPolicy_RecordSnapshot(void)
{
  g_ps_hw6_clock_policy_probe.sysclk_after_hz =
    HAL_RCC_GetSysClockFreq();
  g_ps_hw6_clock_policy_probe.hclk_after_hz =
    HAL_RCC_GetHCLKFreq();
  g_ps_hw6_clock_policy_probe.pclk1_after_hz =
    HAL_RCC_GetPCLK1Freq();
  g_ps_hw6_clock_policy_probe.pclk2_after_hz =
    HAL_RCC_GetPCLK2Freq();
  g_ps_hw6_clock_policy_probe.pclk3_after_hz =
    HAL_RCC_GetPCLK3Freq();
  g_ps_hw6_clock_policy_probe.flash_latency =
    __HAL_FLASH_GET_LATENCY();
  g_ps_hw6_clock_policy_probe.usb_clock_enabled =
    (__HAL_RCC_USB_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.vddusb_enabled =
    (READ_BIT(PWR->SVMCR, PWR_SVMCR_USV) != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.hsi48_ready =
    (__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.shsi_ready =
    (__HAL_RCC_GET_FLAG(RCC_FLAG_SHSIRDY) != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.pll1_ready =
    (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL1RDY) != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.pll2_ready =
    (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.pll3_ready =
    (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY) != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.pll2_output_enabled_mask =
    READ_BIT(RCC->PLL2CFGR,
             RCC_PLL2CFGR_PLL2PEN | RCC_PLL2CFGR_PLL2QEN |
             RCC_PLL2CFGR_PLL2REN);
  g_ps_hw6_clock_policy_probe.sai_clock_enabled =
    (__HAL_RCC_SAI1_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.sai_reset_asserted =
    (READ_BIT(RCC->APB2RSTR, RCC_APB2RSTR_SAI1RST) != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.sai_domain_active =
    ((g_ps_hw6_clock_policy_probe.sai_clock_enabled != 0UL) &&
     (g_ps_hw6_clock_policy_probe.sai_reset_asserted == 0UL) &&
     (g_ps_hw6_clock_policy_probe.pll2_ready != 0UL) &&
     ((g_ps_hw6_clock_policy_probe.pll2_output_enabled_mask &
       RCC_PLL2_DIVP) != 0UL)) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.usb_kernel_hz =
    ((g_ps_hw6_clock_policy_probe.usb_clock_enabled != 0UL) &&
     (g_ps_hw6_clock_policy_probe.vddusb_enabled != 0UL) &&
     (g_ps_hw6_clock_policy_probe.hsi48_ready != 0UL)) ?
    HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_ICLK) : 0UL;
  g_ps_hw6_clock_policy_probe.sai1_kernel_hz =
    (g_ps_hw6_clock_policy_probe.sai_domain_active != 0UL) ?
    HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1) : 0UL;
  g_ps_hw6_clock_policy_probe.ospi_kernel_hz =
    ((g_ps_hw6_clock_policy_probe.pll2_ready != 0UL) &&
     ((g_ps_hw6_clock_policy_probe.pll2_output_enabled_mask &
       RCC_PLL2_DIVQ) != 0UL)) ?
    HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_OSPI) : 0UL;
  g_ps_hw6_clock_policy_probe.readback_domain_mask =
    PS_HW6_ClockPolicy_ReadbackDomainMask();
}

static uint32_t PS_HW6_ClockPolicy_AggregateRequesterCapabilities(void)
{
  uint32_t index;
  uint32_t capabilities = 0UL;
  uint32_t active_mask = 0UL;

  for (index = 0U; index < PS_HW6_CLOCK_REQUESTER_COUNT; ++index)
  {
    uint32_t requester_caps =
      g_ps_hw6_clock_policy_probe.requester_capabilities[index];

    if (requester_caps != 0UL)
    {
      active_mask |= (1UL << index);
      capabilities |= requester_caps;
    }
  }

  g_ps_hw6_clock_policy_probe.requester_active_mask = active_mask;
  g_ps_hw6_clock_policy_probe.aggregated_capabilities = capabilities;
  return capabilities;
}

static void PS_HW6_ClockPolicy_UpdateResolverProbe(uint32_t capabilities,
                                                    uint32_t selected_profile)
{
  uint32_t blocker_capabilities =
    capabilities & PS_HW6_CLOCK_STOP2_BLOCKER_CAP_MASK;

  g_ps_hw6_clock_policy_probe.active_capabilities = capabilities;
  g_ps_hw6_clock_policy_probe.required_domain_mask =
    PS_HW6_ClockPolicy_CapabilitiesToDomainMask(capabilities);
  g_ps_hw6_clock_policy_probe.stop2_blocker_capabilities =
    blocker_capabilities;
  g_ps_hw6_clock_policy_probe.stop2_blocker_domain_mask =
    PS_HW6_ClockPolicy_CapabilitiesToDomainMask(blocker_capabilities);
  g_ps_hw6_clock_policy_probe.stop2_ready =
    (blocker_capabilities == 0UL) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.lpbam_stop2_ready =
    (((capabilities & PS_HW6_CLOCK_CAP_LPBAM_DISPLAY_AUTONOMOUS) != 0UL) &&
     (blocker_capabilities == 0UL)) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.target_sysclk_hz =
    PS_HW6_ClockPolicy_TargetHzForProfile(selected_profile);
  g_ps_hw6_clock_policy_probe.pll2_autogate_enabled =
    ((uint32_t)KNOB_POWER_CLOCK_PLL2_AUTOGATE_ENABLE != 0UL) ? 1UL : 0UL;
}

static void PS_HW6_ClockPolicy_UpdatePostSnapshotProbe(void)
{
  if ((g_ps_hw6_clock_policy_probe.pll2_autogate_enabled == 0UL) &&
      ((g_ps_hw6_clock_policy_probe.required_domain_mask &
        PS_HW6_CLOCK_PLL2_DOMAIN_MASK) == 0UL) &&
      ((g_ps_hw6_clock_policy_probe.pll2_ready != 0UL) ||
       (g_ps_hw6_clock_policy_probe.pll2_output_enabled_mask != 0UL)))
  {
    g_ps_hw6_clock_policy_probe.pll2_autogate_skip_count++;
  }
}

static UINT PS_HW6_ClockPolicy_RetuneThreadXSysTick(void)
{
  uint32_t hclk_hz = HAL_RCC_GetHCLKFreq();
  uint32_t reload;

  if (hclk_hz == 0UL)
  {
    return TX_NOT_DONE;
  }

  reload = hclk_hz / (uint32_t)TX_TIMER_TICKS_PER_SECOND;
  if (reload == 0UL)
  {
    reload = 1UL;
  }
  if (reload > 0x01000000UL)
  {
    return TX_NOT_DONE;
  }

  SysTick->LOAD = reload - 1UL;
  SysTick->VAL = 0UL;
  return TX_SUCCESS;
}

static UINT PS_HW6_ClockPolicy_Hsi48Set(uint32_t hsi48_state)
{
  RCC_OscInitTypeDef osc = {0};
  HAL_StatusTypeDef hal_status;

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  osc.HSI48State = hsi48_state;
  osc.PLL.PLLState = RCC_PLL_NONE;

  hal_status = HAL_RCC_OscConfig(&osc);
  return (hal_status == HAL_OK) ? TX_SUCCESS : TX_NOT_DONE;
}

static void PS_HW6_ClockPolicy_VddUsbSet(UINT enabled)
{
  UINT pwr_clock_was_disabled =
    (__HAL_RCC_PWR_IS_CLK_DISABLED() != 0U) ? 1U : 0U;

  if (pwr_clock_was_disabled != 0U)
  {
    __HAL_RCC_PWR_CLK_ENABLE();
  }

  if (enabled != 0U)
  {
    HAL_PWREx_EnableVddUSB();
  }
  else
  {
    HAL_PWREx_DisableVddUSB();
  }

  if (pwr_clock_was_disabled != 0U)
  {
    __HAL_RCC_PWR_CLK_DISABLE();
  }
}

static UINT PS_HW6_ClockPolicy_UsbDomainSet(UINT enabled)
{
  UINT status;

  if (enabled != 0U)
  {
    status = PS_HW6_ClockPolicy_Hsi48Set(RCC_HSI48_ON);
    if (status == TX_SUCCESS)
    {
      RCC_PeriphCLKInitTypeDef periph_clk = {0};

      periph_clk.PeriphClockSelection = RCC_PERIPHCLK_ICLK;
      periph_clk.IclkClockSelection = RCC_CLK48CLKSOURCE_HSI48;
      status = (HAL_RCCEx_PeriphCLKConfig(&periph_clk) == HAL_OK) ?
               TX_SUCCESS : TX_NOT_DONE;
    }
    if (status == TX_SUCCESS)
    {
      PS_HW6_ClockPolicy_VddUsbSet(1U);
      g_ps_hw6_clock_policy_probe.usb_domain_on_count++;
    }
  }
  else
  {
    PS_HW6_ClockPolicy_VddUsbSet(0U);
    status = PS_HW6_ClockPolicy_Hsi48Set(RCC_HSI48_OFF);
    if (status == TX_SUCCESS)
    {
      g_ps_hw6_clock_policy_probe.usb_domain_off_count++;
    }
  }

  return status;
}

static uint32_t PS_HW6_ClockPolicy_Pll2OutputMaskForDomains(
  uint32_t required_domain_mask)
{
  uint32_t output_mask = 0UL;

  if ((required_domain_mask & PS_HW6_CLOCK_DOMAIN_PLL2_SAI) != 0UL)
  {
    output_mask |= RCC_PLL2_DIVP;
  }
  if ((required_domain_mask & PS_HW6_CLOCK_DOMAIN_PLL2_OCTOSPI) != 0UL)
  {
    output_mask |= RCC_PLL2_DIVQ;
  }

  return output_mask;
}

static void PS_HW6_ClockPolicy_FillPll2Config(
  RCC_PLL2InitTypeDef *pll2,
  uint32_t output_mask)
{
  pll2->PLL2Source = RCC_PLLSOURCE_HSI;
  pll2->PLL2M = 1;
  pll2->PLL2N = 32;
  pll2->PLL2P = 125;
  pll2->PLL2Q = 4;
  pll2->PLL2R = 2;
  pll2->PLL2RGE = RCC_PLLVCIRANGE_1;
  pll2->PLL2FRACN = 0;
  pll2->PLL2ClockOut = output_mask;
}

static HAL_StatusTypeDef PS_HW6_ClockPolicy_SaiMuxParkOnHsi(void)
{
  HAL_StatusTypeDef status = HAL_OK;

  g_ps_hw6_clock_policy_probe.sai_mux_handoff_count++;
  g_ps_hw6_clock_policy_probe.sai_mux_handoff_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;

  if ((__HAL_RCC_SAI1_IS_CLK_ENABLED() != 0U) ||
      (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == 0U))
  {
    status = HAL_ERROR;
  }
  if (status == HAL_OK)
  {
    __HAL_RCC_SAI1_RELEASE_RESET();
    __DSB();
    __ISB();
    __HAL_RCC_SAI1_CONFIG(RCC_SAI1CLKSOURCE_HSI);
    __DSB();
    __ISB();
    g_ps_hw6_clock_policy_probe.sai_mux_park_clock_enabled =
      (__HAL_RCC_SAI1_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
    g_ps_hw6_clock_policy_probe.sai_mux_park_reset_asserted =
      (READ_BIT(RCC->APB2RSTR, RCC_APB2RSTR_SAI1RST) != 0U) ? 1UL : 0UL;
    g_ps_hw6_clock_policy_probe.sai_mux_park_source =
      __HAL_RCC_GET_SAI1_SOURCE();
    g_ps_hw6_clock_policy_probe.sai_mux_park_kernel_hz =
      HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1);
    if ((g_ps_hw6_clock_policy_probe.sai_mux_park_clock_enabled != 0UL) ||
        (g_ps_hw6_clock_policy_probe.sai_mux_park_reset_asserted != 0UL) ||
        (g_ps_hw6_clock_policy_probe.sai_mux_park_source !=
         RCC_SAI1CLKSOURCE_HSI) ||
        (g_ps_hw6_clock_policy_probe.sai_mux_park_kernel_hz == 0UL))
    {
      status = HAL_ERROR;
    }
  }

  if (status != HAL_OK)
  {
    g_ps_hw6_clock_policy_probe.sai_mux_handoff_status =
      (uint32_t)TX_NOT_DONE;
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_ClockPolicy_VerifySaiMuxPll2(void)
{
  HAL_StatusTypeDef status = HAL_OK;

  g_ps_hw6_clock_policy_probe.sai_mux_restore_clock_enabled =
    (__HAL_RCC_SAI1_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.sai_mux_restore_reset_asserted =
    (READ_BIT(RCC->APB2RSTR, RCC_APB2RSTR_SAI1RST) != 0U) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.sai_mux_restore_source =
    __HAL_RCC_GET_SAI1_SOURCE();
  g_ps_hw6_clock_policy_probe.sai_mux_restore_kernel_hz =
    HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1);
  if ((g_ps_hw6_clock_policy_probe.sai_mux_restore_clock_enabled != 0UL) ||
      (g_ps_hw6_clock_policy_probe.sai_mux_restore_reset_asserted != 0UL) ||
      (g_ps_hw6_clock_policy_probe.sai_mux_restore_source !=
       RCC_SAI1CLKSOURCE_PLL2) ||
      (g_ps_hw6_clock_policy_probe.sai_mux_restore_kernel_hz == 0UL))
  {
    status = HAL_ERROR;
    g_ps_hw6_clock_policy_probe.sai_mux_handoff_status =
      (uint32_t)TX_NOT_DONE;
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_ClockPolicy_EnablePll2ForDomains(
  const RCC_PLL2InitTypeDef *pll2,
  uint32_t output_mask)
{
  RCC_PeriphCLKInitTypeDef peripheral_clock = {0};

  if ((output_mask & RCC_PLL2_DIVP) == 0UL)
  {
    return HAL_RCCEx_EnablePLL2(pll2);
  }

  /* Use the same PLL2P bring-up path that establishes SAI1 at boot. */
  peripheral_clock.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
  peripheral_clock.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL2;
  peripheral_clock.PLL2 = *pll2;
  return HAL_RCCEx_PeriphCLKConfig(&peripheral_clock);
}

static UINT PS_HW6_ClockPolicy_SaiDomainSet(UINT enabled,
                                             UINT reset_on_enable)
{
  UINT status = TX_SUCCESS;
  UINT clock_was_enabled =
    (__HAL_RCC_SAI1_IS_CLK_ENABLED() != 0U) ? 1U : 0U;

  if (enabled != 0U)
  {
    g_ps_hw6_clock_policy_probe.last_stage =
      PS_HW6_CLOCK_STAGE_SAI_DOMAIN_ON;
    if ((__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) == 0U) ||
        ((READ_BIT(RCC->PLL2CFGR, RCC_PLL2CFGR_PLL2PEN)) == 0U) ||
        (HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1) == 0UL))
    {
      status = TX_NOT_DONE;
    }
    if ((status == TX_SUCCESS) && (clock_was_enabled == 0U))
    {
      __HAL_RCC_SAI1_CLK_ENABLE();
      __DSB();
      __ISB();
    }
    if ((status == TX_SUCCESS) &&
        ((clock_was_enabled == 0U) || (reset_on_enable != 0U)))
    {
      __HAL_RCC_SAI1_FORCE_RESET();
      __DSB();
      __ISB();
      __HAL_RCC_SAI1_RELEASE_RESET();
      __DSB();
      __ISB();
      g_ps_hw6_clock_policy_probe.sai_reset_count++;
      g_ps_hw6_clock_policy_probe.sai_grant_epoch++;
    }
    if ((status == TX_SUCCESS) &&
        ((__HAL_RCC_SAI1_IS_CLK_ENABLED() == 0U) ||
         (READ_BIT(RCC->APB2RSTR, RCC_APB2RSTR_SAI1RST) != 0U)))
    {
      status = TX_NOT_DONE;
    }
    if (status == TX_SUCCESS)
    {
      if ((clock_was_enabled == 0U) || (reset_on_enable != 0U))
      {
        g_ps_hw6_clock_policy_probe.sai_domain_on_count++;
      }
      g_ps_hw6_clock_policy_probe.sai_domain_active = 1UL;
      g_ps_hw6_clock_policy_probe.managed_domain_mask |=
        PS_HW6_CLOCK_DOMAIN_PLL2_SAI;
    }
  }
  else
  {
    g_ps_hw6_clock_policy_probe.last_stage =
      PS_HW6_CLOCK_STAGE_SAI_DOMAIN_OFF;
    if (clock_was_enabled != 0U)
    {
      __HAL_RCC_SAI1_CLK_DISABLE();
      __DSB();
      __ISB();
      g_ps_hw6_clock_policy_probe.sai_domain_off_count++;
    }
    if (__HAL_RCC_SAI1_IS_CLK_ENABLED() != 0U)
    {
      status = TX_NOT_DONE;
    }
    else
    {
      g_ps_hw6_clock_policy_probe.sai_domain_active = 0UL;
    }
  }

  g_ps_hw6_clock_policy_probe.sai_domain_last_status = (uint32_t)status;
  return status;
}

static UINT PS_HW6_ClockPolicy_Pll2DomainSet(
  uint32_t required_domain_mask,
  UINT force_disable_when_unused)
{
  RCC_PLL2InitTypeDef pll2 = {0};
  HAL_StatusTypeDef hal_status = HAL_OK;
  UINT sai_status;
  uint32_t output_mask = PS_HW6_ClockPolicy_Pll2OutputMaskForDomains(
    required_domain_mask);
  uint32_t enabled_output_mask = READ_BIT(
    RCC->PLL2CFGR,
    RCC_PLL2CFGR_PLL2PEN | RCC_PLL2CFGR_PLL2QEN |
    RCC_PLL2CFGR_PLL2REN);
  uint32_t outputs_are_active =
    ((__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) != 0U) &&
     (enabled_output_mask == output_mask)) ? 1UL : 0UL;
  uint32_t post_stop_stale_domain_mask =
    required_domain_mask &
    g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_pending_domain_mask &
    PS_HW6_CLOCK_PLL2_DOMAIN_MASK;
  uint32_t force_post_stop_rearm =
    ((output_mask != 0UL) && (post_stop_stale_domain_mask != 0UL)) ?
    1UL : 0UL;
  uint32_t sai_mux_parked = 0UL;

  g_ps_hw6_clock_policy_probe.pll2_required_output_mask = output_mask;

  if ((output_mask != 0UL) &&
      (force_post_stop_rearm == 0UL) &&
      (outputs_are_active != 0UL) &&
      (((output_mask & RCC_PLL2_DIVP) == 0UL) ||
       (HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1) != 0UL)) &&
      (((output_mask & RCC_PLL2_DIVQ) == 0UL) ||
       (HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_OSPI) != 0UL)))
  {
    sai_status = PS_HW6_ClockPolicy_SaiDomainSet(
      ((required_domain_mask & PS_HW6_CLOCK_DOMAIN_PLL2_SAI) != 0UL) ?
      1U : 0U,
      0U);
    if (sai_status != TX_SUCCESS)
    {
      g_ps_hw6_clock_policy_probe.pll2_domain_last_status =
        (uint32_t)TX_NOT_DONE;
      return TX_NOT_DONE;
    }
    g_ps_hw6_clock_policy_probe.pll2_fast_path_count++;
    g_ps_hw6_clock_policy_probe.managed_domain_mask |=
      required_domain_mask & PS_HW6_CLOCK_PLL2_DOMAIN_MASK;
    g_ps_hw6_clock_policy_probe.pll2_domain_last_status =
      (uint32_t)TX_SUCCESS;
    return TX_SUCCESS;
  }

  sai_status = PS_HW6_ClockPolicy_SaiDomainSet(0U, 0U);
  if (sai_status != TX_SUCCESS)
  {
    g_ps_hw6_clock_policy_probe.pll2_domain_last_status =
      (uint32_t)TX_NOT_DONE;
    return TX_NOT_DONE;
  }

  if (output_mask != 0UL)
  {
    PS_HW6_ClockPolicy_FillPll2Config(&pll2, output_mask);
    if (force_post_stop_rearm != 0UL)
    {
      g_ps_hw6_clock_policy_probe.last_stage =
        PS_HW6_CLOCK_STAGE_PLL2_REARM;
      g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_attempt_count++;
      g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_status =
        PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
    }
    else
    {
      g_ps_hw6_clock_policy_probe.last_stage =
        PS_HW6_CLOCK_STAGE_PLL2_DOMAIN_ON;
    }
    if ((force_post_stop_rearm != 0UL) &&
        ((post_stop_stale_domain_mask &
          PS_HW6_CLOCK_DOMAIN_PLL2_SAI) != 0UL) &&
        ((output_mask & RCC_PLL2_DIVP) != 0UL))
    {
      hal_status = PS_HW6_ClockPolicy_SaiMuxParkOnHsi();
      if (hal_status == HAL_OK)
      {
        sai_mux_parked = 1UL;
      }
    }
    if ((hal_status == HAL_OK) &&
        ((output_mask & RCC_PLL2_DIVP) == 0UL))
    {
      hal_status = HAL_RCCEx_DisablePLL2();
    }
    if (hal_status == HAL_OK)
    {
      hal_status = PS_HW6_ClockPolicy_EnablePll2ForDomains(&pll2,
                                                           output_mask);
    }
    if (hal_status == HAL_OK)
    {
      if ((output_mask & RCC_PLL2_DIVP) != 0UL)
      {
        hal_status = PS_HW6_ClockPolicy_VerifySaiMuxPll2();
      }
      if ((hal_status == HAL_OK) &&
          ((output_mask & RCC_PLL2_DIVQ) != 0UL))
      {
        __HAL_RCC_OSPI_CONFIG(RCC_OSPICLKSOURCE_PLL2);
      }
      if (hal_status == HAL_OK)
      {
        enabled_output_mask = READ_BIT(
          RCC->PLL2CFGR,
          RCC_PLL2CFGR_PLL2PEN | RCC_PLL2CFGR_PLL2QEN |
          RCC_PLL2CFGR_PLL2REN);
        if ((__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) == 0U) ||
            (enabled_output_mask != output_mask) ||
            (((output_mask & RCC_PLL2_DIVP) != 0UL) &&
             (HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1) == 0UL)) ||
            (((output_mask & RCC_PLL2_DIVQ) != 0UL) &&
             (HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_OSPI) == 0UL)))
        {
          hal_status = HAL_ERROR;
        }
      }
    }
    if ((hal_status == HAL_OK) &&
        ((required_domain_mask & PS_HW6_CLOCK_DOMAIN_PLL2_SAI) != 0UL) &&
        (PS_HW6_ClockPolicy_SaiDomainSet(1U, 1U) != TX_SUCCESS))
    {
      hal_status = HAL_ERROR;
    }
    if (hal_status == HAL_OK)
    {
      if (sai_mux_parked != 0UL)
      {
        g_ps_hw6_clock_policy_probe.sai_mux_handoff_success_count++;
        g_ps_hw6_clock_policy_probe.sai_mux_handoff_status =
          (uint32_t)TX_SUCCESS;
      }
      g_ps_hw6_clock_policy_probe.pll2_domain_on_count++;
      g_ps_hw6_clock_policy_probe.managed_domain_mask |=
        required_domain_mask & PS_HW6_CLOCK_PLL2_DOMAIN_MASK;
      if (force_post_stop_rearm != 0UL)
      {
        g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_pending_domain_mask &=
          ~post_stop_stale_domain_mask;
        g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_pending =
          (g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_pending_domain_mask !=
           0UL) ? 1UL : 0UL;
        g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_success_count++;
        g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_status =
          (uint32_t)TX_SUCCESS;
      }
    }
    else if (force_post_stop_rearm != 0UL)
    {
      g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_status =
        (uint32_t)TX_NOT_DONE;
      g_ps_hw6_clock_policy_probe.sai_mux_handoff_status =
        (uint32_t)TX_NOT_DONE;
    }
  }
  else if ((g_ps_hw6_clock_policy_probe.pll2_autogate_enabled != 0UL) ||
           (force_disable_when_unused != 0U))
  {
    g_ps_hw6_clock_policy_probe.last_stage =
      PS_HW6_CLOCK_STAGE_PLL2_DOMAIN_OFF;
    hal_status = HAL_RCCEx_DisablePLL2();
    if ((hal_status == HAL_OK) &&
        ((__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) != 0U) ||
         (READ_BIT(RCC->PLL2CFGR,
                   RCC_PLL2CFGR_PLL2PEN | RCC_PLL2CFGR_PLL2QEN |
                   RCC_PLL2CFGR_PLL2REN) != 0U)))
    {
      hal_status = HAL_ERROR;
    }
    if (hal_status == HAL_OK)
    {
      g_ps_hw6_clock_policy_probe.pll2_domain_off_count++;
    }
  }

  g_ps_hw6_clock_policy_probe.pll2_domain_last_status =
    (hal_status == HAL_OK) ? (uint32_t)TX_SUCCESS : (uint32_t)TX_NOT_DONE;
  return (hal_status == HAL_OK) ? TX_SUCCESS : TX_NOT_DONE;
}

static UINT PS_HW6_ClockPolicy_ApplyIoHigh(uint32_t required_domain_mask)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};
  HAL_StatusTypeDef hal_status;
  UINT tx_status;

  if ((uint32_t)KNOB_POWER_CLOCK_IO_HIGH_HZ != 160000000UL)
  {
    return TX_NOT_DONE;
  }

  if (HAL_RCC_GetSysClockFreq() < 160000000UL)
  {
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                    RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    clk.APB3CLKDivider = RCC_HCLK_DIV8;
    hal_status = HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4);
    if (hal_status != HAL_OK)
    {
      return TX_NOT_DONE;
    }

    hal_status = HAL_PWREx_ControlVoltageScaling(
      PWR_REGULATOR_VOLTAGE_SCALE1);
    if (hal_status != HAL_OK)
    {
      return TX_NOT_DONE;
    }

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_OFF;
    hal_status = HAL_RCC_OscConfig(&osc);
    if (hal_status != HAL_OK)
    {
      return TX_NOT_DONE;
    }

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 1;
    osc.PLL.PLLN = 20;
    osc.PLL.PLLP = 2;
    osc.PLL.PLLQ = 2;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
    osc.PLL.PLLFRACN = 0;
    osc.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
    hal_status = HAL_RCC_OscConfig(&osc);
    if (hal_status != HAL_OK)
    {
      return TX_NOT_DONE;
    }

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    hal_status = HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4);
    if (hal_status != HAL_OK)
    {
      return TX_NOT_DONE;
    }
  }

  g_ps_hw6_clock_policy_probe.last_stage =
    PS_HW6_CLOCK_STAGE_SYSTICK;
  tx_status = PS_HW6_ClockPolicy_RetuneThreadXSysTick();
  if (tx_status != TX_SUCCESS)
  {
    return tx_status;
  }

  if ((required_domain_mask & PS_HW6_CLOCK_DOMAIN_USB_DEVICE) != 0UL)
  {
    g_ps_hw6_clock_policy_probe.last_stage =
      PS_HW6_CLOCK_STAGE_USB_DOMAIN_ON;
    tx_status = PS_HW6_ClockPolicy_UsbDomainSet(1U);
    if (tx_status == TX_SUCCESS)
    {
      g_ps_hw6_clock_policy_probe.managed_domain_mask |=
        PS_HW6_CLOCK_DOMAIN_USB_DEVICE;
    }
    return tx_status;
  }

  g_ps_hw6_clock_policy_probe.last_stage =
    PS_HW6_CLOCK_STAGE_USB_DOMAIN_OFF;
  return PS_HW6_ClockPolicy_UsbDomainSet(0U);
}

static UINT PS_HW6_ClockPolicy_RestorePostStopBaseClocks(void)
{
  RCC_OscInitTypeDef oscillator = {0};
  RCC_ClkInitTypeDef clock = {0};
  HAL_StatusTypeDef hal_status;

  if ((uint32_t)KNOB_POWER_CLOCK_REACTIVE_BASE_HZ != 24000000UL)
  {
    return TX_NOT_DONE;
  }

  g_ps_hw6_clock_policy_probe.last_stage =
    PS_HW6_CLOCK_STAGE_RESTORE_BASE;

  g_ps_hw6_clock_policy_probe.post_stop_vosr_before = PWR->VOSR;
  g_ps_hw6_clock_policy_probe.post_stop_svmsr_before = PWR->SVMSR;
  hal_status = HAL_PWREx_ControlVoltageScaling(
    PWR_REGULATOR_VOLTAGE_SCALE3);
  g_ps_hw6_clock_policy_probe.post_stop_voltage_scale_status =
    (uint32_t)hal_status;
  g_ps_hw6_clock_policy_probe.post_stop_vosr_after = PWR->VOSR;
  g_ps_hw6_clock_policy_probe.post_stop_svmsr_after = PWR->SVMSR;
  if ((hal_status != HAL_OK) ||
      (HAL_PWREx_GetVoltageRange() != PWR_REGULATOR_VOLTAGE_SCALE3) ||
      (HAL_IS_BIT_CLR(PWR->VOSR, PWR_VOSR_VOSRDY)) ||
      (HAL_IS_BIT_CLR(PWR->SVMSR, PWR_SVMSR_ACTVOSRDY)))
  {
    return TX_NOT_DONE;
  }

  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSI |
                              RCC_OSCILLATORTYPE_MSI |
                              RCC_OSCILLATORTYPE_MSIK;
  oscillator.HSIState = RCC_HSI_ON;
  oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscillator.MSIState = RCC_MSI_ON;
  oscillator.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  oscillator.MSIClockRange = RCC_MSIRANGE_1;
  oscillator.MSIKClockRange = RCC_MSIKRANGE_4;
  oscillator.MSIKState = RCC_MSIK_ON;
  oscillator.PLL.PLLState = RCC_PLL_NONE;
  hal_status = HAL_RCC_OscConfig(&oscillator);
  if (hal_status != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  clock.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                    RCC_CLOCKTYPE_PCLK3;
  clock.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  clock.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clock.APB1CLKDivider = RCC_HCLK_DIV2;
  clock.APB2CLKDivider = RCC_HCLK_DIV2;
  clock.APB3CLKDivider = RCC_HCLK_DIV8;
  hal_status = HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_0);
  if (hal_status != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  __HAL_RCC_MSIKSTOP_ENABLE();
  return (HAL_RCC_GetSysClockFreq() ==
          (uint32_t)KNOB_POWER_CLOCK_REACTIVE_BASE_HZ) ?
         TX_SUCCESS : TX_NOT_DONE;
}

static UINT PS_HW6_ClockPolicy_ApplyBase(uint32_t selected_profile)
{
  UINT status;

  (void)selected_profile;
  g_ps_hw6_clock_policy_probe.last_stage =
    PS_HW6_CLOCK_STAGE_USB_DOMAIN_OFF;
  status = PS_HW6_ClockPolicy_UsbDomainSet(0U);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  g_ps_hw6_clock_policy_probe.last_stage =
    PS_HW6_CLOCK_STAGE_RESTORE_BASE;
  /*
   * Runtime clock release must not re-run Cube's global clock setup. USB
   * reclaim can call this path while middleware teardown is still unwinding;
   * keep the release bounded to owned domains and leave full restore to boot.
   */

  g_ps_hw6_clock_policy_probe.last_stage =
    PS_HW6_CLOCK_STAGE_SYSTICK;
  status = PS_HW6_ClockPolicy_RetuneThreadXSysTick();
  if (status == TX_SUCCESS)
  {
    g_ps_hw6_clock_policy_probe.restore_count++;
  }
  return status;
}

static UINT PS_HW6_ClockPolicy_ApplyResolvedProfile(
  uint32_t requested_profile,
  uint32_t capabilities)
{
  UINT status;
  uint32_t selected_profile = requested_profile;
  uint32_t required_domain_mask = 0UL;

  g_ps_hw6_clock_policy_probe.sysclk_before_hz =
    HAL_RCC_GetSysClockFreq();
  g_ps_hw6_clock_policy_probe.hclk_before_hz =
    HAL_RCC_GetHCLKFreq();
  g_ps_hw6_clock_policy_probe.requested_profile = requested_profile;
  g_ps_hw6_clock_policy_probe.managed_domain_mask = 0UL;
  g_ps_hw6_clock_policy_probe.last_stage = PS_HW6_CLOCK_STAGE_RESOLVE;
  g_ps_hw6_clock_policy_probe.last_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  g_ps_hw6_clock_policy_probe.last_tick = (uint32_t)tx_time_get();
  g_ps_hw6_clock_policy_probe.apply_count++;

  if ((capabilities & ~PS_HW6_CLOCK_CAP_ALL) != 0UL)
  {
    status = TX_NOT_DONE;
  }
  else
  {
    if (requested_profile == (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN)
    {
      selected_profile = PS_HW6_ClockPolicy_SelectProfile(capabilities);
    }
    g_ps_hw6_clock_policy_probe.selected_profile = selected_profile;
    PS_HW6_ClockPolicy_UpdateResolverProbe(capabilities, selected_profile);
    required_domain_mask =
      g_ps_hw6_clock_policy_probe.required_domain_mask;

    if (selected_profile == (uint32_t)PS_HW6_CLOCK_PROFILE_IO_HIGH)
    {
      g_ps_hw6_clock_policy_probe.last_stage =
        PS_HW6_CLOCK_STAGE_SYSCLK_IO_HIGH;
      status = PS_HW6_ClockPolicy_ApplyIoHigh(required_domain_mask);
      if (status == TX_SUCCESS)
      {
        g_ps_hw6_clock_policy_probe.current_profile = selected_profile;
      }
    }
    else if ((selected_profile ==
              (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE) ||
             (selected_profile ==
              (uint32_t)PS_HW6_CLOCK_PROFILE_BOOT_RECOVERY) ||
             (selected_profile ==
              (uint32_t)PS_HW6_CLOCK_PROFILE_STOP_PREP))
    {
      status = PS_HW6_ClockPolicy_ApplyBase(selected_profile);
      if (status == TX_SUCCESS)
      {
        g_ps_hw6_clock_policy_probe.current_profile = selected_profile;
      }
    }
    else if ((selected_profile ==
              (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BURST) ||
             (selected_profile ==
              (uint32_t)PS_HW6_CLOCK_PROFILE_REALTIME_BALANCED))
    {
      status = PS_HW6_ClockPolicy_ApplyBase(
        (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE);
      if (status == TX_SUCCESS)
      {
        g_ps_hw6_clock_policy_probe.current_profile =
          (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE;
      }
    }
    else
    {
      status = TX_NOT_DONE;
    }
  }

  if (status == TX_SUCCESS)
  {
    status = PS_HW6_ClockPolicy_Pll2DomainSet(
      required_domain_mask,
      (selected_profile == (uint32_t)PS_HW6_CLOCK_PROFILE_STOP_PREP) ?
      1U : 0U);
  }

  g_ps_hw6_clock_policy_probe.last_status = (uint32_t)status;
  g_ps_hw6_clock_policy_probe.last_stage =
    (status == TX_SUCCESS) ? PS_HW6_CLOCK_STAGE_COMPLETE :
                             g_ps_hw6_clock_policy_probe.last_stage;
  g_ps_hw6_clock_policy_probe.last_tick = (uint32_t)tx_time_get();
  PS_HW6_ClockPolicy_RecordSnapshot();
  PS_HW6_ClockPolicy_UpdatePostSnapshotProbe();
  PS_HW6_TraceClockPolicy(selected_profile,
                          capabilities,
                          (uint32_t)status,
                          g_ps_hw6_clock_policy_probe.sysclk_after_hz);
  return status;
}

static void PS_HW6_ClockPolicy_ClearRequesterCapabilities(void)
{
  uint32_t index;

  for (index = 0U; index < PS_HW6_CLOCK_REQUESTER_COUNT; ++index)
  {
    g_ps_hw6_clock_policy_probe.requester_capabilities[index] = 0UL;
    g_ps_hw6_clock_policy_probe.requester_status[index] =
      PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  }
  g_ps_hw6_clock_policy_probe.requester_active_mask = 0UL;
  g_ps_hw6_clock_policy_probe.aggregated_capabilities = 0UL;
}

void PS_HW6_ClockPolicy_Reset(void)
{
  (void)memset((void *)&g_ps_hw6_clock_policy_probe, 0,
               sizeof(g_ps_hw6_clock_policy_probe));
  PS_HW6_ClockPolicy_SetStaticProbeFields();
  g_ps_hw6_clock_policy_probe.requested_profile =
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN;
  g_ps_hw6_clock_policy_probe.selected_profile =
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN;
  g_ps_hw6_clock_policy_probe.current_profile =
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN;
  g_ps_hw6_clock_policy_probe.last_stage = PS_HW6_CLOCK_STAGE_IDLE;
  g_ps_hw6_clock_policy_probe.last_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  g_ps_hw6_clock_policy_probe.pll2_domain_last_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  g_ps_hw6_clock_policy_probe.sai_mux_handoff_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  g_ps_hw6_clock_policy_probe.sai_domain_last_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  g_ps_hw6_clock_policy_probe.stop2_prepare_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  PS_HW6_ClockPolicy_ClearRequesterStatus();
  PS_HW6_ClockPolicy_RecordSnapshot();
  PS_HW6_ClockPolicy_UpdateResolverProbe(0UL,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN);
}

void PS_HW6_ClockPolicy_RecordHardwareSnapshot(void)
{
  PS_HW6_ClockPolicy_PrimeProbe();
  PS_HW6_ClockPolicy_RecordSnapshot();
}

uint32_t PS_HW6_ClockPolicy_SelectProfile(uint32_t capabilities)
{
  if ((capabilities & PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE) != 0UL)
  {
    return (uint32_t)PS_HW6_CLOCK_PROFILE_IO_HIGH;
  }
  if ((capabilities & (PS_HW6_CLOCK_CAP_SAI_AUDIO_ACTIVE |
                       PS_HW6_CLOCK_CAP_REALTIME_DEADLINE_ACTIVE)) != 0UL)
  {
    return (uint32_t)PS_HW6_CLOCK_PROFILE_REALTIME_BALANCED;
  }
  if ((capabilities & (PS_HW6_CLOCK_CAP_OCTOSPI_ACTIVE |
                       PS_HW6_CLOCK_CAP_DISPLAY_TRANSFER_ACTIVE |
                       PS_HW6_CLOCK_CAP_REACTIVE_TRANSACTION_ACTIVE)) != 0UL)
  {
    return (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BURST;
  }

  return (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE;
}

uint32_t PS_HW6_ClockPolicy_ProfileIsActive(
  uint32_t profile,
  uint32_t required_capabilities)
{
  PS_HW6_ClockPolicy_PrimeProbe();

  return ((g_ps_hw6_clock_policy_probe.current_profile == profile) &&
          ((g_ps_hw6_clock_policy_probe.active_capabilities &
            required_capabilities) == required_capabilities) &&
          (g_ps_hw6_clock_policy_probe.last_status == TX_SUCCESS)) ?
         1UL : 0UL;
}

UINT PS_HW6_ClockPolicy_ApplyProfile(
  uint32_t requested_profile,
  uint32_t capabilities)
{
  return PS_HW6_ClockPolicy_ApplyRequesterProfile(
    0UL,
    requested_profile,
    capabilities);
}

UINT PS_HW6_ClockPolicy_ApplyBootIdleDomains(void)
{
  UINT status;

  PS_HW6_ClockPolicy_PrimeProbe();
  PS_HW6_ClockPolicy_ClearRequesterCapabilities();
  g_ps_hw6_clock_policy_probe.sysclk_before_hz =
    HAL_RCC_GetSysClockFreq();
  g_ps_hw6_clock_policy_probe.hclk_before_hz =
    HAL_RCC_GetHCLKFreq();
  g_ps_hw6_clock_policy_probe.requested_profile =
    (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE;
  g_ps_hw6_clock_policy_probe.selected_profile =
    (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE;
  g_ps_hw6_clock_policy_probe.managed_domain_mask = 0UL;
  g_ps_hw6_clock_policy_probe.last_stage = PS_HW6_CLOCK_STAGE_RESOLVE;
  g_ps_hw6_clock_policy_probe.last_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  g_ps_hw6_clock_policy_probe.last_tick = (uint32_t)tx_time_get();
  g_ps_hw6_clock_policy_probe.apply_count++;
  PS_HW6_ClockPolicy_UpdateResolverProbe(
    0UL,
    (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE);

  if (HAL_RCC_GetSysClockFreq() !=
      (uint32_t)KNOB_POWER_CLOCK_REACTIVE_BASE_HZ)
  {
    status = TX_NOT_DONE;
  }
  else
  {
    g_ps_hw6_clock_policy_probe.last_stage =
      PS_HW6_CLOCK_STAGE_USB_DOMAIN_OFF;
    status = PS_HW6_ClockPolicy_UsbDomainSet(0U);
    if (status == TX_SUCCESS)
    {
      status = PS_HW6_ClockPolicy_Pll2DomainSet(0UL, 1U);
    }
  }

  g_ps_hw6_clock_policy_probe.last_status = (uint32_t)status;
  g_ps_hw6_clock_policy_probe.last_stage =
    (status == TX_SUCCESS) ? PS_HW6_CLOCK_STAGE_COMPLETE :
                             g_ps_hw6_clock_policy_probe.last_stage;
  if (status == TX_SUCCESS)
  {
    g_ps_hw6_clock_policy_probe.current_profile =
      (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE;
  }
  g_ps_hw6_clock_policy_probe.last_tick = (uint32_t)tx_time_get();
  PS_HW6_ClockPolicy_RecordSnapshot();
  PS_HW6_ClockPolicy_UpdatePostSnapshotProbe();
  PS_HW6_TraceClockPolicy(
    (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE,
    0UL,
    (uint32_t)status,
    g_ps_hw6_clock_policy_probe.sysclk_after_hz);
  return status;
}

UINT PS_HW6_ClockPolicy_ApplyRequesterProfile(
  uint32_t requester_id,
  uint32_t requested_profile,
  uint32_t capabilities)
{
  uint32_t aggregated_capabilities;
  UINT status;

  PS_HW6_ClockPolicy_PrimeProbe();
  g_ps_hw6_clock_policy_probe.last_stage =
    PS_HW6_CLOCK_STAGE_REQUESTER_UPDATE;

  if ((requester_id >= PS_HW6_CLOCK_REQUESTER_COUNT) ||
      (requested_profile > (uint32_t)PS_HW6_CLOCK_PROFILE_STOP_PREP) ||
      ((capabilities & ~PS_HW6_CLOCK_CAP_ALL) != 0UL))
  {
    g_ps_hw6_clock_policy_probe.last_status = (uint32_t)TX_NOT_DONE;
    if (requester_id < PS_HW6_CLOCK_REQUESTER_COUNT)
    {
      g_ps_hw6_clock_policy_probe.requester_status[requester_id] =
        (uint32_t)TX_NOT_DONE;
    }
    return TX_NOT_DONE;
  }

  g_ps_hw6_clock_policy_probe.requester_capabilities[requester_id] =
    capabilities;
  aggregated_capabilities =
    PS_HW6_ClockPolicy_AggregateRequesterCapabilities();
  status = PS_HW6_ClockPolicy_ApplyResolvedProfile(requested_profile,
                                                   aggregated_capabilities);
  g_ps_hw6_clock_policy_probe.requester_status[requester_id] =
    (uint32_t)status;
  return status;
}

static uint32_t PS_HW6_ClockPolicy_Stop2PhysicalFailureMask(void)
{
  uint32_t failure_mask = 0UL;

  if (g_ps_hw6_clock_policy_probe.sai_clock_enabled != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_SAI_GATE;
  }
  if (g_ps_hw6_clock_policy_probe.sai_reset_asserted != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_SAI_RESET;
  }
  if (g_ps_hw6_clock_policy_probe.pll2_ready != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_PLL2_READY;
  }
  if (g_ps_hw6_clock_policy_probe.pll2_output_enabled_mask != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_PLL2_OUTPUT;
  }
  if (g_ps_hw6_clock_policy_probe.pll3_ready != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_PLL3_READY;
  }
  if (g_ps_hw6_clock_policy_probe.hsi48_ready != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_HSI48_READY;
  }
  if (g_ps_hw6_clock_policy_probe.shsi_ready != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_SHSI_READY;
  }
  if (g_ps_hw6_clock_policy_probe.usb_clock_enabled != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_USB_GATE;
  }
  if (g_ps_hw6_clock_policy_probe.vddusb_enabled != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_VDDUSB;
  }

  return failure_mask;
}

UINT PS_HW6_ClockPolicy_PrepareStop2(void)
{
  uint32_t capabilities;
  uint32_t failure_mask = 0UL;
  UINT status;

  PS_HW6_ClockPolicy_PrimeProbe();
  g_ps_hw6_clock_policy_probe.stop2_prepare_count++;
  g_ps_hw6_clock_policy_probe.stop2_prepare_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  g_ps_hw6_clock_policy_probe.stop2_physical_ready = 0UL;
  g_ps_hw6_clock_policy_probe.stop2_physical_failure_mask = 0UL;

  capabilities = PS_HW6_ClockPolicy_AggregateRequesterCapabilities();
  PS_HW6_ClockPolicy_UpdateResolverProbe(
    capabilities,
    (uint32_t)PS_HW6_CLOCK_PROFILE_STOP_PREP);
  if ((capabilities & PS_HW6_CLOCK_STOP2_BLOCKER_CAP_MASK) != 0UL)
  {
    failure_mask |= PS_HW6_CLOCK_STOP2_FAIL_REQUESTERS_ACTIVE;
    status = TX_NOT_DONE;
    PS_HW6_ClockPolicy_RecordSnapshot();
    failure_mask |= PS_HW6_ClockPolicy_Stop2PhysicalFailureMask();
  }
  else
  {
    status = PS_HW6_ClockPolicy_ApplyResolvedProfile(
      (uint32_t)PS_HW6_CLOCK_PROFILE_STOP_PREP,
      capabilities);
    PS_HW6_ClockPolicy_RecordSnapshot();
    failure_mask |= PS_HW6_ClockPolicy_Stop2PhysicalFailureMask();
    if (failure_mask != 0UL)
    {
      status = TX_NOT_DONE;
    }
  }

  g_ps_hw6_clock_policy_probe.last_stage =
    PS_HW6_CLOCK_STAGE_STOP2_VERIFY;
  g_ps_hw6_clock_policy_probe.stop2_physical_failure_mask = failure_mask;
  g_ps_hw6_clock_policy_probe.stop2_physical_ready =
    ((status == TX_SUCCESS) && (failure_mask == 0UL)) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.stop2_ready =
    g_ps_hw6_clock_policy_probe.stop2_physical_ready;
  g_ps_hw6_clock_policy_probe.lpbam_stop2_ready =
    (((capabilities & PS_HW6_CLOCK_CAP_LPBAM_DISPLAY_AUTONOMOUS) != 0UL) &&
     (g_ps_hw6_clock_policy_probe.stop2_physical_ready != 0UL)) ? 1UL : 0UL;
  g_ps_hw6_clock_policy_probe.stop2_prepare_status = (uint32_t)status;
  g_ps_hw6_clock_policy_probe.last_status = (uint32_t)status;
  g_ps_hw6_clock_policy_probe.last_tick = (uint32_t)tx_time_get();
  return status;
}

UINT PS_HW6_ClockPolicy_RestoreBase(void)
{
  UINT status;

  PS_HW6_ClockPolicy_PrimeProbe();
  g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_pending = 1UL;
  g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_pending_domain_mask =
    PS_HW6_CLOCK_PLL2_DOMAIN_MASK;
  g_ps_hw6_clock_policy_probe.pll2_post_stop_invalidate_count++;
  g_ps_hw6_clock_policy_probe.pll2_post_stop_rearm_status =
    PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN;
  status = PS_HW6_ClockPolicy_RestorePostStopBaseClocks();
  if (status != TX_SUCCESS)
  {
    g_ps_hw6_clock_policy_probe.last_status = (uint32_t)status;
    g_ps_hw6_clock_policy_probe.last_tick = (uint32_t)tx_time_get();
    PS_HW6_ClockPolicy_RecordSnapshot();
    return status;
  }
  PS_HW6_ClockPolicy_ClearRequesterCapabilities();
  return PS_HW6_ClockPolicy_ApplyResolvedProfile(
    (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE,
    0UL);
}
