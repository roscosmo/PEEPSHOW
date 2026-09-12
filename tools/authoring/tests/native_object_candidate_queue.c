#define PS_OBJECT_AWAKE_MAIN awake_main
#include "native_object_awake.c"
#define LS013B7DH05_H
#define LCD_DMA_MAX_ROWS_PER_TRANSFER 48U
#include "ps_lpbam_display_buffers.c"
#include "object_display_under_test.inc"
#define DISPLAY_RENDERER_H
#include "ps_scene_object_display_admission.c"
#include "ps_hw6_object_candidate.h"
#include "ps_package_workflow.h"

typedef uint32_t UINT;
typedef uint32_t ULONG;
#define TX_SUCCESS 0U
#define TX_CALLER_ERROR 1U
#define TX_AND_CLEAR 1U
#define TX_OR 2U
#define TX_NO_WAIT 0U
#define __DMB() ((void)0)
#define PS_HW6_RTOS_STATUS_NOT_RUN UINT32_MAX
#define PS_HW6_RTOS_EVENT_DEBUG_INDEX 3U
#define PS_HW6_RTOS_OWNER_DISPLAY 3U
#define PS_HW6_RTOS_MESSAGE_WORDS 4U
#define PS_HW6_RTOS_OBJECT_CANDIDATE_MAGIC 0x43414E32U
#define PS_HW6_RTOS_OBJECT_CANDIDATE_ACK (1U << 10)
#define PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS 1000U
#define PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER 1U
#define PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE 2U
#define PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES 42U
#define PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REACTIVE_TRANSACTION 1U
#define PS_HW6_RTOS_RUNTIME_CLOCK_REASON_RELEASE 2U
#define PS_HW6_RTOS_RUNTIME_CLOCK_REACTIVE_CAPABILITIES 43U
static uint32_t ps_event_groups[4], ps_queues[9];
static uint32_t ps_package_validation_busy;
static const uint8_t *ps_package_validation_blob;
static uint32_t ps_package_validation_size, ps_package_validation_status;
volatile ps_package_workflow_probe_t g_ps_package_workflow_probe;
static struct { uint32_t ospi_kernel_hz; } g_ps_hw6_clock_policy_probe;
static uint32_t HAL_RCC_GetHCLKFreq(void) { return 24000000U; }
static struct { uint32_t runtime_active_capabilities; } g_ps_hw6_rtos_probe;
static struct { uint32_t display_lpbam_active, display_lpbam_prearmed, display_complete; }
  g_ps_hw6_owner_probe = {0, 0, 1};
const uint8_t g_ps_object_development_egg[1] = {0};
const uint32_t g_ps_object_development_egg_size = 0;
#include "candidate_globals.inc"

