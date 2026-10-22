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
      window.mockSerial={opens:[],requests:[],uploaded:0,oldVersion:'0.1.0.0',polls:0,clock:0};
      const mock=window.mockSerial;
      const port={
        async open({baudRate}) {
          mock.opens.push(baudRate);mock.baud=baudRate;
          this.readable=new ReadableStream({start:c=>{mock.controller=c;}});
          this.writable=new WritableStream({write:bytes=>mock.write(bytes)});
        },
        async close() {if(this.readable.locked||this.writable.locked)throw Error('port close with locked streams');this.readable=null;this.writable=null;}
      };
      Object.defineProperty(navigator,'serial',{value:{getPorts:async()=>[],requestPort:async()=>port}});
      mock.write=bytes=>{
        const text=new TextDecoder().decode(bytes);
        if(text.startsWith('BLEGW$')) {
          mock.requests.push(text.trim());const fields=text.trim().split(';');
          if(fields[0]==='BLEGW$GCHK')mock.controller.enqueue(new TextEncoder().encode(`AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.6-dev;0;${fields[1]}\r\n`));
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
    assert.equal(await page.locator('.parsed-panel > #et100-viz').isVisible(),true);
    assert.equal(await page.locator('#et100-panel .et100-memory').count(),0,'diagram belongs in main area, not sidebar');
    assert.equal(await page.locator('#flash-ota-viz').isVisible(),false);
    assert.match(await page.locator('#et100-flash-map').textContent(),/MCU 內建 Flash · 2 MB/);
    assert.match(await page.locator('#et100-flash-map').textContent(),/禁止擦除／寫入/);
    assert.equal(await page.locator('#et100-flash-map [data-name="ROM B 資訊段"] small:last-child').isVisible(),true);
    assert.match(await page.locator('#et100-flash-map').textContent(),/1792 KB/);
    assert.match(await page.locator('#et100-nor-map').textContent(),/16 MB（EVB）/);
    assert.match(await page.locator('#et100-nor-map').textContent(),/11.625 MB/);
    assert.equal(await page.locator('#et100-viz select, #et100-viz details, #et100-viz ol, #et100-viz p').count(),0,'no extra selector, explanations or workflow list');
    const layouts=await page.evaluate(()=>[et100FlashPartitions,et100StoragePartitions('evb'),et100StoragePartitions('product')]);
    for(const [i,parts] of layouts.entries()) {
      assert.equal(parts.reduce((sum,p)=>sum+p[2],0),[2,16,32][i]*1024*1024);
      for(let j=1;j<parts.length;j++)assert.equal(parts[j][1],parts[j-1][1]+parts[j-1][2],'contiguous, no overlap');
    }
    await page.locator('#et100-query').click();
    await page.waitForFunction(()=>document.querySelector('#et100-device').textContent.includes('0.1.0.0'));
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
    for (const id of ['et100-flash-map','et100-nor-map']) {
      assert.equal(await page.locator('#'+id).evaluate(el=>el.lastElementChild.lastElementChild.getBoundingClientRect().bottom>el.getBoundingClientRect().bottom+1),false,`${id} final partition must not be clipped`);
    }
    await page.locator('#et100-flash-map').scrollIntoViewIfNeeded();
    await page.screenshot({path:'/tmp/et100_memory_desktop.png'});
    await page.setViewportSize({width:1920,height:1200});
    assert.equal(Math.round((await page.locator('.et100-viz-layout').boundingBox()).height),348,'desktop diagram has fixed compact height');
    assert.ok((await page.locator('.et100-diagram').boundingBox()).height<=349,'diagram must not grow past fixed frame');
    assert.equal(await page.locator('#et100-viz').evaluate(el=>el.scrollHeight>el.clientHeight+1),false,'desktop diagram fits visualization panel');
    for (const id of ['et100-flash-map','et100-nor-map','et100-stats']) {
      assert.equal(await page.locator('#'+id).evaluate(el=>el.scrollHeight>el.clientHeight+1),false,`${id} contents fit compact frame`);
    }
    const positions=await page.evaluate(()=>['et100-source','et100-nor-map','et100-flash-map','et100-stats'].map(id=>document.getElementById(id).getBoundingClientRect().left));
    assert.ok(positions.every((x,i)=>i===0||x>positions[i-1]),'desktop: file → NOR → MCU → status');
    await page.locator('.et100-viz-layout').screenshot({path:'/tmp/et100_memory_diagram.png'});
    await page.setViewportSize({width:1440,height:1000});
    await page.evaluate(()=>{
      mockSerial.file=et100File;et100Client.io.now=()=>mockSerial.clock;
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
    assert.match(log,/slot 0 .*confirmed=false/);
    assert.match(log,/slot 0 .*confirmed=true/);
    assert.match(log,/更新成功：.*已確認/);
    const installPos=log.lastIndexOf('ET-100：安裝中');
    const checkPos=log.lastIndexOf('ET-100：新版程式檢查中');
    assert.ok(installPos>=0&&checkPos>installPos&&log.lastIndexOf('ET-100：更新成功：')>checkPos,'install → checking → success follows actual state');
    assert.ok((log.match(/SMP upload ACK/g)||[]).length<=13,'upload log is throttled');
    assert.match(await page.locator('#et100-file-info').textContent(),/裝置／檔案硬體一致/);
    let state=await page.evaluate(()=>({opens:mockSerial.opens,requests:mockSerial.requests,polls:mockSerial.polls}));
    assert.deepEqual(state.opens,[9600,9600,9600,115200,9600]);
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
    assert.equal(await page.locator('#ota-device-submode').isVisible(),false,'no unimplemented relay controls');
    assert.equal(await page.locator('#et100-start').textContent(),'儲存套件');
    assert.equal(await page.locator('#et100-start').isDisabled(),true,'self image must not become sensor package');
    const beforeWrongZip=await page.evaluate(()=>mockSerial.requests.length);
    await page.locator('#et100-file').setInputFiles(path.join(root,'fw/build_serial/output/dfu_application.zip'));
    await page.waitForFunction(()=>document.getElementById('et100-file-info').textContent.includes('檔案無效'));
    assert.equal(await page.locator('#et100-start').isDisabled(),true);
    assert.equal(await page.evaluate(()=>mockSerial.requests.length),beforeWrongZip,'ET-100 zip rejected as sensor without device access');
    await page.locator('#et100-file').setInputFiles({name:'p1_dfu.zip',mimeType:'application/zip',buffer:Buffer.from(sensorFixture.zip)});
    await page.waitForFunction(()=>!document.getElementById('et100-start').disabled);
    assert.match(await page.locator('#et100-file-info').textContent(),/EUP_AIoT_P1/);
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
    assert.equal(await page.locator('#et100-package-info .meta-compare-badge.diff').count(),0);
    const sensorRequests=await page.evaluate(n=>mockSerial.requests.slice(n),sensorStart);
    assert.ok(sensorRequests.some(s=>s.startsWith('BLEGW$GSOTAPKG;')));
    assert.ok(!sensorRequests.some(s=>s==='2/0/5'||s==='2/1/0'||s.includes('SOTASTAGE')||s.includes('ZVERO')));
    assert.ok(await page.locator('[data-name="Sensor OTA"] .et100-slot-committed').evaluate(el=>parseFloat(el.style.width)>0));
    assert.match(await page.locator('#et100-stat-rows').textContent(),/225,280 B/);
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'sensor UI mobile width');
    await page.setViewportSize({width:1440,height:1000});
    await page.locator('#et100-package-query').click();
    await page.waitForFunction(()=>!et100Busy);
    assert.match(await page.locator('#et100-package-info').textContent(),/VALID/);
    await page.locator('#et100-panel').screenshot({path:'/tmp/et100_sensor_sidebar.png'});
    await page.locator('.et100-viz-layout').screenshot({path:'/tmp/et100_sensor_diagram.png'});
    page.once('dialog',dialog=>dialog.accept());
    await page.locator('#et100-package-delete').click();
    await page.waitForFunction(()=>document.getElementById('et100-result').textContent.includes('EMPTY'));
    assert.match(await page.locator('#et100-package-info').textContent(),/EMPTY/);
    await page.locator('#ota-self-target').click();
    assert.equal(await page.locator('#et100-panel').isVisible(),true);
    assert.match(await page.locator('#et100-file-info').textContent(),/EUP_ET100/,'self file is preserved separately');
    await page.locator('[data-tab="ble"]').click();
    assert.equal(await page.locator('#et100-viz').isVisible(),false,'ET-100 diagram hidden outside OTA tab');
    assert.equal(await page.locator('[data-tab="lte"]').textContent(),'AT 分析');
    assert.equal(await page.locator('#lte-page').getAttribute('aria-label'),'AT 分析終端');
    for (const width of [1400,1920]) {
      await page.setViewportSize({width,height:1000});
      for (const tab of ['ble','can','gatt','dash','lte']) {
        await page.locator(`[data-tab="${tab}"]`).click();
        const sidebar=await page.locator('.sidebar').boundingBox(),main=await page.locator('.main-panel').boundingBox();
        assert.equal(sidebar.width,320,`${tab} sidebar fixed at 320px (${width}px viewport)`);
        assert.equal(main.x,320,`${tab} right panel does not shift`);
      }
      assert.equal(await page.locator('#lte-page').isVisible(),true);
      assert.equal(await page.locator('.main-panel > .log-wrap').isVisible(),false,'AT page still hides common log');
    }
    await page.screenshot({path:'/tmp/et100_at_analysis_desktop.png'});
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.locator('#lte-page').isVisible(),true);
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'AT mobile layout stays within page');
    assert.deepEqual(errors,[],'uncaught browser errors');
    console.log('PASS browser mock Serial: self OTA + Sensor SMP image 1 store/query/delete, wrong ZIP blocked, separate file selections, Sensor NOR fill, no Sensor Test/reset/relay, Gateway metadata style + mode detection, AT widths, desktop/mobile layouts');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
