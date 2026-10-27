// Synthetic images and mocked Web Serial only; never opens real hardware.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {webcrypto}=require('node:crypto'),{pathToFileURL}=require('node:url');
const root=path.resolve(__dirname,'../..'),html=fs.readFileSync(path.join(root,'tools/rs232_tester.html'),'utf8');
const context=vm.createContext({Uint8Array,DataView,TextEncoder,TextDecoder,crypto:webcrypto,btoa,atob,setTimeout,clearTimeout});
vm.runInContext(html.match(/<script id="et100-smp-core">([\s\S]*?)<\/script>/)[1]+'\nthis.S=ET100Smp;',context);
const S=context.S,zip=require('../vendor/fflate.min.js'),encoder=new TextEncoder();
const info={project:'eup_canbus_32f407',hardware:'EUP_stm32f407_dk',soc:'stm32f407'};
const manifest={type:'application',file:'f407.bin',board:'stm32f407_dk',soc:'stm32f407'};
async function makeImage({project=info.project,hardware=info.hardware,sp=0x20020000,size=4096}={}) {
  const bytes=new Uint8Array(size),view=new DataView(bytes.buffer),at=544,end=size-40;
  view.setUint32(0,0x96f3b83d,true);view.setUint16(8,512,true);view.setUint32(12,end-512,true);
  bytes[20]=0;bytes[21]=2;view.setUint16(22,4,true);
  view.setUint32(512,sp,true);view.setUint32(516,0x08020201,true);
  bytes.set(encoder.encode('EUPFWV1\0'),at);view.setUint16(at+8,1,true);
  bytes.set(encoder.encode(project),at+10);bytes.set(encoder.encode(hardware),at+34);
  bytes.set(encoder.encode('0.2.4-dev'),at+54);
  view.setUint32(at+282,S.crc32(bytes.slice(at,at+282)),true);
  bytes.set(Uint8Array.of(7,105,40,0,16,0,32,0),end);
  bytes.set(new Uint8Array(await webcrypto.subtle.digest('SHA-256',bytes.slice(0,end))),end+8);
  return bytes;
}
const inst=body=>`${body}*${S.crc16(encoder.encode(body)).toString(16).toUpperCase().padStart(4,'0')}`;
async function offline() {
  const bytes=await makeImage(),file=await S.validate(bytes,manifest,info);
  assert.equal(file.device,'f407');assert.equal(file.soc,'stm32f407');
  assert.equal(S.deviceProfile(info).maxSize,0x60000);
  assert.equal(S.deviceProfile({...info,soc:'fr3068e_c'}),null);
  const zipped=zip.zipSync({'manifest.json':encoder.encode(JSON.stringify({files:[{...manifest,size:bytes.length}]})),[manifest.file]:bytes});
  assert.equal((await S.loadFile('dfu_application.zip',zipped,zip.unzipSync,info)).device,'f407');
  await assert.rejects(S.validate(await makeImage({project:'OTHER'})),/OTHER/);
  await assert.rejects(S.validate(bytes,{...manifest,board:'dev14_fr3068ep_c'}),/board/);
  await assert.rejects(S.validate(bytes,{...manifest,soc:'fr3068e_c'}),/soc/);
  await assert.rejects(S.validate(await makeImage({size:0x60000+1})),/大小超界/);
  await S.validate(await makeImage({size:0x60000}));
  for(const sp of [0x20000000,0x20020008,0x2001ffff])await assert.rejects(S.validate(await makeImage({sp})),/vector/);
  const etInfo={project:'EUP_ET100',hardware:'ET100_EVB_DEV1.4',soc:'fr3068e_c'};
  const etBytes=await makeImage({...etInfo,sp:0x20080000});
  for(const [image,device] of [[bytes,etInfo],[etBytes,info]]) {
    await assert.rejects(S.validate(image,null,device),e=>e.message.includes('裝置／檔案硬體不符')&&e.message.includes(device.project)&&e.message.includes('stm32f407'));
  }
  const write=inst('F407$INST;1;L;WRITE;8755CE49;262144;314752');
  const hello=inst('F407$INST;1;L;HELLO;8755CE49;0.8.11;PENDING');
  assert.equal(write,'F407$INST;1;L;WRITE;8755CE49;262144;314752*5311');
  assert.equal(hello,'F407$INST;1;L;HELLO;8755CE49;0.8.11;PENDING*6AD5');
  assert.equal(S.parseInst(write,'f407').event,'WRITE');assert.equal(S.parseInst(hello).device,'f407');
  assert.equal(S.parseInst(write,'et100'),null);
  assert.equal(S.parseInst(inst('ET100$INST;1;L;WRITE;8755CE49;262144;314752'),'f407'),null);
  assert.equal(S.parseInst(write.slice(0,-1)+(write.endsWith('0')?'1':'0')),null);
  assert.equal(S.parseInst(write.replace('F407','ET100')),null,'CRC includes prefix');
  const client=new S.Client({now:()=>0,line:()=>{},inst:()=>{throw Error('wrong device event adopted');}});
  client.inst={device:'f407',txn:'8755CE49'};
  client.receiveLine(inst('ET100$INST;1;L;HELLO;8755CE49;0.8.11;PENDING'));
  assert.equal(client.inst.hello,undefined);
  console.log('PASS F407 synthetic image/ZIP, profile identity, size/SP boundaries and INST vectors:',write,hello);
  return {bytes,etBytes,zipped};
}
async function browserTests(fixtures) {
  const {chromium}=require(process.env.PLAYWRIGHT_MODULE || '/Users/stanley/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
  const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL || 'chrome'});
  try {
    for(const scenario of ['usb','poll-only','event-only','rollback-reboot','timeout','cancel','rs232']) {
      const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
      page.on('pageerror',e=>errors.push(e.message));
      await page.addInitScript(({scenario})=>{
        const mock=window.mockF407={scenario,clock:0,opens:[],closes:[],commands:[],smp:[],polls:0,reset:false,off:0,ports:[],events:0};
        const bus=new EventTarget(),listeners=new Set();mock.listeners=listeners;
        const emit=(name,port)=>{const event=new Event(name);Object.defineProperty(event,'port',{value:port});bus.dispatchEvent(event);mock.events++;};
        mock.emit=emit;
        const makePort=(name,uid)=>({name,uid,
          getInfo:()=>scenario==='rs232'?{usbVendorId:0x0403,usbProductId:0x6001}:{usbVendorId:0x0483,usbProductId:0x5740},
          async open({baudRate}) {
            mock.opens.push([name,baudRate]);this.baud=baudRate;
            this.readable=new ReadableStream({start:c=>{this.controller=c;}});
            this.writable=new WritableStream({write:b=>mock.write(this,b)});
          },
          async close(){assertUnlocked(this);mock.closes.push(name);this.readable=null;this.writable=null;}
        });
        function assertUnlocked(p){if(p.readable?.locked||p.writable?.locked)throw Error('locked close '+p.name);}
        const uid='00112233445566778899AABB';
        mock.original=makePort('original',uid);mock.wrong=makePort('wrong','FFEEDDCCBBAA998877665544');
        mock.next=makePort('next',uid);mock.final=makePort('final',uid);
        const serial={requestPort:async()=>mock.original,getPorts:async()=>scenario==='event-only'?[]:mock.ports,
          addEventListener:(name,fn)=>{listeners.add(fn);bus.addEventListener(name,fn);},
          removeEventListener:(name,fn)=>{listeners.delete(fn);bus.removeEventListener(name,fn);}};
        Object.defineProperty(navigator,'serial',{value:serial});
        mock.reply=(p,text)=>p.controller.enqueue(new TextEncoder().encode(text+'\r\n'));
        mock.slot=(current=false,confirmed=true)=>({slot:0,version:current?mock.file.version:'0.2.3.0',
          hash:current?mock.file.tlvHash:new Uint8Array(32),confirmed,active:true,bootable:true});
        mock.drop=p=>{p.controller.error(Error('mock USB removed'));emit('disconnect',p);};
        mock.write=(p,bytes)=>{
          const text=new TextDecoder().decode(bytes);
          if(text==='\r\n')return;
          if(text.startsWith('BLEGW$')) {
            const f=text.trim().split(';');mock.commands.push([p.name,f[0]]);
            if(f[0]==='BLEGW$GCHK')mock.reply(p,`AT$PD=BLEGW$GCHK;1;${p.uid};ST;EUP_stm32f407_dk;0.2.3-dev;0;${f[1]}`);
            else if(f[0]==='BLEGW$GFWINFO')mock.reply(p,`AT$PD=BLEGW$GFWINFO;1;eup_canbus_32f407;EUP_stm32f407_dk;0.2.3-dev;stm32f407;${f[1]}`);
            else if(f[0]==='BLEGW$OTABAUD')mock.reply(p,`AT$PD=BLEGW$OTABAUD;1;${f[1]};${f[2]}`);
            else if(f[0]==='BLEGW$GOTASTAT')mock.reply(p,`AT$PD=BLEGW$GOTASTAT;1;CONFIRMED;${et100Client.inst.txn};ROLLED_BACK;9;0.2.3-dev;${f[1]}`);
            return;
          }
          p.decoder??=new ET100Smp.Decoder(packet=>{
            const body=ET100Smp.uncbor(packet.slice(8)),key=`${packet[0]&7}/${packet[4]*256+packet[5]}/${packet[7]}`;
            mock.smp.push([p.name,key]);
            const respond=reply=>p.controller.enqueue(ET100Smp.frame(ET100Smp.packet((packet[0]&7)+1,packet[4]*256+packet[5],packet[7],packet[6],reply)));
            if(key==='0/1/0') {
              if(mock.reset) {
                mock.polls++;
                if(scenario==='rollback-reboot'&&mock.polls===1) {
                  respond({images:[mock.slot(true,false)]});
                  queueMicrotask(()=>{mock.drop(p);mock.ports=[mock.final];emit('connect',mock.final);});return;
                }
                respond({images:[scenario==='rollback-reboot'?mock.slot():mock.slot(true,scenario!=='timeout'&&mock.polls>1)]});return;
              }
              respond({images:mock.off?[mock.slot(),{slot:1,version:mock.file.version,hash:mock.file.tlvHash,bootable:true}]:[mock.slot()]});return;
            }
            if(key==='2/1/1'){mock.off=body.off+body.data.length;respond({off:mock.off});return;}
            if(key==='2/1/0'){respond({});return;}
            if(key==='2/0/5') {
              mock.reset=true;respond({});
              if(scenario==='rs232') {
                for(const body of [`F407$INST;1;L;HELLO;${et100Client.inst.txn};0.2.3;PENDING`,
                  `F407$INST;1;L;BOOT;${et100Client.inst.txn};TRIAL;9600`])
                  mock.reply(p,body+'*'+ET100Smp.crc16(new TextEncoder().encode(body)).toString(16).toUpperCase().padStart(4,'0'));
              } else queueMicrotask(()=>{
                mock.drop(p);
                if(!['timeout','cancel'].includes(scenario)) {
                  mock.ports=[mock.wrong,mock.next];
                  if(scenario!=='poll-only'){emit('connect',mock.wrong);emit('connect',mock.next);}
                }
              });return;
            }
            throw Error('unexpected '+key);
          });
          p.decoder.feed(bytes);
        };
      },{scenario});
      await page.goto(pathToFileURL(path.join(root,'tools/rs232_tester.html')).href);
      await page.locator('#connect-btn').click();
      await page.waitForFunction(()=>document.getElementById('ota-self-label').textContent==='F407');
      await page.locator('[data-tab="ota"]').click();
      if(scenario==='usb') {
        await page.evaluate(zipped=>{
          window.buildChecks={picked:[],paths:[],records:new Map()};
          window.savedRootRecord=et100RootRecord;
          et100RootRecord=async(action,handle,key=ET100_BUILD_ROOT_KEY)=>{
            if(action==='get')return buildChecks.records.get(key)||null;
            if(action==='put')buildChecks.records.set(key,handle);else buildChecks.records.delete(key);
          };
          function directory(parts=[]) {return {
            queryPermission:async()=> 'granted',
            getDirectoryHandle:async name=>{if(buildChecks.missing)throw Error('missing');return directory([...parts,name]);},
            getFileHandle:async name=>({getFile:async()=>{buildChecks.paths.push([...parts,name].join('/'));return new File([new Uint8Array(zipped)],name);}})
          };}
          window.showDirectoryPicker=async options=>{buildChecks.picked.push(options.id);return directory();};
        },Array.from(fixtures.zipped));
        await page.locator('#et100-pick-file').click();
        assert.equal(await page.locator('[data-et100-build="build"]').isVisible(),false);
        await page.locator('[data-et100-build="Release"]').click();
        await page.waitForFunction(()=>et100File?.device==='f407');
        await page.locator('#et100-pick-file').click();
        await page.locator('[data-et100-build="Debug"]').click();
        await page.waitForFunction(()=>buildChecks.paths.length===2&&et100File!==null);
        const checks=await page.evaluate(async()=>{
          f407BuildRootHandle=null;await getEt100DfuFile(otaSmpProfile,'Release');
          await getEt100DfuFile(ET100Smp.profiles[0]);
          const isolated=f407BuildRootHandle!==et100BuildRootHandle;
          buildChecks.missing=true;let error='';
          try{await getEt100DfuFile(otaSmpProfile,'Debug');}catch(e){error=e.message;}
          const cleared=f407BuildRootHandle===null&&!buildChecks.records.has(F407_BUILD_ROOT_KEY);
          buildChecks.missing=false;await getEt100DfuFile(otaSmpProfile,'Release');
          const picked=buildChecks.picked.slice(),paths=buildChecks.paths.slice();
          et100RootRecord=savedRootRecord;f407BuildRootHandle=null;et100BuildRootHandle=null;
          delete window.showDirectoryPicker;
          let unsupported='';try{await getEt100DfuFile();}catch(e){unsupported=e.message;}
          return {isolated,error,cleared,picked,paths,unsupported};
        });
        assert.deepEqual(checks.picked,['eup-f407-root','eup-et100-root','eup-f407-root']);
        assert.deepEqual(checks.paths.slice(0,4),['build/Release/dfu_application.zip','build/Debug/dfu_application.zip','build/Release/dfu_application.zip','fw/build_serial/output/dfu_application.zip']);
        assert.ok(checks.isolated&&checks.cleared);assert.match(checks.error,/build\/Debug.*eup_canbus_32f407/);
        assert.match(checks.unsupported,/本機檔案/);
      }
      assert.equal(await page.locator('[data-ota-mode="sensor"]').isDisabled(),true);
      assert.equal(await page.locator('#et100-query').textContent(),'讀取 F407 資訊');
      assert.match(await page.locator('#et100-flash-map h4').textContent(),/512 KB/);
      assert.match(await page.locator('#et100-nor-map h4').textContent(),/8 MB/);
      assert.equal(await page.locator('#et100-nor-map [data-name="Sensor OTA"]').count(),0);
      const partitions=await page.locator('#et100-flash-map .et100-partition').evaluateAll(rows=>rows.map(r=>[Number(r.dataset.start),Number(r.dataset.bytes)]));
      assert.deepEqual(partitions,[[0x08000000,0x8000],[0x08008000,0x4000],[0x0800c000,0x4000],[0x08010000,0x10000],[0x08020000,0x60000]]);
      const nor=await page.locator('#et100-nor-map .et100-partition').evaluateAll(rows=>rows.map(r=>[r.dataset.name,Number(r.dataset.start),Number(r.dataset.bytes)]));
      assert.deepEqual(nor,[
        ['Download journal',0,0x20000],['Candidate',0x020000,0x100000],['Rollback',0x120000,0x100000],
        ['Installer 結果',0x220000,0x2000],['App 已回報旗標',0x222000,0x2000],
        ['Logs／預留',0x224000,0x7ff000-0x224000],['NOR 自測',0x7ff000,0x1000]
      ]);
      assert.equal(nor.reduce((sum,[,,size])=>sum+size,0),8*1024*1024);
      assert.ok(nor.every(([,start],i)=>i===0||start===nor[i-1][1]+nor[i-1][2]),'NOR regions must be contiguous');
      await page.evaluate(async bytes=>{
        await et100LoadFile(new File([new Uint8Array(bytes)],'wrong-et100.bin'));
      },Array.from(fixtures.etBytes));
      assert.match(await page.locator('#et100-file-info').textContent(),/裝置.*eup_canbus_32f407.*檔案.*EUP_ET100/);
      await page.evaluate(async bytes=>{
        await et100LoadFile(new File([new Uint8Array(bytes)],'f407.bin'));mockF407.file=et100File;
        et100Client.io.now=()=>mockF407.clock;
        et100Client.io.sleep=async n=>{mockF407.clock+=n;await new Promise(r=>setTimeout(r,0));};
        toolSettings.clearOnConnect=true;addLog('sys','F407 KEEP LOG THROUGH USB RESET');
        window.f407Stages=[];
        const stage=et100Client.io.stage;et100Client.io.stage=t=>{f407Stages.push(t);stage(t);};
      },Array.from(fixtures.bytes));
      assert.equal(await page.locator('#et100-start').isEnabled(),true);
      await page.locator('#et100-start').click();
      if(scenario==='cancel') {
        await page.waitForFunction(()=>et100UsbSession!==null&&mockF407.reset);
        await page.evaluate(()=>disconnect());
      }
      await page.waitForFunction(()=>!et100Busy,{},{timeout:20000});
      const result=await page.evaluate(()=>({kind:document.getElementById('et100-result').dataset.kind,
        text:document.getElementById('et100-result').textContent,clock:mockF407.clock,opens:mockF407.opens,
        smp:mockF407.smp,commands:mockF407.commands,stages:f407Stages,connected,
        session:et100UsbSession!==null,hardware:gatewayVersionInfo?.hwVer,logs:document.getElementById('log').textContent}));
      if(scenario==='timeout') {assert.equal(result.kind,'error');assert.match(result.text,/180 秒/);assert.ok(result.clock<=180000);}
      else if(scenario==='cancel') {assert.equal(result.kind,'error');assert.equal(result.connected,false);assert.equal(result.opens.some(([n])=>n!=='original'),false);}
      else {
        assert.equal(result.kind,scenario==='rollback-reboot'?'rollback':'success',JSON.stringify(result));
        assert.equal(result.connected,true);assert.equal(result.hardware,'EUP_stm32f407_dk');
      }
      assert.equal(result.session,false);
      assert.equal(await page.evaluate(()=>mockF407.listeners.size),0,'reconnect listeners must be removed');
      assert.match(result.logs,/F407 KEEP LOG THROUGH USB RESET/);
      assert.equal(result.commands.some(([,c])=>/GSOTAPKG|DSOTAPKG|SENSOROTA/.test(c)),false);
      assert.equal(result.smp.some(([p])=>p==='wrong'),false,'wrong UID must receive no SMP');
      if(scenario==='rs232')assert.deepEqual(result.opens.map(([,b])=>b),[9600,115200,9600]);
      else {
        assert.ok(result.opens.every(([,b])=>b===9600));
        assert.ok(result.stages.includes('安裝中（USB 模式無即時進度）'));
        assert.doesNotMatch(result.logs,/讀取錯誤/);
      }
      if(['usb','poll-only','event-only'].includes(scenario)) {
        assert.ok(result.opens.some(([p])=>p==='wrong'));assert.ok(result.opens.some(([p])=>p==='next'));
      }
      if(scenario==='rollback-reboot')assert.ok(result.opens.some(([p])=>p==='final'));
      if(scenario==='timeout'||scenario==='cancel') {
        const count=result.opens.length;
        await page.evaluate(()=>{mockF407.ports=[mockF407.next];mockF407.emit('connect',mockF407.next);});
        await page.waitForTimeout(20);
        assert.equal(await page.evaluate(()=>mockF407.opens.length),count,'late event must not revive a finished session');
      }
      if(scenario==='usb') {
        await page.screenshot({path:'/tmp/f407_ota_desktop.png'});
        await page.setViewportSize({width:390,height:844});
        await page.evaluate(()=>{document.documentElement.setAttribute('data-theme','dark');et100FitDiagram();});
        await page.waitForTimeout(50);
        await page.screenshot({path:'/tmp/f407_ota_mobile_dark.png'});
        assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
      }
      assert.deepEqual(errors,[],scenario);
      console.log('PASS F407 mocked Web Serial:',scenario);
      await page.close();
    }
  } finally {await browser.close();}
}
(async()=>{const fixtures=await offline();if(!process.argv.includes('--offline'))await browserTests(fixtures);})().catch(e=>{console.error(e);process.exitCode=1;});