static void PS_HW6_RTOS_CandidateDisplay(const ULONG *message);
static uint32_t send_status, wait_status, delivery = 1, ack_count, sends;
static uint32_t display_clock_failure, runtime_clock_failure;
static uint32_t display_release_failure, runtime_release_failure;
static uint32_t display_clock_calls, runtime_clock_calls;
static uint32_t inject_missing_sprite;
static ULONG queued[4];
static UINT PS_HW6_RTOS_RequestDisplayClockCapabilities(uint32_t reason, uint32_t caps)
{
  display_clock_calls++;
  assert((reason == 1 && caps == 42) || (reason == 2 && caps == 0));
  return reason == 1 ? display_clock_failure : display_release_failure;
}
static UINT PS_HW6_RTOS_RequestRuntimeClockCapabilities(uint32_t reason, uint32_t caps)
{
  runtime_clock_calls++;
  assert((reason == 1 && caps == 43) || (reason == 2 && caps == 0));
  return reason == 1 ? runtime_clock_failure : runtime_release_failure;
}
static UINT tx_event_flags_set(uint32_t *group, ULONG flag, UINT op)
{
  assert(group == &ps_event_groups[3] && flag == PS_HW6_RTOS_OBJECT_CANDIDATE_ACK && op == TX_OR);
  assert(g_ps_object_candidate_probe.display_complete == queued[2]);
  ack_count++;
  return TX_SUCCESS;
}
static UINT tx_queue_send(uint32_t *queue, const ULONG *message, ULONG wait)
{
  assert(queue == &ps_queues[3] && wait == TX_NO_WAIT);
  assert(message[0] == PS_HW6_RTOS_OBJECT_CANDIDATE_MAGIC && message[1] == 3);
  assert(message[2] == g_ps_object_candidate_probe.request_id && message[3] == ~message[2]);
  assert(ps_candidate_busy && ps_candidate_sent &&
    (ps_candidate_blob == candidate || ps_candidate_blob == ps_candidate_owned_bytes));
  memcpy(queued, message, sizeof(queued));
  sends++;
  if (inject_missing_sprite) { ps_candidate_catalog.frame_count = 0; }
  if (send_status == 0 && delivery == 2) { PS_HW6_RTOS_CandidateDisplay(queued); }
  return send_status;
}
static UINT tx_event_flags_get(uint32_t *group, ULONG flag, UINT op, ULONG *actual, ULONG wait)
{
  assert(group == &ps_event_groups[3] && flag == PS_HW6_RTOS_OBJECT_CANDIDATE_ACK && op == TX_AND_CLEAR);
  *actual = flag;
  if (wait == TX_NO_WAIT) { return TX_SUCCESS; }
  assert(wait == PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  if (delivery == 1) { PS_HW6_RTOS_CandidateDisplay(queued); }
  return wait_status;
}
#include "candidate_queue_under_test.inc"

static void complete(uint32_t result)
{
  assert(g_ps_object_candidate_probe.status == result);
  assert(g_ps_object_candidate_probe.request_id == g_ps_object_candidate_probe.display_complete);
  assert(g_ps_object_candidate_probe.leased == 0 && ps_candidate_busy == 0);
  assert(ps_candidate_blob == NULL && ps_candidate_size == 0 && ps_candidate_catalog.records == NULL);
}

static void workflow_test(uint32_t size)
{
  static ps_scene_object_graph_t saved_graph;
  static ps_egg_context_t saved_catalog;
  uint32_t token, sent;
  assert(PS_SceneRuntime_EnterDevelopmentObjects(baseline, baseline_size) == 0);
  saved_graph = s_ps_object_graph;
  saved_catalog = s_ps_egg_runtime_context;
  ps_package_validation_blob = candidate;
  ps_package_validation_size = size;
  g_ps_package_workflow_probe.active = 1;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 0 && g_ps_package_workflow_probe.validation_reason == 0);
  assert(g_ps_package_workflow_probe.validation_scene == 1);
  assert(g_ps_package_workflow_probe.phase_cpu_hz[PS_PACKAGE_WORKFLOW_VALIDATING] == 24000000U);

  sent = sends;
  candidate[size - 1] ^= 1; /* Same stored-digest bit as prepare_v2_rejection.py. */
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 1 && g_ps_package_workflow_probe.validation_reason == 4);
  assert(g_ps_package_workflow_probe.validation_scene == 0 && sends == sent && !ps_candidate_busy);
  candidate[size - 1] ^= 1;
  candidate[44] ^= 1;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 1 && g_ps_package_workflow_probe.validation_reason == 3);
  assert(sends == sent);
  candidate[44] ^= 1;

  token = g_ps_object_candidate_probe.request_id;
  ps_package_validation_size = PS_TARGET_PROFILE_PACKAGE_RESIDENT_BYTES + 1U;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(g_ps_package_workflow_probe.validation_reason == 13);
  assert(g_ps_package_workflow_probe.validation_scene == 0 && g_ps_object_candidate_probe.request_id == token);
  ps_package_validation_size = size;
  PS_HW6_RTOS_RunPackageValidation(9);
  assert(g_ps_package_workflow_probe.validation_reason == 15 && g_ps_package_workflow_probe.validation_scene == 0);
  assert(g_ps_object_candidate_probe.request_id == token);
  runtime_clock_failure = 5;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(g_ps_package_workflow_probe.validation_reason == 15 && sends == sent);
  runtime_clock_failure = 0;

  inject_missing_sprite = 1;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 1 && g_ps_package_workflow_probe.validation_reason == 11);
  assert(g_ps_object_candidate_probe.raster_status == 1 && !ps_candidate_busy);
  inject_missing_sprite = 0;
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 1;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 1 && g_ps_package_workflow_probe.validation_reason == UINT32_MAX);
  assert(g_ps_object_candidate_probe.display_status == 2);
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 0;

  delivery = 0; wait_status = 7;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_candidate_busy && g_ps_package_workflow_probe.validation_reason == UINT32_MAX);
  token = g_ps_object_candidate_probe.request_id;
  g_ps_egg_validation_probe.reason = 4; /* Refusal must not borrow stale detail. */
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(g_ps_object_candidate_probe.request_id == token);
  assert(g_ps_package_workflow_probe.validation_reason == UINT32_MAX && g_ps_package_workflow_probe.validation_scene == 0);
  PS_HW6_RTOS_CandidateDisplay(queued);
  PS_HW6_RTOS_CandidateReap();
  assert(!ps_candidate_busy && g_ps_package_workflow_probe.validation_reason == UINT32_MAX);
  delivery = 1; wait_status = 0;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 0 && g_ps_package_workflow_probe.validation_reason == 0);
  assert(g_ps_package_workflow_probe.validation_scene == 1);
  assert(memcmp(&saved_graph, &s_ps_object_graph, sizeof(saved_graph)) == 0);
  assert(memcmp(&saved_catalog, &s_ps_egg_runtime_context, sizeof(saved_catalog)) == 0);
  puts("workflow rejection reasons passed");
}

