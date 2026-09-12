const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),readline=require('node:readline');
const {spawn}=require('node:child_process');
const root=path.resolve(__dirname,'../../..');
const source=path.join(root,'examples/authoring/native_v2_scene_connections.peepproj');
const child=spawn(process.env.PEEPSHOW_PYTHON||'python',['-u','tools/authoring/egg_tool.py','service'],{cwd:root,windowsHide:true});
let id=0,revision;const pending=new Map();
readline.createInterface({input:child.stdout}).on('line',line=>{const r=JSON.parse(line),p=pending.get(r.id);pending.delete(r.id);r.ok?p.resolve(r.result):p.reject(new Error(JSON.stringify(r.error)))});
child.stderr.on('data',data=>process.stderr.write(data));
const watchdog=setTimeout(()=>{child.kill();process.exitCode=1},30000);
async function call(operation,params={}) {
  const result=await new Promise((resolve,reject)=>{const requestId=String(++id);pending.set(requestId,{resolve,reject});
    child.stdin.write(JSON.stringify({protocol_version:1,id:requestId,operation,params:{
      ...(!['service.hello','project.create','project.load'].includes(operation)?{project_revision:revision}:{}),...params}})+'\n')});
  revision=result.project_revision??revision;return result;
}
const edit=(...commands)=>call('project.apply_commands',{commands});
const command=(scene_id,kind,params)=>({scene_id,kind,...params});
(async()=>{
  if(process.argv.includes('--create')) {
    assert(!fs.existsSync(source),'Refusing to overwrite existing fixture');
    await call('project.create',{path:source,scene_schema_version:2});
    await edit(command('main','scene.rename',{display_name:'Lobby'}),{kind:'scene.add',display_name:'Garden',scene_schema_version:2});
    for(const [scene,name,hint,x] of [['main','LOBBY','A: GARDEN',0],['garden','GARDEN','B: LOBBY',520]]) {
      const assets=[['title',name,2],['hint',hint,1]];
      for(const [suffix,text,scale] of assets){const id=`${scene}_${suffix}`;
        await edit({kind:'asset.upsert',asset:{asset_id:id,display_name:text,asset_type:'masked_1bpp',source_format:'system_font_text',
          font_id:'peepshow.system.8x8.basic.v1',text,scale,frames:[{frame_id:`${id}.frame`,pivot_x:0,pivot_y:0}]}},
          command(scene,'object.add',{object:{object_id:suffix,kind:'sprite',width:text.length*8*scale,height:8*scale,z_order:1,layer:'SCENE',
            defaults:{x:16,y:suffix==='title'?24:80,visible:true,visual_ref:`${id}.frame`}}}));
      }
      await edit(command(scene,'object.add',{object:{object_id:'border',kind:'outline_rect',width:160,height:136,z_order:0,layer:'SCENE',defaults:{x:4,y:4,visible:true}}}),
        command(scene,'editor.scene_flow.set_node_position',{x,y:80}));
    }
    for(const [scene,target,input] of [['main','garden','BUTTON_A'],['garden','main','BUTTON_B']]) {
      const result=await edit(command(scene,'scene_exit.add',{target_scene:target}));
      const exit=result.applied_commands[0].scene_exit.scene_exit_id;
      await edit(command(scene,'route.create_trigger',{source_state:'start',logical_source:input,scene_exit_ref:exit}));
    }
    const alias=await edit({kind:'editor.scene_flow.add_reference',target_scene:'main',x:1000,y:100});
    const referenceId=alias.applied_commands[0].reference_id;
    assert(referenceId,'Service must return reference ID');
    await edit({kind:'editor.scene_flow.set_exit_reference',scene_id:'garden',endpoint_kind:'scene_exit',endpoint_id:'to_main',reference_id:referenceId});
    await call('project.save');
  }
  const loaded=await call('project.load',{path:source});assert(loaded.valid);
  assert.equal(loaded.document.scenes.length,2);
  for(const scene of loaded.document.scenes){assert.equal(scene.schema_version,2);assert.equal(scene.routes.length,1);assert.deepEqual(scene.routes[0].actions,[]);
    assert.equal(loaded.scene_capabilities[scene.scene_id].scene_connection_commands,true);assert.equal(loaded.scene_capabilities[scene.scene_id].export_ready,false)}
  assert(loaded.build_issues.length>0);
  let snapshot=await call('project.preview_reset',{scene_id:'main'});const lobby=snapshot.framebuffer.data_base64;
  for(let i=0;i<3;i++){
    snapshot=await call('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:'BUTTON_A'});
    assert.equal(snapshot.scene.scene_id,'garden');assert.equal(snapshot.scene.state_id,'start');assert.notEqual(snapshot.framebuffer.data_base64,lobby);
    snapshot=await call('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:700});
    snapshot=await call('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:'BUTTON_B'});
    assert.equal(snapshot.scene.scene_id,'main');assert.equal(snapshot.scene.state_id,'start');assert.equal(snapshot.framebuffer.data_base64,lobby);
  }
  console.log(`Host verified: ${source}\nLobby A -> Garden; Garden B -> Lobby. Default entry, distinct rendered labels, named exits and return alias. Multi-scene export blocked.`);
})().catch(error=>{console.error(error);process.exitCode=1}).finally(()=>{clearTimeout(watchdog);child.kill()});
