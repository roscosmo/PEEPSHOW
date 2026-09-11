const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), readline = require('node:readline');
const {spawn} = require('node:child_process');
const root = path.resolve(__dirname,'../../..');
const source = path.join(root,'examples/authoring/native_v2_menu.peepproj');
const create = process.argv.includes('--create');
const child = spawn(process.env.PEEPSHOW_PYTHON || 'python',['-u','tools/authoring/egg_tool.py','service'],{cwd:root,windowsHide:true});
const pending = new Map();let id=0,revision;
readline.createInterface({input:child.stdout}).on('line',line=>{
  const r=JSON.parse(line),p=pending.get(r.id);pending.delete(r.id);
  if(r.ok)p.resolve(r.result);else p.reject(new Error(JSON.stringify(r.error)));
});
child.stderr.on('data',data=>process.stderr.write(data));
const watchdog=setTimeout(()=>{child.kill();process.exitCode=1},30000);
async function call(operation,params={}) {
  const requestId=String(++id);
  const result=await new Promise((resolve,reject)=>{
    pending.set(requestId,{resolve,reject});
    child.stdin.write(JSON.stringify({protocol_version:1,id:requestId,operation,params:{
      ...(!['service.hello','project.create','project.load'].includes(operation)?{project_revision:revision}:{}),...params}})+'\n');
  });
  revision=result.project_revision??revision;return result;
}
const command=(kind,params)=>({kind,scene_id:'main',...params});
const edit=(...commands)=>call('project.apply_commands',{commands});
const textAsset=(id,text,scale=1)=>({kind:'asset.upsert',asset:{asset_id:id,display_name:text,asset_type:'masked_1bpp',
  source_format:'system_font_text',font_id:'peepshow.system.8x8.basic.v1',text,scale,
  frames:[{frame_id:`${id}.frame`,pivot_x:0,pivot_y:0}]}});
const object=(id,kind,width,height,x,y,z,extra={})=>command('object.add',{object:{object_id:id,kind,width,height,z_order:z,layer:'SCENE',defaults:{x,y,visible:true},...extra}});
(async()=>{
  if(create) {
    assert(!fs.existsSync(source),'Refusing to replace an existing example');
    await call('project.create',{path:source,scene_schema_version:2});
    await edit(textAsset('menu_title','MENU',2),textAsset('start_label','Start Game'),textAsset('settings_label','Settings'),textAsset('credits_label','Credits'),
      ...[1,2,3,4].map(n=>textAsset(`counter_${n}`,String(n))));
    const settings=await edit(command('state.create',{display_name:'Settings',x:260,y:390}));
    const settingsId=settings.applied_commands[0].state.state_id;
    const credits=await edit(command('state.create',{display_name:'Credits',x:260,y:780}));
    const creditsId=credits.applied_commands[0].state.state_id;
    await edit(command('scene.rename',{display_name:'Main Menu'}),command('state.rename',{state_id:'start',display_name:'Start Game'}),
      command('editor.state_graph.set_node_position',{state_id:'start',x:260,y:0}),
      command('editor.state_graph.set_node_position',{node_id:'scene-entry',x:0,y:-90}),
      {kind:'animation.upsert',animation:{animation_id:'menu_counter_loop',frame_refs:[1,2,3,4].map(n=>`counter_${n}.frame`),frame_duration_ms:[400,400,400,400],loop_policy:'loop'}},
      object('menu_border','outline_rect',160,136,4,4,0),
      object('menu_divider','filled_rect',144,1,12,31,1),
      ...[{id:'menu_title',w:64,h:16,x:16,y:12},{id:'start_label',w:80,h:8,x:24,y:46},
        {id:'settings_label',w:64,h:8,x:24,y:78},{id:'credits_label',w:56,h:8,x:24,y:110}].map(item=>
        object(item.id,'sprite',item.w,item.h,item.x,item.y,2,{defaults:{x:item.x,y:item.y,visible:true,visual_ref:`${item.id}.frame`}})),
      object('selection_outline','outline_rect',144,24,12,38,3),
      object('continuity_counter','sprite',8,8,140,16,4,{defaults:{x:140,y:16,visible:true,visual_ref:'counter_1.frame'},animation_ref:'menu_counter_loop'}));
    const states=['start',settingsId,creditsId];
    await edit(...states.flatMap((state,index)=>[
      command('object_override.set',{state_id:state,object_id:'selection_outline',properties:{y:38+index*32}}),
      command('route.create_trigger',{source_state:state,logical_source:'JOY_DOWN',event_kind:'press',target_state:states[(index+1)%3]}),
      command('route.create_trigger',{source_state:state,logical_source:'JOY_UP',event_kind:'press',target_state:states[(index+2)%3]}),
    ]));
    await call('project.save');
  }
  const loaded=await call('project.load',{path:source});assert(loaded.valid);
  const scene=loaded.document.scenes[0];
  assert.equal(loaded.document.scenes.length,1);assert.equal(scene.schema_version,2);
  assert.equal(scene.states.length,3);assert.equal(scene.objects.length,8);assert.equal(scene.routes.length,6);
  assert(!scene.render_models&&!scene.waiting_visuals);
  assert(scene.states.every(state=>state.object_overrides.length===1&&state.object_overrides[0].object_ref==='selection_outline'));
  assert(scene.routes.every(route=>route.guards.length===0&&route.actions.length===0&&!route.target_scene));
  assert.equal(loaded.document.audio_cues.length,0);
  let snapshot=await call('project.preview_reset',{scene_id:'main',state_id:'start'});
  const find=id=>snapshot.objects.find(object=>object.object_id===id);
  const states=['start',scene.states.find(state=>state.display_name==='Settings').state_id,scene.states.find(state=>state.display_name==='Credits').state_id];
  const buffers=new Set([snapshot.framebuffer.data_base64]);
  for(let index=0;index<4;index++) {
    snapshot=await call('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:400});
    buffers.add(snapshot.framebuffer.data_base64);
  }
  assert.equal(buffers.size,4);
  for(const [input,indices] of [['JOY_DOWN',[1,2,0]],['JOY_UP',[2,1,0]]]) for(const index of indices) {
    snapshot=await call('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:150});
    const playback=find('continuity_counter').playback;
    snapshot=await call('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:input});
    assert.equal(snapshot.scene.state_id,states[index]);assert.equal(find('selection_outline').effective.y,38+index*32);
    assert.deepEqual(find('continuity_counter').playback,playback);
  }
  assert.deepEqual(loaded.build_issues,[]);
  assert.equal(loaded.scene_capabilities.main.export_ready,true);
  console.log(`Native menu example: ${source}\nThree states, scene-owned labels, selector-only overrides, wrap navigation and four-frame continuity verified; backend export ready.`);
})().catch(error=>{console.error(error);process.exitCode=1}).finally(()=>{clearTimeout(watchdog);child.kill()});
