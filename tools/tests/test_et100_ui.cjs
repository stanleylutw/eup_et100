// Playwright：只使用模擬 Web Serial，不存取 USB／EVB。
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || '/Users/stanley/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const root=path.resolve(__dirname,'../..');
(async()=>{
  const sensorFixture=await require('./et100_sensor_fixture.cjs')();
  const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL || 'chrome'});
  try {
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(()=>{
      window.mockSerial={opens:[],closes:0,resyncs:0,requests:[],uploaded:0,oldVersion:'0.1.0.0',polls:0,clock:0};
      const mock=window.mockSerial;
      const port={
        async open({baudRate}) {
          if(mock.failOpen) {mock.failOpen=false;throw Error('mock port open failure');}
          mock.opens.push(baudRate);mock.baud=baudRate;
          this.readable=new ReadableStream({start:c=>{mock.controller=c;}});
          this.writable=new WritableStream({write:bytes=>mock.write(bytes)});
        },
        async close() {if(this.readable.locked||this.writable.locked)throw Error('port close with locked streams');mock.closes++;this.readable=null;this.writable=null;}
      };
      Object.defineProperty(navigator,'serial',{value:{getPorts:async()=>[],requestPort:async()=>port}});
      mock.write=bytes=>{
        const text=new TextDecoder().decode(bytes);
        if(text==='\r\n') {mock.resyncs++;return;}
        if(text.startsWith('BLEGW$')) {
          mock.requests.push(text.trim());const fields=text.trim().split(';');
          if(fields[0]==='BLEGW$GCHK')mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;${mock.version || '0.8.6-dev'};0;${fields[1]}\r\n`));
          else if(fields[0]==='BLEGW$GGTIME')mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$GGTIME;1;37;${fields[1]}\r\n`));
          else if(fields[0]==='BLEGW$GFWINFO') {
            const info={project:'EUP_ET100',hardware:'ET100_EVB_DEV1.4',soc:'fr3068e_c',...mock.fwInfo};
            const body=mock.pre087?'0;':`1;${info.project};${info.hardware};0.8.7-dev;${info.soc};`;
            mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$GFWINFO;${body}${fields[1]}\r\n`));
          }
          else if(fields[0]==='BLEGW$OTABAUD')mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$OTABAUD;1;${fields[1]};${fields[2]}\r\n`));
          else if(fields[0]==='BLEGW$GSOTAPKG') {
            const m=mock.file?.metadata;
            const body=mock.packageState==='VALID'?`1;VALID;1;${m.package_length};${m.image_length};${m.product};${m.board};${m.soc};${m.firmware};${m.upload_sha256};${m.build_id}`:'1;EMPTY;0;-;-;-;-;-;-;-;-';
            mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$GSOTAPKG;${body};${fields[1]}\r\n`));
          } else if(fields[0]==='BLEGW$DSOTAPKG') {
            mock.packageState='EMPTY';mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$DSOTAPKG;1;OK;${fields[1]}\r\n`));
          } else if(fields[0]==='BLEGW$SENSOROTA') {
            mock.relayMac=fields[1];mock.relayState='WAIT_ADV';
            mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$SENSOROTA;1;${fields[1]};1;${mock.file.metadata.image_length};${fields[2]}\r\n`));
          } else if(fields[0]==='BLEGW$CSENSOROTA') {
            mock.relayState='CANCELLED';
            mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$CSENSOROTA;1;OK;${fields[1]}\r\n`));
          } else if(fields[0]==='BLEGW$GSENSOROTA') {
            const state=mock.relayState || 'IDLE',mac=mock.relayMac || '000000000000';
            const error=state==='CANCELLED'?19:0,done=state==='SUCCESS'?mock.file.metadata.image_length:0;
            mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$GSENSOROTA;1;${state};${mac};1;${done};${mock.file?.metadata?.image_length || 0};1000;${error};${error?'CANCELLED':'NONE'};${fields[1]}\r\n`));
          } else if(fields[0]==='BLEGW$GSENSOROTAFLOW') {
            const state=mock.relayState || 'IDLE',terminal=['SUCCESS','CANCELLED'].includes(state);
            mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$GSENSOROTAFLOW;1;${state};${terminal?'RESULT':'WAIT_ADV'};${state==='SUCCESS'?'PASS':terminal?'FAIL':'ACTIVE'};NONE;00000000;000000000000;${fields[1]}\r\n`));
          } else if(fields[0]==='BLEGW$GSENSOROTAMETA') {
            const m=mock.file.metadata,body=mock.relayMac?
              `1;${mock.relayMac};${m.product};${m.hardware};${m.firmware};${m.build_id};${m.soc};${m.board};${mock.relayMac};0000001F;00;NONE`:'0;NOT_READY';
            mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$GSENSOROTAMETA;${body};${fields[1]}\r\n`));
          }
          return;
        }
        if(!mock.decoder)mock.decoder=new ET100Smp.Decoder(p=>{
          const body=ET100Smp.uncbor(p.slice(8)),key=`${p[0]&7}/${p[4]*256+p[5]}/${p[7]}`;
          mock.requests.push(key);
          const slot={slot:0,version:mock.oldVersion,hash:new Uint8Array(32),confirmed:true,active:true};
          let response={};
          if(key==='0/1/0') {
            if(mock.reset) {mock.polls++;Object.assign(slot,{version:mock.file.version,hash:mock.file.tlvHash,confirmed:mock.polls>=2});}
            response={images:[slot]};
            if(mock.uploaded&&!mock.reset&&!mock.sensorUpload)response.images.push({slot:1,version:mock.file.version,hash:mock.file.tlvHash,bootable:true});
          } else if(key==='2/1/1') {
            if(mock.failRc)response={rc:mock.failRc};
            else {
              if(mock.sensorUpload) {
                if(body.image!==1)throw Error('every sensor chunk must specify image=1');
                if(body.off===0&&!ET100Smp.equal(body.sha,mock.file.sha))throw Error('wrong full-package sha');
              }
              mock.uploaded=body.off+body.data.length;response={rc:0,off:mock.uploaded};
              if(mock.sensorUpload&&mock.uploaded===mock.file.bytes.length)mock.packageState='VALID';
            }
          } else if(key==='2/1/0') {
            if(!ET100Smp.equal(body.hash,mock.file.tlvHash)||body.confirm!==false)throw Error('wrong Test hash');
          } else if(key==='2/0/5')mock.reset=true;
          else throw Error('unexpected request '+key);
          mock.controller.enqueue(ET100Smp.frame(ET100Smp.packet((p[0]&7)+1,p[4]*256+p[5],p[7],p[6],response)));
          if(key==='2/0/5'&&mock.instProtocol) {
            const txn=Array.from(mock.file.sha.slice(0,4),v=>v.toString(16).padStart(2,'0')).join('').toUpperCase();
            for(const [event,fields] of [['HELLO','0.8.11;PENDING'],['BACKUP','SKIP'],['WRITE',`${mock.file.bytes.length};${mock.file.bytes.length}`],['BOOT','TRIAL;9600']]) {
              const raw=`ET100$INST;1;L;${event};${txn};${fields}`;
              const crc=ET100Smp.crc16(new TextEncoder().encode(raw)).toString(16).toUpperCase().padStart(4,'0');
              mock.controller.enqueue(new TextEncoder().encode(`${raw}*${crc}\r\n`));
            }
          }
        });
        mock.decoder.feed(bytes);
      };
    });
    await page.goto(pathToFileURL(path.join(root,'tools/rs232_tester.html')).href);
    await page.locator('[data-tab="ota"]').click();
    assert.equal(await page.locator('[aria-label="OTA 目標"] button').count(),2);
    assert.equal(await page.locator('#ota-self-label').textContent(),'Gateway');
    await page.locator('#connect-btn').click();
    await page.waitForFunction(()=>document.getElementById('ota-self-label').textContent==='ET-100');
    assert.equal(await page.locator('#ota-self-target svg').count(),1,'ET-100 retains Gateway icon');
    assert.equal(await page.locator('#ota-self-target').getAttribute('data-ota-mode'),'et100');
    await page.locator('[data-tab="ble"]').click();
    await page.locator('.ble-qbtn[data-cmd="GGTIME"]').click();
    await page.waitForFunction(()=>bleGtimeMs===37000);
    assert.ok(await page.evaluate(()=>mockSerial.requests.some(s=>s.startsWith('BLEGW$GGTIME;'))),'ET-100 must send normal BLE commands');
    assert.match(await page.locator('#log').textContent(),/AT\$PD=BLEGW\$GGTIME;1;37;/,'ASCII response must reach normal parser through SMP decoder');
    await page.locator('[data-tab="ota"]').click();
    assert.equal(await page.locator('#et100-dfu-path').count(),0,'no misleading static path hint');
    await page.locator('#et100-pick-file').click();
    assert.equal(await page.locator('#et100-build-menu').isVisible(),true);
    assert.equal(await page.locator('#et100-build-menu [role="menuitem"]:visible').count(),2);
    for(const preset of ['Release','Debug'])assert.equal(await page.locator(`[data-et100-build="${preset}"]`).isVisible(),false,'F407 shortcuts stay hidden on ET-100');
    assert.match(await page.locator('[data-et100-build="build"]').textContent(),/fw\/build_serial\/output/);
    await page.locator('#et100-load-section').screenshot({path:'/tmp/et100_build_picker_menu.png'});
    await page.locator('#et100-pick-file').click();
    assert.equal(await page.locator('#et100-build-menu').isVisible(),false);
    const rootChecks=await page.evaluate(async()=>{
      const picker=window.showDirectoryPicker, record=et100RootRecord;
      const gateway=gatewayBuildRootHandle, sensor=sensorBuildRootHandle;
      const paths=[], actions=[], ids=[];
      let stored=null, missing=false;
      const root={queryPermission:async()=> 'granted',
        async getDirectoryHandle(name){paths.push(name);if(missing)throw Error('missing');return this;},
        async getFileHandle(name){paths.push(name);return {getFile:async()=>new File(['dfu'],name)};}};
      try {
        et100BuildRootHandle=null;
        et100RootRecord=async(action,handle)=>{
          actions.push([action,ET100_BUILD_ROOT_KEY]);
          if(action==='put')stored=handle;
          if(action==='delete')stored=null;
          return stored;
        };
        window.showDirectoryPicker=async options=>{ids.push(options.id);return root;};
        const first=await getEt100DfuFile();
        et100BuildRootHandle=null; // Simulate a reload using the persisted handle.
        const second=await getEt100DfuFile();
        missing=true;
        let error='';try{await getEt100DfuFile();}catch(err){error=err.message;}
        return {paths,actions,ids,names:[first.name,second.name],error,cleared:et100BuildRootHandle===null&&stored===null,
          isolated:gateway===gatewayBuildRootHandle&&sensor===sensorBuildRootHandle};
      } finally {window.showDirectoryPicker=picker;et100RootRecord=record;et100BuildRootHandle=null;}
    });
    assert.deepEqual(rootChecks.ids,['eup-et100-root']);
    assert.deepEqual(rootChecks.names,['dfu_application.zip','dfu_application.zip']);
    assert.deepEqual(rootChecks.paths.slice(0,8),['fw','build_serial','output','dfu_application.zip','fw','build_serial','output','dfu_application.zip']);
    assert.ok(rootChecks.actions.every(([,key])=>key==='eup-et100-root'));
    assert.equal(rootChecks.cleared,true);assert.equal(rootChecks.isolated,true);
    assert.match(rootChecks.error,/eup_et100 專案根目錄/);
    const pickerChecks=await page.evaluate(async()=>{
      const original=window.showOpenFilePicker, options=[], loaded=[];
      try {
        window.showOpenFilePicker=async opts=>{
          options.push(opts);
          return [{getFile:async()=>new File(['test'],'test.zip')}];
        };
        for (const target of ['et100','sensor','gateway'])
          await chooseOtaLocalFile(target,'et100-file',file=>loaded.push(file.name));
        await chooseOtaLocalFile('et100','et100-file',()=>loaded.push('stale'),()=>false);
        window.showOpenFilePicker=async()=>{throw new DOMException('cancel','AbortError');};
        await chooseOtaLocalFile('et100','et100-file',()=>loaded.push('cancelled'));
        const input=document.getElementById('et100-file'), click=input.click;
        let fallback=0;
        input.click=()=>{fallback++;};
        try {
          window.showOpenFilePicker=undefined;
          await chooseOtaLocalFile('et100','et100-file',()=>loaded.push('fallback'));
          window.showOpenFilePicker=async()=>{throw new DOMException('blocked','SecurityError');};
          await chooseOtaLocalFile('sensor','et100-file',()=>loaded.push('blocked'));
        } finally {input.click=click;}
        return {options,loaded,fallback};
      } finally {window.showOpenFilePicker=original;}
    });
    assert.deepEqual(pickerChecks.options.map(o=>o.id),['et100-dfu','sensor-dfu','gateway-dfu','et100-dfu']);
    assert.deepEqual(pickerChecks.loaded,['test.zip','test.zip','test.zip']);
    assert.equal(pickerChecks.fallback,2);
    assert.deepEqual(pickerChecks.options[0].types[0].accept,{'application/zip':['.zip'],'application/octet-stream':['.bin']});
    assert.deepEqual(pickerChecks.options[1].types[0].accept,{'application/zip':['.zip']});
    assert.deepEqual(pickerChecks.options[2].types[0].accept,{'application/octet-stream':['.bin']});
    const sections=await page.evaluate(()=>({
      load:document.getElementById('et100-load-section').getBoundingClientRect(),
      version:document.getElementById('et100-version-section').getBoundingClientRect(),
      pick:document.getElementById('et100-pick-file').getBoundingClientRect(),
      query:document.getElementById('et100-query').getBoundingClientRect(),
      start:document.getElementById('et100-start').getBoundingClientRect()
    }));
    assert.ok(sections.load.y<sections.version.y,'file section comes before version, like Gateway');
    assert.equal(sections.pick.width,sections.query.width,'file and query buttons share full width');
    assert.equal(sections.start.width,sections.pick.width,'idle start button is full width');
    assert.equal(await page.locator('#et100-cancel').isVisible(),false);
    assert.equal(await page.locator('#et100-panel progress').count(),0,'no separate native progress bar');
    assert.equal(await page.locator('#et100-phase').isVisible(),false);
    assert.deepEqual(await page.locator('#et100-file-info .ota-meta-label').allTextContents(),['韌體大小','產品','硬體','韌體','建置時間']);
    await page.locator('#et100-panel').screenshot({path:'/tmp/et100_gateway_style_idle.png'});
    assert.equal(await page.locator('.parsed-panel > #et100-viz').isVisible(),true);
    assert.equal(await page.locator('#et100-panel .et100-memory').count(),0,'diagram belongs in main area, not sidebar');
    assert.equal(await page.locator('#flash-ota-viz').isVisible(),false);
    assert.match(await page.locator('#et100-flash-map').textContent(),/MCU 內建 Flash · 2 MB/);
    assert.match(await page.locator('#et100-flash-map').textContent(),/禁止擦除／寫入/);
    assert.equal(await page.locator('#et100-flash-map [data-name="ROM B 資訊段"] small:last-child').isVisible(),false);
    assert.match(await page.locator('#et100-flash-map').textContent(),/1792 KB/);
    assert.match(await page.locator('#et100-nor-map').textContent(),/16 MB（EVB）/);
    assert.match(await page.locator('#et100-nor-map').textContent(),/11\.6 MB/);
    assert.equal(await page.evaluate(()=>et100Capacity(32*1024*1024-0x462000)),'27.6 MB');
    assert.equal(await page.evaluate(()=>et100Capacity(2*1024*1024)),'2 MB');
    assert.deepEqual(await page.locator('#et100-flash-map .et100-partition strong').allTextContents(),
      ['開機資訊','原廠 OTA 資訊','Bootloader','主程式','系統設定','更新狀態']);
    assert.deepEqual(await page.locator('#et100-nor-map .et100-partition strong').allTextContents(),
      ['下載紀錄','新版韌體暫存','舊版韌體備份','感測器更新套件','安裝結果紀錄','日誌與預留空間']);
    assert.match(await page.locator('#et100-flash-map [data-name="ROM A 資訊段"]').textContent(),/僅更新 Installer 時寫入/);
    assert.equal(await page.locator('#et100-viz select, #et100-viz details, #et100-viz ol, #et100-viz p').count(),0,'no extra selector, explanations or workflow list');
    const layouts=await page.evaluate(()=>[et100FlashPartitions,et100StoragePartitions('evb'),et100StoragePartitions('product')]);
    for(const [i,parts] of layouts.entries()) {
      assert.equal(parts.reduce((sum,p)=>sum+p[2],0),[2,16,32][i]*1024*1024);
      for(let j=1;j<parts.length;j++)assert.equal(parts[j][1],parts[j-1][1]+parts[j-1][2],'contiguous, no overlap');
    }
    await page.locator('#et100-query').click();
    await page.waitForFunction(()=>document.querySelector('#et100-device').textContent.includes('0.1.0.0'));
    const sameBaud=await page.evaluate(async()=>{
      const before={opens:mockSerial.opens.length,closes:mockSerial.closes,resyncs:mockSerial.resyncs};
      const currentReader=reader,currentWriter=writer;
      for(let i=0;i<50;i++)await et100Reopen(9600);
      return {before,after:{opens:mockSerial.opens.length,closes:mockSerial.closes,resyncs:mockSerial.resyncs},
        streamsSame:reader===currentReader&&writer===currentWriter};
    });
    assert.deepEqual(sameBaud.after,sameBaud.before,'50 same-baud requests keep the port open without sending CRLF');
    assert.equal(sameBaud.streamsSame,true,'same-baud reader/writer are preserved');
    assert.equal(await page.locator('#et100-device .ota-meta-row:not(.ota-meta-extra)').count(),4,'same four-row device summary as Gateway');
    assert.equal(await page.locator('#et100-device-toggle').getAttribute('aria-expanded'),'false');
    assert.equal(await page.locator('#et100-device [data-field="確認狀態"]').isVisible(),false);
    await page.locator('#et100-device-toggle').click();
    assert.equal(await page.locator('#et100-device-toggle').getAttribute('aria-expanded'),'true');
    assert.equal(await page.locator('#et100-device [data-field="確認狀態"]').isVisible(),true);
    await page.locator('#et100-device-header').click();
    assert.match(await page.locator('#log').textContent(),/BLEGW\$GFWINFO;[A-Z0-9]{4}/);
    assert.match(await page.locator('#log').textContent(),/AT\$PD=BLEGW\$GFWINFO;1;EUP_ET100;ET100_EVB_DEV1.4;/);
    await page.locator('#et100-file').setInputFiles(path.join(root,'fw/build_serial/output/dfu_application.zip'));
    await page.waitForFunction(()=>!document.querySelector('#et100-start').disabled);
    assert.match(await page.locator('#et100-file-info').textContent(),/board／soc 通過/);
    assert.equal(await page.locator('#et100-file-info .ota-meta-row:not(.ota-meta-extra)').count(),5);
    assert.equal(await page.locator('#et100-file-info [data-field="Manifest"]').isVisible(),false);
    await page.locator('#et100-file-toggle').click();
    assert.equal(await page.locator('#et100-file-info [data-field="Manifest"]').isVisible(),true);
    await page.locator('#et100-file-header').click();
    assert.equal(await page.locator('#et100-file').isVisible(),false,'native file input replaced by Gateway-style button');
    assert.equal(await page.locator('#et100-pick-file').isVisible(),true);
    await page.locator('#et100-panel').screenshot({path:'/tmp/et100_metadata_sidebar.png'});
    assert.match(await page.locator('#et100-source-name').textContent(),/dfu_application.zip/);
    assert.ok(await page.locator('[data-name="Candidate"] .et100-slot-image').evaluate(el=>parseFloat(el.style.width)>0),'file footprint shown before upload');
    assert.match(await page.locator('#et100-candidate-state').textContent(),/已提交未知/);
    await page.evaluate(()=>{
      et100Busy=true;et100Client.running=true;et100Started=Date.now();
      et100Client.io.stage('上傳');
      et100Client.io.progress(Math.ceil(et100File.bytes.length*0.53),et100File.bytes.length);
    });
    assert.match(await page.locator('#et100-start').textContent(),/上傳\s+53%/);
    assert.equal(await page.locator('#et100-start').evaluate(el=>el.classList.contains('in-progress')),true);
    assert.equal(await page.locator('#et100-start').evaluate(el=>el.style.getPropertyValue('--progress')),'53.0%');
    assert.match(await page.locator('#et100-start').evaluate(el=>getComputedStyle(el).backgroundImage),/linear-gradient/);
    await page.locator('#et100-panel').screenshot({path:'/tmp/et100_gateway_progress.png'});
    await page.evaluate(()=>et100Client.io.stage('驗證 Candidate'));
    assert.equal(await page.locator('#et100-start').textContent(),'驗證中…');
    assert.equal(await page.locator('#et100-start').evaluate(el=>el.classList.contains('verify-progress')),true);
    await page.evaluate(()=>{et100Busy=false;et100Client.running=false;et100Controls();});
    assert.equal(await page.locator('#et100-start').textContent(),'開始 OTA');
    assert.equal(await page.locator('#et100-start').evaluate(el=>el.classList.contains('in-progress')),false);
    await page.evaluate(()=>{
      et100Client.io.stage('上傳');et100Client.io.progress(4096,et100File.bytes.length);
    });
    assert.match(await page.locator('#et100-candidate-state').textContent(),/已提交 4,096/);
    assert.ok(await page.locator('[data-name="Candidate"] .et100-slot-committed').evaluate(el=>parseFloat(el.style.width)>0),'ACK bytes fill candidate');
    assert.equal(await page.locator('#et100-route-upload').getAttribute('data-active'),'true');
    await page.evaluate(()=>et100Client.io.stage('安裝中'));
    assert.match(await page.locator('#et100-stat-rows').textContent(),/安裝中/);
    assert.equal(await page.locator('#et100-route-install').getAttribute('data-waiting'),'true');
    assert.match(await page.locator('#et100-stat-rows').textContent(),/裝置未回報/);
    await page.screenshot({path:'/tmp/et100_smp_desktop.png',fullPage:true});
    await page.setViewportSize({width:390,height:844});
    await page.locator('#et100-viz').scrollIntoViewIfNeeded();
    await page.screenshot({path:'/tmp/et100_memory_flow.png'});
    for(const stage of ['cancelled','rollback']) {
      await page.evaluate(s=>et100Flow(s),stage);
      assert.match(await page.locator('#et100-stat-rows').textContent(),stage==='cancelled'?/取消傳輸/:/推定回滾/);
    }
    await page.locator('#et100-flash-map').scrollIntoViewIfNeeded();
    await page.screenshot({path:'/tmp/et100_smp_mobile.png',fullPage:true});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'horizontal page overflow');
    await page.screenshot({path:'/tmp/et100_memory_mobile.png'});
    await page.setViewportSize({width:320,height:740});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'320px horizontal page overflow');
    await page.setViewportSize({width:1440,height:1000});
    await page.evaluate(()=>et100Client.io.stage('安裝中'));
    await page.waitForFunction(()=>Math.abs(document.querySelector('.et100-viz-layout').getBoundingClientRect().height-
      document.querySelector('.et100-fit-frame').getBoundingClientRect().height)<1);
    for (const id of ['et100-flash-map','et100-nor-map']) {
      const clip=await page.locator('#'+id).evaluate(el=>({clipped:el.lastElementChild.lastElementChild.getBoundingClientRect().bottom>el.getBoundingClientRect().bottom+1,
        rows:[...el.lastElementChild.children].map(row=>[row.dataset.name,row.clientHeight,row.scrollHeight]),height:el.clientHeight}));
      assert.equal(clip.clipped,false,`${id} final partition must not be clipped: ${JSON.stringify(clip)}`);
    }
    await page.locator('#et100-flash-map').scrollIntoViewIfNeeded();
    await page.screenshot({path:'/tmp/et100_memory_desktop.png'});
    await page.setViewportSize({width:1920,height:1200});
    await page.waitForFunction(()=>Math.abs(document.querySelector('.et100-viz-layout').getBoundingClientRect().height-
      document.querySelector('.et100-fit-frame').getBoundingClientRect().height)<1);
    const visualStyle=await page.evaluate(()=>{
      // The standard map is empty until GCHK identifies an nRF board.
      const reference=document.createElement('div');reference.className='flash-viz-slot';
      reference.innerHTML='<span class="flash-viz-slot-name">Reference</span>';
      document.querySelector('#flash-viz-slots').append(reference);
      const style=selector=>getComputedStyle(document.querySelector(selector));
      const pairs=[
        ['.et100-memory h4','.flash-viz-chip-name',['fontSize','fontWeight']],
        ['#et100-source .flash-viz-source-header','#flash-viz-source-title',['fontSize','fontWeight','padding']],
        ['#et100-source .flash-viz-source-note','#flash-viz-source-note',['fontSize']],
        ['.et100-partition strong','.flash-viz-slot-name',['fontSize','fontWeight']],
        ['#et100-stats .flash-viz-info-label','#flash-ota-viz .flash-viz-info-label',['fontSize','padding']],
        ['#et100-stats .flash-viz-info-value','#flash-ota-viz .flash-viz-info-value',['fontSize','padding']]
      ];
      const renderedFont=selector=>{
        const el=document.querySelector(selector),layout=el.closest('.et100-viz-layout');
        const scale=layout?layout.getBoundingClientRect().width/layout.offsetWidth:1;
        return parseFloat(style(selector).fontSize)*scale;
      };
      const result=pairs.flatMap(([a,b,props])=>props.map(prop=>({a,b,prop,
        same:prop==='fontSize'?Math.abs(renderedFont(a)-renderedFont(b))<0.1:style(a)[prop]===style(b)[prop]})));
      reference.remove();return result;
    });
    assert.deepEqual(visualStyle.filter(check=>!check.same),[],'ET-100 uses shared Device GATT OTA typography and table spacing');
    assert.equal(await page.locator('#et100-viz').evaluate(el=>el.scrollHeight>el.clientHeight+1),false,'desktop diagram fits visualization panel');
    for (const id of ['et100-flash-map','et100-nor-map','et100-stats']) {
      assert.equal(await page.locator('#'+id).evaluate(el=>el.scrollHeight>el.clientHeight+1),false,`${id} contents fit compact frame`);
    }
    const positions=await page.evaluate(()=>['et100-source','et100-nor-map','et100-flash-map','et100-stats'].map(id=>document.getElementById(id).getBoundingClientRect().left));
    const partitionLayout=await page.locator('.et100-partition').evaluateAll(rows=>rows.map(row=>{
      const box=row.getBoundingClientRect(), capacity=row.querySelector('.et100-partition-capacity').getBoundingClientRect();
      const title=row.querySelector('.et100-partition-title').getBoundingClientRect();
      const details=[...row.querySelectorAll(':scope > small:not([hidden])')].filter(el=>getComputedStyle(el).display!=='none').map(el=>el.getBoundingClientRect());
      const bottom=Math.max(title.bottom,...details.map(el=>el.bottom));
      return {center:Math.abs((capacity.top+capacity.bottom-box.top-box.bottom)/2),
        contentCenter:Math.abs((title.top+bottom-box.top-box.bottom)/2),
        right:box.right-capacity.right, fits:row.scrollHeight<=row.clientHeight+1};
    }));
    assert.ok(partitionLayout.every(row=>row.center<2&&row.contentCenter<3&&row.right<12&&row.fits),'partition content and capacities vertically centered like GATT OTA; no clipped details');
    assert.ok(await page.locator('.et100-partition').evaluateAll(rows=>rows.every(row=>
      [...row.querySelectorAll('small')].every(el=>getComputedStyle(el).display==='none') &&
      getComputedStyle(row.querySelector('.et100-partition-title')).justifyContent==='center')),
      'ET-100 Flash/NOR maps show centered section names and right-side capacities only');
    assert.ok(positions.every((x,i)=>i===0||x>positions[i-1]),'desktop: file → NOR → MCU → status');
    await page.locator('.et100-viz-layout').screenshot({path:'/tmp/et100_memory_diagram.png'});
    for (const [width,height] of [[1920,900],[1440,900],[1366,768],[1280,800]]) {
      await page.setViewportSize({width,height});
      await page.waitForFunction(()=>{
        const frame=document.querySelector('.et100-fit-frame').getBoundingClientRect();
        const diagram=document.querySelector('.et100-viz-layout').getBoundingClientRect();
        return diagram.width>0&&Math.abs(diagram.height-frame.height)<1&&diagram.bottom<=frame.bottom+1&&diagram.right<=frame.right+1;
      });
      const fit=await page.evaluate(()=>{
        const viz=document.getElementById('et100-viz');
        const log=document.querySelector('.log-wrap');
        const currentLog=log.getBoundingClientRect().height;
        viz.hidden=true;
        const sharedLog=log.getBoundingClientRect().height;
        viz.hidden=false;
        const frame=viz.querySelector('.et100-fit-frame').getBoundingClientRect();
        const diagram=viz.querySelector('.et100-viz-layout').getBoundingClientRect();
        const a=document.querySelector('[data-name="ROM A 資訊段"]').getBoundingClientRect();
        const b=document.querySelector('[data-name="ROM B 資訊段"]').getBoundingClientRect();
        return {overflow:viz.scrollHeight>viz.clientHeight+1,a:a.height,b:b.height,currentLog,sharedLog,
          bodyFont:parseFloat(getComputedStyle(viz.querySelector('.et100-partition-capacity')).fontSize)*diagram.width/viz.querySelector('.et100-viz-layout').offsetWidth,
          detailFont:parseFloat(getComputedStyle(viz.querySelector('.et100-partition small')).fontSize)*diagram.width/viz.querySelector('.et100-viz-layout').offsetWidth,
          detailColor:getComputedStyle(viz.querySelector('.et100-partition small')).color,
          fills:Math.abs(diagram.height-frame.height)<1,
          fits:diagram.top>=frame.top-1&&diagram.left>=frame.left-1&&diagram.bottom<=frame.bottom+1&&diagram.right<=frame.right+1,
          clipped:[...viz.querySelectorAll('.et100-partition')].filter(row=>row.scrollHeight>row.clientHeight+1).map(row=>row.dataset.name)};
      });
      assert.equal(fit.overflow,false,`${width}x${height}: complete diagram fits available panel`);
      assert.ok(Math.abs(fit.currentLog-fit.sharedLog)<1,`${width}x${height}: shared log height must not change for ET-100`);
      assert.equal(fit.fits,true,`${width}x${height}: scaled diagram stays inside its frame`);
      assert.equal(fit.fills,true,`${width}x${height}: diagram fills the available Flash UI height`);
      assert.ok(Math.abs(fit.bodyFont-13)<0.1,`${width}x${height}: rendered body type remains 13px like GATT OTA`);
      assert.ok(Math.abs(fit.detailFont-11)<0.1,`${width}x${height}: addresses and details use 11px type`);
      assert.equal(fit.detailColor,'rgb(174, 183, 196)','addresses and details use muted gray');
      assert.ok(await page.evaluate(()=>Math.abs(document.querySelector('.et100-viz-layout').getBoundingClientRect().left-
        document.querySelector('.et100-fit-frame').getBoundingClientRect().left)<1),`${width}x${height}: diagram starts at the standard left edge`);
      assert.ok(Math.abs(fit.a-fit.b)<1,`${width}x${height}: A/B info sections have equal height`);
      assert.ok(fit.a<40 && fit.b<40,`${width}x${height}: both 4 KB sections stay compact`);
      assert.ok(await page.evaluate(()=>{
        const height=name=>document.querySelector(`#et100-flash-map [data-name="${name}"]`).getBoundingClientRect().height;
        return Math.abs(height('Boot state')-height('ROM A 資訊段'))<1;
      }),`${width}x${height}: 16 KB boot state uses the same compact height as 4 KB info`);
      assert.ok(await page.evaluate(()=>{
        const result=document.querySelector('#et100-nor-map [data-name="Installer 結果"]').getBoundingClientRect();
        const info=document.querySelector('#et100-flash-map [data-name="ROM A 資訊段"]').getBoundingClientRect();
        const logs=document.querySelector('#et100-nor-map [data-name="Logs／預留"]').getBoundingClientRect();
        return Math.abs(result.height-info.height)<1 && logs.height>result.height;
      }),`${width}x${height}: 8 KB result row is compact and reserved space receives more height`);
      assert.deepEqual(fit.clipped,[],`${width}x${height}: partition text fits`);
      if(width===1366) {
        await page.locator('.et100-viz-layout').screenshot({path:'/tmp/et100_memory_1366.png'});
        await page.screenshot({path:'/tmp/et100_fixed_log_desktop.png'});
      }
    }
    await page.setViewportSize({width:1440,height:1000});
    await page.evaluate(()=>{
      mockSerial.file=et100File;mockSerial.instProtocol=true;et100Client.io.now=()=>mockSerial.clock;
      et100Client.io.sleep=async n=>{mockSerial.clock+=n;};
    });
    await page.locator('#et100-start').click();
    await page.waitForFunction(()=>document.querySelector('#et100-result').dataset.kind==='success');
    assert.match(await page.locator('#et100-result').textContent(),/更新成功/);
    assert.match(await page.locator('#et100-app-state').textContent(),/已確認/);
    assert.equal(await page.locator('#et100-device .ota-meta-row:not(.ota-meta-extra)').count(),4,'metadata preserved after update');
    assert.match(await page.locator('#et100-device [data-field="品牌"]').textContent(),/Freqchip/);
    assert.match(await page.locator('#et100-stat-rows').textContent(),/更新成功/);
    assert.equal(await page.locator('#et100-phase').textContent(),'更新成功');
    const log=await page.locator('#log').textContent();
    assert.match(log,/SMP upload ACK/);
    assert.match(log,/SMP Test ACK/);
    assert.match(log,/SMP Reset ACK/);
    assert.match(log,/ET100\$INST;1;L;HELLO;/);
    assert.match(log,/備份目前版本/);
    assert.match(log,/slot 0 .*confirmed=false/);
    assert.match(log,/slot 0 .*confirmed=true/);
    assert.match(log,/更新成功：.*已確認/);
    const installPos=log.lastIndexOf('ET-100：安裝中');
    const checkPos=log.lastIndexOf('ET-100：新版程式檢查中');
    assert.ok(installPos>=0&&checkPos>installPos&&log.lastIndexOf('ET-100：更新成功：')>checkPos,'install → checking → success follows actual state');
    assert.ok((log.match(/SMP upload ACK/g)||[]).length<=13,'upload log is throttled');
    assert.match(await page.locator('#et100-file-info').textContent(),/裝置／檔案硬體一致/);
    let state=await page.evaluate(()=>({opens:mockSerial.opens,requests:mockSerial.requests,polls:mockSerial.polls}));
    assert.deepEqual(state.opens,[9600,115200,9600]);
    assert.equal(await page.evaluate(()=>mockSerial.resyncs),2,'CRLF sent only after the two real baud transitions');
    assert.ok(state.requests.includes('2/0/5'));assert.equal(state.polls,2);
    assert.ok(!state.requests.some(s=>s.includes('ZVERO')||s.includes('SOTASTAGE')));
    await page.evaluate(()=>{mockSerial.failRc=10;mockSerial.reset=false;mockSerial.uploaded=0;});
    await page.locator('#et100-start').click();await page.waitForFunction(()=>document.querySelector('#et100-result').dataset.kind==='error');
    assert.match(await page.locator('#et100-result').textContent(),/EBUSY/);
    assert.match(await page.locator('#et100-stat-rows').textContent(),/流程停止/);
    for(const fwInfo of [{project:'OTHER'},{hardware:'ET100_EVB_OTHER'},{soc:'nrf52840'}]) {
      const start=await page.evaluate(info=>{
        mockSerial.fwInfo=info;mockSerial.failRc=0;mockSerial.reset=false;mockSerial.uploaded=0;
        return mockSerial.requests.length;
      },fwInfo);
      await page.locator('#et100-start').click();
      await page.waitForFunction(()=>document.querySelector('#et100-result').textContent.includes('裝置／檔案硬體不符'));
      const requests=await page.evaluate(n=>mockSerial.requests.slice(n),start);
      assert.ok(!requests.some(s=>s==='2/1/1'||s==='2/1/0'||s==='2/0/5'||s.includes('OTABAUD')),'mismatch must not switch baud or mutate device');
    }
    await page.evaluate(()=>{mockSerial.pre087=true;mockSerial.fwInfo={};mockSerial.reset=false;mockSerial.uploaded=0;mockSerial.polls=0;});
    await page.locator('#et100-start').click();
    await page.waitForFunction(()=>document.querySelector('#et100-result').dataset.kind==='success');
    assert.match(await page.locator('#et100-file-info').textContent(),/裝置韌體早於 0.8.7，無法比對硬體/);
    assert.match(await page.locator('#log').textContent(),/AT\$PD=BLEGW\$GFWINFO;0;[A-Z0-9]{4}/);
    await page.evaluate(()=>processFrame('AT$PD=BLEGW$GCHK;1;C03232325903;EUP;MS88SF2_nrf52840;1.0.0;0;TEST'));
    assert.equal(await page.locator('#ota-self-label').textContent(),'Gateway');
    assert.equal(await page.locator('[aria-label="OTA 目標"] button').count(),2);
    assert.equal(await page.locator('#ota-rs232-panel').isVisible(),true);
    assert.equal(await page.locator('#et100-viz').isVisible(),false);
    assert.equal(await page.locator('#flash-ota-viz').isVisible(),true);
    await page.locator('[data-ota-mode="sensor"]').click();assert.equal(await page.locator('#ota-device-submode').isVisible(),true);
    await page.evaluate(()=>processFrame('AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.7-dev;0;TEST'));
    assert.equal(await page.locator('#ota-self-label').textContent(),'ET-100');
    assert.equal(await page.locator('[data-ota-mode="sensor"]').evaluate(el=>el.classList.contains('active')),true,'identity update preserves Device tab');
    assert.equal(await page.locator('#et100-sensor-store').isVisible(),true);
    assert.equal(await page.locator('#ota-device-submode').isVisible(),true,'Device keeps RS232/GATT choices on ET-100');
    assert.equal(await page.locator('[data-device-submode="rs232"]').isDisabled(),false);
    assert.equal(await page.locator('[data-device-submode="rs232"]').evaluate(el=>el.classList.contains('active')),true);
    assert.equal(await page.locator('[data-device-submode="gatt"]').isDisabled(),true,'unimplemented ET-100 relay cannot start');
    assert.match(await page.locator('[data-device-submode="gatt"]').getAttribute('title'),/尚未支援/);
    assert.equal(await page.locator('#et100-start').textContent(),'儲存到 ET-100');
    assert.equal(await page.locator('#et100-version-section').isVisible(),false,'Device hides gateway identity section');
    assert.equal(await page.locator('#et100-package-download').isVisible(),true);
    assert.equal(await page.locator('#et100-package-download').isDisabled(),true);
    assert.deepEqual(await page.locator('#et100-file-info .ota-meta-row:not(.ota-meta-extra) .ota-meta-label').allTextContents(),['韌體大小','產品','Board','韌體','建置時間']);
    await page.locator('#et100-pick-file').click();
    assert.equal(await page.locator('#et100-sensor-menu').isVisible(),true);
    assert.equal(await page.locator('#et100-sensor-menu [role="menuitem"]').count(),3);
    await page.locator('#et100-pick-file').click();
    assert.equal(await page.locator('#et100-sensor-menu').isVisible(),false);
    assert.equal(await page.locator('#et100-start').isDisabled(),true,'self image must not become sensor package');
    const beforeWrongZip=await page.evaluate(()=>mockSerial.requests.length);
    assert.equal(await page.locator('#et100-build-menu').isVisible(),false,'Sensor target must not show ET-100 build menu');
    await page.locator('#et100-file').setInputFiles(path.join(root,'fw/build_serial/output/dfu_application.zip'));
    await page.waitForFunction(()=>document.getElementById('et100-file-info').textContent.includes('檔案無效'));
    assert.equal(await page.locator('#et100-start').isDisabled(),true);
    assert.equal(await page.evaluate(()=>mockSerial.requests.length),beforeWrongZip,'ET-100 zip rejected as sensor without device access');
    await page.locator('#et100-file').setInputFiles({name:'p1_dfu.zip',mimeType:'application/zip',buffer:Buffer.from(sensorFixture.zip)});
    await page.waitForFunction(()=>!document.getElementById('et100-start').disabled);
    assert.match(await page.locator('#et100-file-info').textContent(),/EUP_AIoT_P1/);
    assert.equal(await page.locator('#et100-file-info .meta-compare-badge').count(),0,'no comparison before VALID NOR snapshot');
    assert.equal(await page.locator('#et100-package-download').isDisabled(),false);
    const [download]=await Promise.all([page.waitForEvent('download'),page.locator('#et100-package-download').click()]);
    assert.equal(download.suggestedFilename(),'p1_dfu.sensor-ota.bin');
    assert.deepEqual(fs.readFileSync(await download.path()),Buffer.from(await page.evaluate(()=>Array.from(et100Files[1].bytes))),'download contains the exact relay package');
    assert.ok(await page.locator('[data-name="Sensor OTA"] .et100-slot-image').evaluate(el=>parseFloat(el.style.width)>0));
    assert.equal(await page.locator('[data-name="Candidate"] .et100-slot-image').evaluate(el=>parseFloat(el.style.width)),0);
    const sensorStart=await page.evaluate(()=>{
      mockSerial.pre087=false;mockSerial.fwInfo={};mockSerial.reset=false;mockSerial.uploaded=0;
      mockSerial.sensorUpload=true;mockSerial.packageState='EMPTY';mockSerial.file=et100File;
      return mockSerial.requests.length;
    });
    await page.locator('#et100-start').click();
    await page.waitForFunction(()=>document.getElementById('et100-result').dataset.kind==='stored');
    assert.match(await page.locator('#et100-result').textContent(),/Sensor 套件已儲存，驗證通過/);
    assert.match(await page.locator('#et100-package-info').textContent(),/VALID/);
    assert.deepEqual(await page.locator('#et100-package-info .ota-meta-row:not(.ota-meta-extra) .ota-meta-label').allTextContents(),['韌體大小','產品','Board','韌體','建置時間']);
    assert.equal(await page.locator('#et100-package-info .meta-compare-badge.diff').count(),0);
    for (const id of ['et100-file-info','et100-package-info']) {
      assert.equal(await page.locator(`#${id} .ota-meta-row:not(.ota-meta-extra) .meta-compare-badge.same`).count(),5,'all five summary fields match');
    }
    for (const key of ['image_length','product','board','firmware','build_id']) {
      await page.evaluate(key=>{
        window.savedPackage={...et100StoredPackage};
        et100StoredPackage={...et100StoredPackage,[key]:key==='image_length'?1:'MISMATCH'};
        et100RenderPackage();
      },key);
      for (const id of ['et100-file-info','et100-package-info'])
        assert.equal(await page.locator(`#${id} .ota-meta-row:not(.ota-meta-extra) .meta-compare-badge.diff`).count(),1,`${key} mismatch shown on both tables`);
      await page.evaluate(()=>{et100StoredPackage=window.savedPackage;et100RenderPackage();});
    }
    const sensorRequests=await page.evaluate(n=>mockSerial.requests.slice(n),sensorStart);
    assert.ok(sensorRequests.some(s=>s.startsWith('BLEGW$GSOTAPKG;')));
    assert.ok(!sensorRequests.some(s=>s==='2/0/5'||s==='2/1/0'||s.includes('SOTASTAGE')||s.includes('ZVERO')));
    assert.ok(await page.locator('[data-name="Sensor OTA"] .et100-slot-committed').evaluate(el=>parseFloat(el.style.width)>0));
    assert.doesNotMatch(await page.locator('#et100-viz').textContent(),/套件上限|225,280 B/);
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'sensor UI mobile width');
    await page.setViewportSize({width:1440,height:1000});
    await page.locator('#et100-package-query').click();
    await page.waitForFunction(()=>!et100Busy);
    assert.match(await page.locator('#et100-package-info').textContent(),/VALID/);
    const packageQueries=await page.evaluate(()=>mockSerial.opens.length);
    for(let i=0;i<3;i++) {
      await page.locator('#et100-package-query').click();
      await page.waitForFunction(()=>!et100Busy);
    }
    assert.equal(await page.evaluate(()=>mockSerial.opens.length),packageQueries,'repeated NOR queries do not reopen 9600');
    const sharedNorHeights=await page.evaluate(()=>{
      const mode=otaMode;
      const heights=()=>[...document.querySelectorAll('#et100-nor-map .et100-partition')].map(row=>row.getBoundingClientRect().height);
      const sensor=heights();
      otaMode='et100';et100RefreshVisual();const self=heights();
      otaMode=mode;et100RefreshVisual();const restored=heights();
      return {sensor,self,restored};
    });
    assert.ok(sharedNorHeights.self.every((height,i)=>Math.abs(height-sharedNorHeights.sensor[i])<1 &&
      Math.abs(height-sharedNorHeights.restored[i])<1),'Device RS232 and ET-100 self OTA use identical NOR partition heights');
    const snapshot=await page.evaluate(()=>{
      window.savedSensorVisual={file:et100File,visual:{...et100Visual},stored:{...et100StoredPackage}};
      et100File=null;et100Visual.off=null;et100Visual.stage='idle';et100RefreshVisual();
      return et100StoredPackage;
    });
    assert.equal(await page.locator('#et100-sensor-state').textContent(),`${snapshot.firmware}（${snapshot.package_length.toLocaleString()} B）`);
    assert.ok(await page.locator('[data-name="Sensor OTA"] .et100-slot-committed').evaluate(el=>parseFloat(el.style.width)>0),'read-only VALID snapshot fills the stored package, even without a selected file');
    assert.match(await page.locator('#et100-stat-rows').textContent(),/已提交進度未知/,'stored package is not an upload ACK');
    for (const state of ['EMPTY','INVALID']) {
      await page.evaluate(state=>et100ApplyPackage({...et100StoredPackage,state}),state);
      assert.equal(await page.locator('#et100-sensor-state').textContent(),'');
      assert.equal(await page.locator('#et100-sensor-state').isVisible(),false);
      assert.equal(await page.locator('[data-name="Sensor OTA"] .et100-slot-committed').evaluate(el=>parseFloat(el.style.width)),0);
    }
    await page.evaluate(()=>{
      et100File=savedSensorVisual.file;Object.assign(et100Visual,savedSensorVisual.visual,{stage:'upload',off:4096});
      et100ApplyPackage(savedSensorVisual.stored);
    });
    assert.match(await page.locator('#et100-sensor-state').textContent(),/^已提交 4,096 \/ /,'live upload takes precedence over the prior stored snapshot');
    await page.evaluate(()=>{Object.assign(et100Visual,savedSensorVisual.visual);et100RefreshVisual();});
    await page.locator('#et100-panel').screenshot({path:'/tmp/et100_sensor_sidebar.png'});
    await page.locator('.et100-viz-layout').screenshot({path:'/tmp/et100_sensor_diagram.png'});
    const relayStart=await page.evaluate(()=>{
      mockSerial.version='0.8.9-dev';
      processFrame('AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.9-dev;0;TEST');
      return mockSerial.requests.length;
    });
    assert.equal(await page.locator('[data-device-submode="gatt"]').isDisabled(),false,'0.8.9 enables relay');
    await page.locator('[data-device-submode="gatt"]').click();
    await page.waitForFunction(()=>!et100Busy && sensorRelay.available);
    assert.equal(await page.locator('#sensor-relay-panel').isVisible(),true);
    assert.equal(await page.locator('#et100-panel').isVisible(),false);
    assert.equal(await page.locator('#sensor-probe-panel').isVisible(),false);
    assert.equal(await page.locator('#flash-ota-viz').isVisible(),true);
    assert.equal(await page.locator('#flash-viz-source-title').textContent(),'外部 NOR · 16 MB（EVB）');
    assert.match(await page.locator('#flash-viz-source-slots').textContent(),/0x420000/);
    assert.equal(await page.locator('#flash-viz-source-slots .target .flash-viz-slot-size').textContent(),'256 KB');
    assert.ok(await page.locator('#flash-viz-source-slots .target .flash-viz-slot-image').evaluate(el=>parseFloat(el.style.width)>0));
    await page.locator('#sensor-relay-mac').fill('AABBCCDDEEFF');
    await page.locator('#sensor-relay-start').click();
    await page.waitForFunction(()=>sensorRelaySession.generation===1);
    assert.ok(await page.evaluate(()=>mockSerial.requests.some(s=>s.startsWith('BLEGW$SENSOROTA;AABBCCDDEEFF;'))));
    assert.equal(await page.locator('#sensor-relay-mac').isDisabled(),true);
    await page.locator('#sensor-relay-start').click();
    await page.waitForFunction(()=>sensorRelay.state==='CANCELLED');
    assert.equal(await page.locator('#sensor-relay-mac').isDisabled(),false);
    await page.locator('#sensor-relay-start').click();
    await page.waitForFunction(()=>sensorRelaySession.generation===1);
    await page.evaluate(()=>{mockSerial.relayState='SUCCESS';send('BLEGW$GSENSOROTA;R001');});
    await page.waitForFunction(()=>sensorRelay.state==='SUCCESS');
    assert.match(await page.locator('#log').textContent(),/Sensor OTA 成功/);
    await page.locator('#sensor-relay-query-select').selectOption('ALL');
    await page.locator('#sensor-relay-query').click();
    await page.waitForFunction(()=>mockSerial.requests.some(s=>s.startsWith('BLEGW$GSENSOROTAMETA;')));
    await page.waitForFunction(()=>sensorRelay.targetMeta?.valid);
    assert.equal(await page.evaluate(()=>sensorRelay.targetMeta.project),'EUP_AIoT_P1','firmware metadata wire format reaches relay UI');
    await page.locator('#sensor-relay-panel').screenshot({path:'/tmp/et100_relay_sidebar.png'});
    await page.locator('#flash-ota-viz').screenshot({path:'/tmp/et100_relay_diagram.png'});
    const relayRequests=await page.evaluate(n=>mockSerial.requests.slice(n),relayStart);
    for(const command of ['GSOTAPKG','GSENSOROTA','GSENSOROTAFLOW','GSENSOROTAMETA','CSENSOROTA'])
      assert.ok(relayRequests.some(s=>s.startsWith('BLEGW$'+command+';')),command+' connected');
    assert.ok(!relayRequests.some(s=>/SOTAPROBE|SOTASTAGE|ZVERO/.test(s)),'ET-100 never uses unsupported probe/ZVERO paths');
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'relay mobile width');
    await page.setViewportSize({width:1440,height:1000});
    await page.locator('[data-device-submode="rs232"]').click();
    assert.equal(await page.locator('#et100-panel').isVisible(),true);
    assert.match(await page.locator('#et100-file-info').textContent(),/EUP_AIoT_P1/,'sensor file preserved across relay view');
    page.once('dialog',dialog=>dialog.accept());
    await page.locator('#et100-package-delete').click();
    await page.waitForFunction(()=>document.getElementById('et100-result').textContent.includes('EMPTY'));
    assert.match(await page.locator('#et100-package-info').textContent(),/EMPTY/);
    assert.equal(await page.locator('#et100-file-info .meta-compare-badge').count(),0,'delete clears cached comparison marks');
    await page.locator('[data-device-submode="gatt"]').click();
    await page.waitForFunction(()=>!et100Busy && !sensorProbe.packageValid);
    assert.equal(await page.locator('#sensor-relay-panel').isVisible(),false,'EMPTY package cannot start a relay');
    assert.equal(await page.locator('#sensor-relay-no-pkg').isVisible(),true);
    assert.match(await page.locator('#sensor-relay-no-pkg-label').textContent(),/ET-100 NOR/);
    await page.locator('#ota-self-target').click();
    assert.equal(await page.locator('#et100-panel').isVisible(),true);
    assert.match(await page.locator('#et100-file-info').textContent(),/EUP_ET100/,'self file is preserved separately');
    await page.locator('[data-tab="ble"]').click();
    assert.equal(await page.locator('#et100-viz').isVisible(),false,'ET-100 diagram hidden outside OTA tab');
    assert.equal(await page.locator('[data-tab="lte"]').getAttribute('aria-label'),'AT 指令分析');
    assert.equal(await page.locator('#lte-page').getAttribute('aria-label'),'AT 分析終端');
    for (const width of [1400,1920]) {
      await page.setViewportSize({width,height:1000});
      for (const tab of ['ble','can','gatt','dash','lte']) {
        await page.locator(`[data-tab="${tab}"]`).click();
        const sidebar=await page.locator('.sidebar').boundingBox(),main=await page.locator('.main-panel').boundingBox();
        assert.equal(sidebar.width,350,`${tab} sidebar fixed at 350px (${width}px viewport)`);
        assert.equal(main.x,350,`${tab} right panel follows sidebar width`);
      }
      assert.equal(await page.locator('#lte-page').isVisible(),true);
      assert.equal(await page.locator('.main-panel > .log-wrap').isVisible(),false,'AT page still hides common log');
    }
    await page.screenshot({path:'/tmp/et100_at_analysis_desktop.png'});
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.locator('#lte-page').isVisible(),true);
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'AT mobile layout stays within page');
    const recovery=await page.evaluate(async()=>{
      mockSerial.failOpen=true;let error='';
      try {await et100Reopen(115200);}catch(e){error=e.message;}
      await et100Reopen(9600);
      const before=mockSerial.opens.length;
      await et100Reopen(9600);
      return {error,baud:mockSerial.baud,streamsReady:Boolean(reader&&writer),before,after:mockSerial.opens.length};
    });
    assert.match(recovery.error,/mock port open failure/);
    assert.equal(recovery.baud,9600);assert.equal(recovery.streamsReady,true);
    assert.equal(recovery.after,recovery.before,'failed reopen does not leave a stale cached baud');
    assert.deepEqual(errors,[],'uncaught browser errors');
    console.log('PASS browser mock Serial: self OTA, Sensor SMP storage, package comparisons/download, v0.8.9 relay gate/start/cancel/success/metadata/EMPTY, ET-100 NOR to Sensor diagram, no unsupported probe/ZVERO, AT widths and desktop/mobile layouts');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
