const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const readline = require('node:readline');
const { spawn } = require('node:child_process');

const root = path.resolve(__dirname, '../../..');
const fourFrames = process.argv.includes('--four-frames');
const timers = fourFrames || process.argv.includes('--timers');
const fixtureName = fourFrames ? 'native_v2_timer_four_frames' : timers ? 'native_v2_scene_timer' : 'native_v2_continuity';
const projectPath = path.join(root, `examples/authoring/${fixtureName}.peepproj`);
const frameRefs = fourFrames ? ['pulse.a', 'pulse.b', 'pulse.c', 'pulse.d'] : ['pulse.a', 'pulse.b'];
const create = process.argv.includes('--create');
const child = spawn(process.env.PEEPSHOW_PYTHON || 'python', ['-u', 'tools/authoring/egg_tool.py', 'service'], {
  cwd: root, windowsHide: true,
});
let id = 0, revision;
const pending = new Map();
readline.createInterface({ input: child.stdout }).on('line', line => {
  const response = JSON.parse(line), promise = pending.get(response.id);
  pending.delete(response.id);
  if (response.ok) promise?.resolve(response.result);
  else promise?.reject(new Error(JSON.stringify(response.error)));
});
child.stderr.on('data', data => process.stderr.write(data));
child.on('error', error => { for (const p of pending.values()) p.reject(error); });
const watchdog = setTimeout(() => { console.error('Fixture service timed out'); child.kill(); process.exitCode = 1; }, 30000);
async function call(operation, params = {}) {
  const requestId = String(++id);
  const result = await new Promise((resolve, reject) => {
    pending.set(requestId, { resolve, reject });
    child.stdin.write(JSON.stringify({ protocol_version: 1, id: requestId, operation,
      params: { ...(revision === undefined || ['service.hello', 'project.create', 'project.load'].includes(operation)
        ? {} : { project_revision: revision }), ...params } }) + '\n');
  });
  revision = result.project_revision ?? revision;
  return result;
}
const command = (kind, params) => ({ kind, scene_id: 'main', ...params });
const edit = (...commands) => call('project.apply_commands', { commands });
const object = (snapshot, id) => snapshot.objects.find(item => item.object_id === id);

