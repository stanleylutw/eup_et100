// Mock visual states and validated INST callbacks only; never opens serial hardware.
const assert=require('node:assert/strict'),path=require('node:path');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || '/Users/stanley/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async()=>{
  const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL || 'chrome'});
  try {
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:{getPorts:async()=>[]}}));
    await page.goto(pathToFileURL(path.resolve(__dirname,'../rs232_tester.html')).href);
    await page.locator('[data-tab="ota"]').click();
    await page.evaluate(()=>{
      gatewayVersionInfo={hwVer:'DEV1.4_fr3068ep-c',fwVer:'0.9.0-dev'};
      syncOtaSelfTarget(gatewayVersionInfo);otaMode='et100';activeTab='ota';updateOtaModeUi();
      et100File={bytes:new Uint8Array(300000),displayName:'dfu_application.zip',version:'0.9.0.0'};
      et100Visual.off=60000;et100Flow('upload');et100FitDiagram();
    });
    const overlay=page.locator('#et100-transfer-overlay');
    assert.equal(await page.locator('#et100-route-upload').isVisible(),false);
    assert.equal(await page.locator('#et100-route-install').isVisible(),false);
    await page.waitForFunction(()=>document.getElementById('et100-transfer-overlay').dataset.route==='upload');
    assert.equal(await overlay.locator('[data-icon="fileText"]').count(),4);
    assert.equal(await overlay.locator('[data-icon="fileText"]').first().evaluate(el=>getComputedStyle(el).color),'rgb(52, 168, 83)');
    const greenBorder=async selector=>assert.deepEqual(await page.locator(selector).evaluate(el=>{
      const s=getComputedStyle(el);return {color:s.outlineColor,width:s.outlineWidth};
    }),{color:'rgb(52, 168, 83)',width:'3px'});
    await greenBorder('#et100-nor-map [data-name="Candidate"]');
    assert.equal(await overlay.locator('.flash-viz-ble-dot').first().evaluate(el=>getComputedStyle(el).animationDuration),'4s');
    const continuous=await page.evaluate(async()=>{
      const dot=document.querySelector('#et100-transfer-overlay .flash-viz-ble-dot');
      const animation=dot.getAnimations()[0];
      const before=animation.currentTime;
      for(let i=0;i<12;i++) {
        et100Visual.off+=600;et100RefreshVisual();
        await new Promise(resolve=>setTimeout(resolve,60));
      }
      return dot.getAnimations()[0]===animation && animation.currentTime-before>500 && getComputedStyle(dot).opacity==='1';
    });
    assert.ok(continuous,'ACK refresh must not restart or fade the active animation');
    const baseline=await page.evaluate(()=>({log:document.querySelector('.log-wrap').getBoundingClientRect().height,
      flash:document.getElementById('et100-flash-map').getBoundingClientRect().height}));
    const inst=async(event,fields)=>page.evaluate(({event,fields})=>et100InstVisual({version:1,src:'L',event,fields,txn:'12345678',known:true}),{event,fields});
    const route=async kind=>{
      await page.waitForFunction(k=>document.getElementById('et100-transfer-overlay').dataset.route===k,kind);
      return page.evaluate(()=>{
        const diagram=document.querySelector('.et100-diagram'),base=diagram.getBoundingClientRect();
        const path=document.querySelector('#et100-transfer-overlay path');
        const p=path.getPointAtLength(0),q=path.getPointAtLength(path.getTotalLength());
        const r=et100TransferRoute(),a=document.querySelector(r.from).getBoundingClientRect(),b=document.querySelector(r.to).getBoundingClientRect();
        const sx=diagram.offsetWidth/base.width,sy=diagram.offsetHeight/base.height;
        return {start:{x:p.x,y:p.y},end:{x:q.x,y:q.y},
          a:{left:(a.left-base.left)*sx,right:(a.right-base.left)*sx,center:(a.top+a.height/2-base.top)*sy},
          b:{left:(b.left-base.left)*sx,right:(b.right-base.left)*sx,center:(b.top+b.height/2-base.top)*sy}};
      });
    };
    let points=await route('upload');assert.ok(points.start.x<points.end.x);
    assert.ok(Math.abs(points.end.y-points.b.center)<1);
    // CSS offset-distance must actually move the binary icons.
    const first=await overlay.locator('.flash-viz-ble-dot').first().boundingBox();
    await page.waitForTimeout(180);
    const second=await overlay.locator('.flash-viz-ble-dot').first().boundingBox();
    assert.ok(Math.abs(first.x-second.x)+Math.abs(first.y-second.y)>1);
    await page.screenshot({path:'/tmp/et100_transfer_upload.png'});
    await page.evaluate(()=>et100Flow('verify'));await page.waitForFunction(()=>document.getElementById('et100-transfer-overlay').hidden);
    await page.evaluate(()=>et100Flow('backup'));
    await inst('START',['INSTALL','300000']);await inst('BACKUP',['100000','280000']);
    const tagCheck=async(selector,text,kind)=>{
      const tag=page.locator(selector+' .et100-stage-tag');
      assert.equal(await tag.isVisible(),true);assert.equal(await tag.textContent(),text);
      assert.equal(await tag.getAttribute('data-kind'),kind);
      assert.equal(await tag.evaluate(el=>getComputedStyle(el,'::before').animationDuration),'2s');
      assert.ok(await tag.evaluate(el=>{
        const a=el.getBoundingClientRect(),row=el.closest('.et100-partition'),b=row.getBoundingClientRect();
        const name=row.querySelector('strong').getBoundingClientRect(),size=row.querySelector('.et100-partition-capacity').getBoundingClientRect();
        const separate=r=>a.right<=r.left || a.left>=r.right || a.bottom<=r.top || a.top>=r.bottom;
        return a.top>=b.top && a.right<=b.right && separate(name) && separate(size) &&
          a.left>=name.right && a.top<name.top && Math.abs(a.left-name.right)<8;
      }),'tag follows the text upper-right corner without covering the name or capacity');
    };
    await tagCheck('#et100-nor-map [data-name="Rollback"]','備份','backup');
    points=await route('backup');assert.ok(points.start.x>points.end.x);
    assert.ok(Math.abs(points.start.y-points.a.center)<1 && Math.abs(points.end.y-points.b.center)<1);
    assert.match(await page.locator('#et100-route-install small').textContent(),/備份 35%/);
    const backupProgress=page.locator('#et100-nor-map [data-name="Rollback"] .et100-operation-progress');
    assert.equal(await backupProgress.isVisible(),false,'backup byte counters stay out of the partition UI');
    assert.match(await backupProgress.textContent(),/備份 100,000 \/ 280,000 B · 35%/);
    assert.equal(await backupProgress.getAttribute('aria-valuenow'),'100000');
    await greenBorder('#et100-nor-map [data-name="Rollback"]');
    assert.ok(await page.locator('#et100-nor-map [data-name="Rollback"] .et100-slot-committed').evaluate(el=>parseFloat(el.style.width)>0));
    assert.equal(await page.locator('#et100-nor-map [data-name="Rollback"] .et100-slot-committed').evaluate(el=>getComputedStyle(el).backgroundColor),'rgb(23, 111, 84)');
    assert.equal(await overlay.locator('[data-icon="fileText"]').first().evaluate(el=>getComputedStyle(el).color),'rgb(23, 111, 84)');
    assert.equal(await page.locator('#et100-nor-map [data-name="Rollback"] .et100-stage-tag').evaluate(el=>getComputedStyle(el,'::before').backgroundColor),'rgb(23, 111, 84)');
    await page.screenshot({path:'/tmp/et100_transfer_backup.png'});
    await inst('BACKUP',['SKIP']);await page.waitForFunction(()=>document.getElementById('et100-transfer-overlay').hidden);
    await page.evaluate(()=>et100Flow('waiting'));
    await inst('ERASE',['1000','300000']);assert.equal(await overlay.isVisible(),false);
    await inst('WRITE',['150000','300000']);points=await route('install');assert.ok(points.start.x<points.end.x);
    await tagCheck('#et100-flash-map [data-name="App"]','安裝中','install');
    assert.match(await page.locator('#et100-route-install small').textContent(),/寫入 50%/);
    const appProgress=page.locator('#et100-flash-map [data-name="App"] .et100-operation-progress');
    assert.equal(await appProgress.isVisible(),false,'write byte counters stay out of the partition UI');
    assert.match(await appProgress.textContent(),/寫入 150,000 \/ 300,000 B · 50%/);
    assert.equal(await appProgress.getAttribute('aria-valuenow'),'150000');
    await greenBorder('#et100-flash-map [data-name="App"]');
    assert.ok(await page.locator('#et100-flash-map [data-name="App"] .et100-slot-committed').evaluate(el=>parseFloat(el.style.width)>0));
    assert.equal(await page.locator('#et100-flash-map [data-name="App"] .et100-slot-committed').evaluate(el=>getComputedStyle(el).backgroundColor),'rgb(52, 168, 83)');
    await page.screenshot({path:'/tmp/et100_transfer_install.png'});
    await inst('WRITE',['300000','300000']);await page.waitForFunction(()=>document.getElementById('et100-transfer-overlay').hidden);
    assert.match(await appProgress.textContent(),/300,000 \/ 300,000 B · 100%/);
    await inst('VERIFY',['OK']);assert.equal(await overlay.isVisible(),false);
    await page.evaluate(()=>et100Flow('checking'));
    await tagCheck('#et100-flash-map [data-name="App"]','檢查中','check');
    await page.evaluate(()=>et100Flow('waiting'));
    await inst('HELLO',['0.8.11','TRIAL']);await inst('WRITE',['0','300000']);
    assert.equal(await overlay.isVisible(),false,'missing START must not guess Candidate vs Rollback');
    assert.equal(await appProgress.isVisible(),false,'missing START must not invent destination progress');
    await inst('START',['ROLLBACK','280000']);await inst('WRITE',['140000','280000']);
    points=await route('restore');assert.ok(points.start.x<points.end.x);
    assert.match(await page.locator('#et100-route-install small').textContent(),/還原 50%/);
    assert.match(await appProgress.textContent(),/還原 140,000 \/ 280,000 B · 50%/);
    await page.evaluate(()=>et100Flow('error'));await page.waitForFunction(()=>document.getElementById('et100-transfer-overlay').hidden);
    await page.evaluate(()=>et100Flow('confirmed'));
    const success=page.locator('#et100-flash-map [data-name="App"] .et100-stage-tag');
    assert.equal(await success.textContent(),'更新成功');assert.equal(await success.isVisible(),true);
    assert.equal(await success.evaluate(el=>getComputedStyle(el,'::before').animationName),'none');
    assert.equal(await page.locator('#et100-flash-map [data-name="App"]').getAttribute('data-transfer-complete'),'true');
    assert.deepEqual(await page.evaluate(()=>({log:document.querySelector('.log-wrap').getBoundingClientRect().height,
      flash:document.getElementById('et100-flash-map').getBoundingClientRect().height})),baseline);
    await page.evaluate(()=>{
      otaMode='sensor';deviceSubMode='rs232';updateOtaModeUi();
      et100File={bytes:new Uint8Array(167311),displayName:'sensor.zip'};
      et100Visual.off=60000;et100Flow('upload');
    });
    await route('upload');
    assert.match(await page.evaluate(()=>et100TransferRoute().to),/Sensor OTA/);
    await page.evaluate(()=>et100Flow('stored'));await page.waitForFunction(()=>document.getElementById('et100-transfer-overlay').hidden);
    // GATT uses Bluetooth glyphs; wired and internal transfers retain file icons.
    await page.evaluate(()=>{deviceSubMode='gatt';updateOtaModeUi();renderFlashOtaVisualization();});
    assert.equal(await page.locator('#flash-viz-ble-stream [data-icon="bluetooth"]').count(),4);
    assert.equal(await page.locator('#flash-viz-ble-stream [data-icon="fileText"]').count(),0);
    for(const width of [1920,1400,390]) {
      await page.setViewportSize({width,height:1000});
      await page.evaluate(()=>{deviceSubMode='rs232';updateOtaModeUi();et100Visual.off=60000;et100Flow('upload');et100FitDiagram();});
      await route('upload');
      assert.ok(await page.evaluate(()=>{
        const d=document.querySelector('.et100-diagram').getBoundingClientRect();
        const p=document.querySelector('#et100-transfer-overlay path').getBBox();
        return d.width>0 && p.width>=0 && Number.isFinite(p.x) && Number.isFinite(p.y);
      }));
    }
    await page.locator('#et100-source').scrollIntoViewIfNeeded();
    await page.screenshot({path:'/tmp/et100_transfer_mobile.png'});
    assert.deepEqual(errors,[]);
    console.log('PASS binary OTA flow: ACK upload Candidate/Sensor, INST backup/install/restore, SKIP/missing START/terminal, scaled endpoints, motion, stable layout and mobile');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