int main(int argc, char **argv)
{
  static ps_scene_object_graph_t active_before;
  static ps_egg_context_t catalog_before;
  static uint8_t framebuffer_before[sizeof(s_display_framebuffer)];
  static ps_object_waiting_program_t frozen;
  ps_egg_sprite_catalog_t borrowed;
  uint32_t size, count, token;
  ULONG stale[4];
  assert(argc == 2 || argc == 3);
  baseline_size = read_blob(argv[1], baseline);
  set_hash(argv[1], baseline, baseline_size);
  if (argc == 3)
  {
    size = read_blob(argv[1], candidate);
    if (strcmp(argv[2], "workflow") == 0) { workflow_test(size); return 0; }
    goto installed_tests;
  }
  assert(PS_SceneRuntime_EnterDevelopmentObjects(baseline, baseline_size) == 0);
  active_before = s_ps_object_graph;
  catalog_before = s_ps_egg_runtime_context;
  memset(s_display_framebuffer, 0x69, sizeof(s_display_framebuffer));
  memcpy(framebuffer_before, s_display_framebuffer, sizeof(framebuffer_before));
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);

  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  complete(0);
  assert(g_ps_object_candidate_probe.steps == 8 && g_ps_object_candidate_probe.quantum_ms == 400);
  assert(g_ps_object_candidate_probe.frames_composed == 9);
  assert(g_ps_object_candidate_probe.chunks == 16 && g_ps_object_candidate_probe.bytes == 9344);
  assert(display_clock_calls == 2 && runtime_clock_calls == 2 && ack_count == 1);

  PS_HW6_RTOS_CandidateBegin(candidate, size, 2);
  complete(1);
  assert(g_ps_object_candidate_probe.profile_status == 0 && g_ps_object_candidate_probe.raster_status == 1);
  assert(g_ps_object_candidate_probe.payload_reason == PS_LPBAM_ADMISSION_REASON_BUILD);
  assert(g_ps_object_candidate_probe.frames_composed == 0);
  assert(memcmp(&catalog_before, &s_ps_egg_runtime_context, sizeof(catalog_before)) == 0);
  assert(memcmp(&active_before, &s_ps_object_graph, sizeof(active_before)) == 0);
  assert(memcmp(framebuffer_before, s_display_framebuffer, sizeof(framebuffer_before)) == 0);

  send_status = 9;
  count = ack_count;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  assert(ps_candidate_busy == 0 && ps_candidate_blob == NULL && ack_count == count);
  assert(g_ps_object_candidate_probe.queue_status == 9 && g_ps_object_candidate_probe.status == 1);
  send_status = 0;

  delivery = 0;
  wait_status = 7;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  assert(ps_candidate_busy && ps_candidate_sent && g_ps_object_candidate_probe.leased);
  assert(g_ps_object_candidate_probe.wait_status == 7 && g_ps_object_candidate_probe.status == UINT32_MAX);
  frozen = ps_candidate_program;
  borrowed = ps_candidate_catalog;
  token = g_ps_object_candidate_probe.request_id;
  count = sends;
  PS_HW6_RTOS_CandidateBegin(baseline, baseline_size, 1);
  assert(sends == count && g_ps_object_candidate_probe.request_id == token);
  g_ps_object_candidate_request = 2;
  PS_HW6_RTOS_CandidateService();
  assert(g_ps_object_candidate_probe.refused == 2 && ps_candidate_busy);
  assert(memcmp(&frozen, &ps_candidate_program, sizeof(frozen)) == 0);
  assert(memcmp(&borrowed, &ps_candidate_catalog, sizeof(borrowed)) == 0 && ps_candidate_blob == candidate);
  memcpy(stale, queued, sizeof(stale));
  stale[2]--;
  stale[3] = ~stale[2];
  count = ack_count;
  PS_HW6_RTOS_CandidateDisplay(stale);
  assert(ack_count == count && g_ps_object_candidate_probe.display_complete == 0);
  PS_HW6_RTOS_CandidateReap();
  assert(ps_candidate_busy);
  /* Live state/activation does not own this private display request. */
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  PS_HW6_RTOS_CandidateDisplay(queued);
  assert(ps_candidate_busy && ps_candidate_blob == candidate);
  PS_HW6_RTOS_CandidateDisplay(queued);
  assert(ack_count == count + 1);
  PS_HW6_RTOS_CandidateService();
  complete(0);
  assert(g_ps_object_candidate_probe.late_completions == 1);

  /* A stale event with no matching result never frees the current request. */
  wait_status = 0;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 2);
  assert(ps_candidate_busy && g_ps_object_candidate_probe.wait_status == TX_CALLER_ERROR);
  PS_HW6_RTOS_CandidateDisplay(stale);
  assert(ps_candidate_busy && g_ps_object_candidate_probe.display_complete == 0);
  PS_HW6_RTOS_CandidateDisplay(queued);
  PS_HW6_RTOS_CandidateReap();
  complete(1);
  assert(g_ps_object_candidate_probe.late_completions == 2);

  delivery = 2; /* Display may finish inside the send, before runtime waits. */
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  complete(0);
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 1;
  count = display_clock_calls;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  complete(2);
  assert(display_clock_calls == count && g_ps_object_candidate_probe.frames_composed == 0);
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 0;
  g_ps_hw6_owner_probe.display_complete = 0;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  complete(2);
  assert(display_clock_calls == count);
  g_ps_hw6_owner_probe.display_complete = 1;
  display_clock_failure = 5;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  complete(1);
  assert(g_ps_object_candidate_probe.raster_status == UINT32_MAX);
  display_clock_failure = 0;
  display_release_failure = 5;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  complete(1);
  assert(g_ps_object_candidate_probe.payload_status == 0);
  display_release_failure = 0;
  runtime_clock_failure = 5;
  count = sends;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  assert(!ps_candidate_busy && sends == count && g_ps_object_candidate_probe.profile_status == UINT32_MAX);
  runtime_clock_failure = 0;
  runtime_release_failure = 5;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  assert(!ps_candidate_busy && sends == count && g_ps_object_candidate_probe.schedule_status == 0);
  runtime_release_failure = 0;
  candidate[0] ^= 1;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  assert(!ps_candidate_busy && sends == count && g_ps_object_candidate_probe.profile_status == 1);
  candidate[0] ^= 1;
  assert(PS_HW6_RTOS_InstalledObjectCheck(candidate, size, NULL) == 0);
  complete(0);
  assert(PS_HW6_RTOS_InstalledObjectCheck(candidate,
    PS_TARGET_PROFILE_PACKAGE_RESIDENT_BYTES + 1U, NULL) == 1);

  /* The actual source-resolved entry path, not the development launcher. */
