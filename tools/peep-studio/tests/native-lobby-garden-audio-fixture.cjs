const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const readline = require('node:readline');
const { spawn } = require('node:child_process');

const root = path.resolve(__dirname, '../../..');
const projectPath = path.join(root, 'examples/authoring/native_v2_lobby_garden_audio.peepproj');
const outputDir = path.join(root, 'tools/peep-studio/dist/api45-v2-audio');
const outputPath = path.join(outputDir, 'dev.peepshow.native_v2_lobby_garden_audio.egg');
const child = spawn(process.env.PEEPSHOW_PYTHON || 'python', ['-u', 'tools/authoring/egg_tool.py', 'service'], {
  cwd: root,
  windowsHide: true,
});

let requestId = 0;
let revision;
const pending = new Map();
readline.createInterface({ input: child.stdout }).on('line', (line) => {
  const response = JSON.parse(line);
  const request = pending.get(response.id);
  pending.delete(response.id);
  if (response.ok) request?.resolve(response.result);
  else request?.reject(new Error(JSON.stringify(response.error)));
});
child.stderr.on('data', (data) => process.stderr.write(data));
child.on('error', (error) => {
  for (const request of pending.values()) request.reject(error);
});
const watchdog = setTimeout(() => {
  child.kill();
  process.exitCode = 1;
  console.error('API 45 audio fixture verification timed out');
}, 30000);

async function call(operation, params = {}) {
  const id = String(++requestId);
  const result = await new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    child.stdin.write(`${JSON.stringify({
      protocol_version: 1,
      id,
      operation,
      params: {
        ...(revision === undefined || ['service.hello', 'project.load'].includes(operation)
          ? {} : { project_revision: revision }),
        ...params,
      },
    })}\n`);
  });
  revision = result.project_revision ?? revision;
  return result;
}

function object(snapshot, objectId) {
  return snapshot.objects.find((item) => item.object_id === objectId);
}