(async () => {
  if (create) {
    if (fs.existsSync(projectPath)) {
      const draft = await call('project.load', { path: projectPath });
      assert.equal(draft.document.scenes.length, 1, 'Refusing to overwrite a populated fixture');
      assert.equal(draft.document.scenes[0].objects.length, 0);
      assert.equal(draft.document.scenes[0].states.length, 1);
      assert.equal(draft.summary.asset_frame_count, 0);
      assert(!fs.existsSync(path.join(projectPath, 'assets/pulse.png')));
    } else await call('project.create', { path: projectPath, scene_schema_version: 2 });
    fs.mkdirSync(path.join(projectPath, 'assets'), { recursive: true });
    if (fourFrames) {
      await edit(...frameRefs.map((frame, index) => ({ kind: 'asset.upsert', asset: {
        asset_id: `sequence_${index + 1}`, display_name: `Sequence ${index + 1}`, asset_type: 'masked_1bpp',
        source_format: 'system_font_text', font_id: 'peepshow.system.8x8.basic.v1', text: String(index + 1), scale: 3,
        frames: [{ frame_id: frame, pivot_x: 0, pivot_y: 0 }],
      } })));
    } else {
      // Reuse only this bitmap; no example scene, migration, or runtime records are copied.
      fs.copyFileSync(path.join(root, 'examples/authoring/state_transition_slice.peepproj/assets/cursor.png'),
        path.join(projectPath, 'assets/pulse.png'));
      await edit({ kind: 'asset.upsert', asset: {
      asset_id: 'pulse', display_name: 'Continuity Sprite', asset_type: 'masked_1bpp',
      source_path: 'assets/pulse.png', source_format: 'png',
      frames: ['a', 'b'].map((suffix, index) => ({ frame_id: `pulse.${suffix}`,
        source_rect: { x: index * 8, y: 0, width: 8, height: 16 }, pivot_x: 0, pivot_y: 0 })),
      } });
    }
    await edit({ kind: 'animation.upsert', animation: { animation_id: 'pulse_loop',
      frame_refs: frameRefs, frame_duration_ms: frameRefs.map(() => fourFrames ? 400 : 500), loop_policy: 'loop' } });
    const added = await edit(command('state.create', { display_name: 'Marker Right', x: 420, y: 0 }));
    const right = added.applied_commands[0].state.state_id;
    await edit(
      command('scene.rename', { display_name: 'Animation Continuity' }),
      command('state.rename', { state_id: 'start', display_name: 'Marker Left' }),
      command('editor.state_graph.set_node_position', { state_id: 'start', x: 0, y: 0 }),
      command('editor.state_graph.set_node_position', { node_id: 'scene-entry', x: -260, y: 0 }),
      command('object.add', { object: { object_id: 'continuity_sprite', kind: 'sprite', width: fourFrames ? 24 : 8, height: fourFrames ? 24 : 16,
        z_order: 0, layer: 'SCENE', defaults: { x: fourFrames ? 72 : 80, y: 40, visible: true, visual_ref: 'pulse.a' }, animation_ref: 'pulse_loop' } }),
      command('object.add', { object: { object_id: 'position_marker', kind: 'filled_rect', width: 16, height: 16,
        z_order: 1, layer: 'SCENE', defaults: { x: 32, y: 104, visible: true } } }),
      command('object_override.set', { object_id: 'position_marker', state_id: right, properties: { x: 120 } }),
      command('route.create_trigger', { source_state: 'start', logical_source: 'BUTTON_A', event_kind: 'press', target_state: right }),
      command('route.create_trigger', { source_state: right, logical_source: 'BUTTON_B', event_kind: 'press', target_state: 'start' }),
    );
    if (timers) await edit(
      command('scene.rename', { display_name: fourFrames ? 'Four Frame Timer Continuity' : 'Scene Timer Continuity' }),
      command('object.add', { object: { object_id: 'timer_marker', kind: 'filled_rect', width: 16, height: 16,
        z_order: 2, layer: 'SCENE', defaults: { x: 76, y: 80, visible: false } } }),
      command('event_binding.add', { event_binding: { binding_id: 'reveal_timer', event_type: 'time.scene_elapsed',
        configuration: { delay_ms: 2000, start_policy: 'scene_entry' } } }),
      command('event_handler.add', { event_handler: { handler_id: 'reveal_expired', event_ref: 'reveal_timer', guards: [], actions: [] } }),
      command('object_actions.set', { owner_kind: 'handler', owner_id: 'reveal_expired',
        actions: [{ kind: 'object.set_visibility', object_ref: 'timer_marker', visible: true }] }),
      command('scene.set_reactive_wait_default', { reactive_wait_default: { policy_id: 'main_wait_policy',
        hold_fallback_allowed: true, event_interests: ['button_a_press', 'button_b_press', 'reveal_timer'] } }),
    );
    await call('project.save');
  }
  const loaded = await call('project.load', { path: projectPath });
  assert(loaded.valid);
  assert.equal(loaded.document.scenes.length, 1);
  const scene = loaded.document.scenes[0];
  assert.equal(scene.schema_version, 2);
  assert.equal(scene.states.length, 2);
  assert.equal(scene.objects.length, timers ? 3 : 2);
  assert.equal(scene.routes.length, 2);
  assert.equal(scene.scene_exits.length, 0);
  assert(!scene.render_models && !scene.waiting_visuals);
  assert(scene.states.every(state => state.object_overrides.every(override => override.object_ref === 'position_marker')));
  assert.equal(loaded.scene_capabilities.main.egg_export, false);
  assert.equal(scene.objects.find(item => item.object_id === 'continuity_sprite').animation_ref, 'pulse_loop');
  const right = scene.states.find(state => state.state_id !== 'start').state_id;
  let snapshot = await call('project.preview_reset', { scene_id: 'main', state_id: 'start' });
  const advance = elapsed_ms => call('project.preview_advance', { preview_revision: snapshot.preview_revision, elapsed_ms });
  const input = logical_source => call('project.preview_input', { preview_revision: snapshot.preview_revision, logical_source });
  if (fourFrames) {
    assert.equal(loaded.summary.asset_frame_count, 4);
    const pixels = new Set();
    for (let i = 0; i < 4; i++) {
      assert.equal(object(snapshot, 'continuity_sprite').effective.visual_ref, frameRefs[i]);
      pixels.add(snapshot.framebuffer.data_base64);
      snapshot = await advance(400);
    }
    assert.equal(pixels.size, 4, 'All four sequential frames must actually render distinct pixels');
    assert.equal(object(snapshot, 'continuity_sprite').effective.visual_ref, 'pulse.a');
    snapshot = await call('project.preview_reset', { scene_id: 'main', state_id: 'start' });
  }
  assert(snapshot.framebuffer.black_pixel_count > 256);
  assert.equal(object(snapshot, 'position_marker').effective.x, 32);
  snapshot = await advance(650);
  assert.equal(object(snapshot, 'continuity_sprite').effective.visual_ref, 'pulse.b');
  const beforeA = object(snapshot, 'continuity_sprite');
  snapshot = await input('BUTTON_A');
  assert.equal(snapshot.scene.state_id, right);
  assert.deepEqual(object(snapshot, 'continuity_sprite'), beforeA, 'A must not reset or override the sprite');
  assert.equal(object(snapshot, 'position_marker').effective.x, 120);
  assert.equal(object(snapshot, 'position_marker').underlying.x, 32);
  snapshot = await advance(300);
  const beforeB = object(snapshot, 'continuity_sprite');
  snapshot = await input('BUTTON_B');
  assert.equal(snapshot.scene.state_id, 'start');
  assert.deepEqual(object(snapshot, 'continuity_sprite'), beforeB, 'B must preserve the running clip phase');
  assert.equal(object(snapshot, 'position_marker').effective.x, 32);
  snapshot = await advance(100);
  assert.equal(object(snapshot, 'continuity_sprite').effective.visual_ref, fourFrames ? 'pulse.c' : 'pulse.a');
  snapshot = await advance(500);
  assert.equal(object(snapshot, 'continuity_sprite').effective.visual_ref, fourFrames ? 'pulse.d' : 'pulse.b');
  if (timers) {
    assert.equal(object(snapshot, 'timer_marker').effective.visible, false);
    snapshot = await advance(250);
    snapshot = await input('BUTTON_A');
    assert.equal(snapshot.scene.state_id, right);
    snapshot = await advance(250);
    if (fourFrames) assert.equal(object(snapshot, 'continuity_sprite').effective.visual_ref, 'pulse.b', 'Timer expiry must not restart at frame 1');
    assert.equal(snapshot.scene.state_id, right, 'Action-only expiry must not enter another state');
    assert.equal(object(snapshot, 'timer_marker').underlying.visible, true);
    assert.equal(object(snapshot, 'position_marker').effective.x, 120);
    assert.equal(snapshot.timer_events.length, 1, 'Scene timer must expire across local state changes');
    snapshot = await input('BUTTON_B');
    assert.equal(object(snapshot, 'timer_marker').effective.visible, true);
    assert.equal(object(snapshot, 'position_marker').effective.x, 32);
    snapshot = await advance(6000);
    assert.equal(snapshot.timer_events.length, 0, 'One-shot timer must not repeat');
  }
  console.log(`Validated source fixture: ${projectPath}`);
  console.log('A/B state changes preserve sprite playback; marker override changes/restores X; loop wraps. No egg generated.');
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(() => {
  clearTimeout(watchdog); child.kill();
});