installed_tests:
  PS_SceneRuntime_ExitStateScene();
  set_hash(argv[1], baseline, baseline_size);
  PS_SceneRuntime_SetObjectAdmission(NULL);
  assert(PS_SceneRuntime_EnterStateScene() == PS_SCENE_RUNTIME_INDEX_INVALID);
  PS_SceneRuntime_SetObjectAdmission(PS_HW6_RTOS_InstalledObjectCheck);
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  assert(PS_SceneRuntime_InstalledObjectsActive() == 1);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(550) == 0);
  active_before = s_ps_object_graph;
  display_clock_failure = 1;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_ERROR);
  assert(memcmp(&active_before, &s_ps_object_graph, sizeof(active_before)) == 0);
  display_clock_failure = 0;
  g_ps_hw6_rtos_probe.runtime_active_capabilities = PS_HW6_RTOS_RUNTIME_CLOCK_REACTIVE_CAPABILITIES;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(s_ps_object_graph.objects.state == 1);
  assert(s_ps_object_graph.objects.objects[0].cycle_ms ==
    active_before.objects.objects[0].cycle_ms);
  g_ps_hw6_rtos_probe.runtime_active_capabilities = 0;

  /* Timeout aborts the transaction; a later display completion owns its copy. */
  active_before = s_ps_object_graph;
  delivery = 0; wait_status = 7;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_ERROR);
  assert(ps_candidate_busy && memcmp(&active_before, &s_ps_object_graph, sizeof(active_before)) == 0);
  assert(PS_HW6_RTOS_InstalledObjectCheck(candidate, size, NULL) == 1);
  baseline[0] ^= 1;
  PS_HW6_RTOS_CandidateDisplay(queued);
  PS_HW6_RTOS_CandidateReap();
  complete(0);
  baseline[0] ^= 1;
  delivery = 1; wait_status = 0;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  PS_SceneRuntime_ExitStateScene();
  assert(PS_SceneRuntime_InstalledObjectsActive() == 0);
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  assert(s_ps_object_graph.objects.state == 0);
  g_ps_object_candidate_probe.request_id = UINT32_MAX;
  count = sends;
  PS_HW6_RTOS_CandidateBegin(candidate, size, 1);
  assert(!ps_candidate_busy && sends == count); /* Never wrap into stale tokens. */
  return 0;
}