(async () => {
  const hello = await call('service.hello');
  const audioProfile = hello.package_export.v2_profile.audio_profile;
  assert.equal(hello.package_export.v2_profile.audio, true);
  assert.equal(audioProfile.supported, true);
  assert.equal(audioProfile.capability, 'audio.sampled_sfx');
  assert.deepEqual(audioProfile.action_contexts, ['local_transition', 'local_timer_handler']);
  assert.deepEqual(hello.scene_object_authoring.audio_export, audioProfile);

  const loaded = await call('project.load', { path: projectPath });
  assert.equal(loaded.valid, true);
  assert.deepEqual(loaded.build_issues, []);
  assert.equal(loaded.document.audio_assets.length, 2);
  assert.equal(loaded.document.audio_cues.length, 2);
  assert.deepEqual(loaded.document.audio_assets.map((asset) => asset.duration_ms), [191, 6000]);
  for (const capability of Object.values(loaded.scene_capabilities)) {
    assert.equal(capability.export_ready, true);
    assert.equal(capability.export_readiness_scope, 'whole_project');
    assert.deepEqual(capability.audio_export, audioProfile);
  }

  const lobby = loaded.document.scenes.find((scene) => scene.scene_id === 'lobby');
  const garden = loaded.document.scenes.find((scene) => scene.scene_id === 'garden');
  assert(lobby && garden);
  assert(lobby.routes.every((route) => route.actions.length === 0));
  assert(garden.routes.filter((route) => route.target_scene).every((route) => route.actions.length === 0));
  assert.deepEqual(garden.routes.filter((route) => route.target_state).map((route) => route.actions), [
    [{ kind: 'play_sfx', cue_ref: 'garden_select.cue' }],
    [{ kind: 'play_sfx', cue_ref: 'garden_select.cue' }],
  ]);
  assert.deepEqual(garden.event_handlers[0].actions, [
    { kind: 'object.set_visibility', object_ref: 'timer_fill', visible: true },
    { kind: 'play_sfx', cue_ref: 'garden_timer_tone.cue' },
  ]);
  assert.equal(garden.event_handlers[0].target_state, undefined);

  let snapshot = await call('project.preview_reset', { scene_id: 'lobby', state_id: 'idle' });
  const input = async (logicalSource) => {
    snapshot = await call('project.preview_input', {
      preview_revision: snapshot.preview_revision,
      logical_source: logicalSource,
    });
    return snapshot;
  };
  const advance = async (elapsedMs) => {
    snapshot = await call('project.preview_advance', {
      preview_revision: snapshot.preview_revision,
      elapsed_ms: elapsedMs,
    });
    return snapshot;
  };

  await input('BUTTON_A');
  assert.equal(snapshot.scene.scene_id, 'garden');
  assert.equal(snapshot.scene.state_id, 'left');
  assert.deepEqual(snapshot.input.audio_events, []);
  assert.equal(object(snapshot, 'timer_fill').effective.visible, false);
  await advance(650);
  const animationBeforeRight = object(snapshot, 'sequence').playback;
  await input('BUTTON_R');
  assert.equal(snapshot.scene.state_id, 'right');
  assert.equal(object(snapshot, 'selection_marker').effective.x, 126);
  assert.deepEqual(object(snapshot, 'sequence').playback, animationBeforeRight);
  assert.deepEqual(snapshot.input.audio_events.map((event) => event.cue_id), ['garden_select.cue']);
  const objectsBeforeRepeatedRight = snapshot.objects;
  await input('BUTTON_R');
  assert.equal(snapshot.input.accepted, false);
  assert.deepEqual(snapshot.input.audio_events, []);
  assert.deepEqual(snapshot.objects, objectsBeforeRepeatedRight);
  const animationBeforeLeft = object(snapshot, 'sequence').playback;
  await input('BUTTON_L');
  assert.equal(snapshot.scene.state_id, 'left');
  assert.deepEqual(object(snapshot, 'sequence').playback, animationBeforeLeft);
  assert.deepEqual(snapshot.input.audio_events.map((event) => event.cue_id), ['garden_select.cue']);

  await advance(1350);
  assert.equal(snapshot.timeline.elapsed_ms, 2000);
  assert.equal(object(snapshot, 'timer_fill').effective.visible, true);
  assert.equal(snapshot.timer_events.length, 1);
  assert.deepEqual(snapshot.timer_events[0].audio_events.map((event) => event.cue_id), ['garden_timer_tone.cue']);
  await input('BUTTON_B');
  assert.equal(snapshot.scene.scene_id, 'lobby');
  assert.deepEqual(snapshot.input.audio_events, []);
  await input('BUTTON_A');
  assert.equal(snapshot.scene.scene_id, 'garden');
  assert.equal(snapshot.scene.state_id, 'left');
  assert.equal(snapshot.timeline.elapsed_ms, 0);
  assert.equal(object(snapshot, 'timer_fill').effective.visible, false);
  assert.equal(object(snapshot, 'sequence').effective.visual_ref, 'sequence.1');

  const built = await call('project.build_package');
  const egg = Buffer.from(built.package.blob_base64, 'base64');
  const sha256 = crypto.createHash('sha256').update(egg).digest('hex');
  assert.equal(egg.length, built.package.size_bytes);
  assert.equal(sha256, built.package.sha256);
  assert.equal(built.package.scene_count, 2);
  assert.equal(built.package.audio_asset_count, 2);
  assert.equal(built.package.audio_cue_count, 2);
  fs.mkdirSync(outputDir, { recursive: true });
  fs.writeFileSync(outputPath, egg);

  console.log(`Validated API 45 source fixture: ${projectPath}`);
  console.log(`Built normal service egg: ${outputPath}`);
  console.log(`Size: ${egg.length} bytes`);
  console.log(`SHA-256: ${sha256}`);
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
}).finally(() => {
  clearTimeout(watchdog);
  child.kill();
});
