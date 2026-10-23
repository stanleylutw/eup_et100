// Mock JS state and browser clock only; no serial port or hardware access.
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
    await page.clock.install();
    await page.locator('[data-tab="ota"]').click();
    await page.evaluate(()=>{
      window.requests=[];send=async text=>requests.push(text);
      connected=true;gatewayVersionInfo={hwVer:'DEV1.4_fr3068ep-c',fwVer:'0.9.1-dev'};
      syncOtaSelfTarget(gatewayVersionInfo);otaMode='sensor';deviceSubMode='gatt';updateOtaModeUi();
      sensorProbe.packageValid=true;sensorProbe.imageLength=167055;sensorRelay.available=true;
      document.getElementById('sensor-relay-mac').value='EEACB6F5627E';renderSensorRelay();
      window.replyStart=(result)=>handleSensorRelayReply(`AT$PD=BLEGW$SENSOROTA;${result};${sensorRelayStart.token}`);
    });
    const start=page.locator('#sensor-relay-start');
    await start.click();
    await page.evaluate(()=>replyStart('0;4;BUSY'));
    assert.equal(await page.evaluate(()=>sensorRelay.stepStatuses.PACKAGE_CHECK),'PENDING');
    assert.match(await page.locator('#sensor-relay-timeline').textContent(),/裝置忙碌，稍後自動重試/);
    assert.match(await page.locator('#sensor-relay-timeline').innerText(),/裝置忙碌，稍後自動重試/);
    await page.screenshot({path:'/tmp/et100_relay_busy_ui.png'});
    const before=await page.evaluate(()=>requests.filter(s=>s.startsWith('BLEGW$SENSOROTA;')).length);
    await page.clock.runFor(2000);
    assert.equal(await page.evaluate(()=>requests.filter(s=>s.startsWith('BLEGW$SENSOROTA;')).length),before+1);
    await page.evaluate(()=>replyStart('1;EEACB6F5627E;9;167055'));
    assert.equal(await page.evaluate(()=>sensorRelaySession.accepted),true);
    assert.equal(await page.evaluate(()=>sensorRelayStart.timer),null);
    await page.evaluate(()=>{
      sensorRelay.state='CONNECTING';sensorRelay.workflowStep='CONNECTING';sensorRelay.stepResult='ACTIVE';
      parseGattType8('CF;AABBCCDDEEFF;disconnect_drain_timeout:i=1,c=0,s=0');
    });
    assert.equal(await page.evaluate(()=>sensorRelayStart.cleaning),false);
    await page.evaluate(()=>parseGattType8('CF;EEACB6F5627E;connect_timeout'));
    assert.match(await page.locator('#sensor-relay-timeline').textContent(),/連線逾時，BLE 清理中/);
    await page.evaluate(()=>{
      parseGattType8('CF;EEACB6F5627E;disconnect_drain_timeout:i=1,c=0,s=0,stop=1,disc=-1');
      handleSensorRelayReply('AT$PD=BLEGW$GSENSOROTA;1;FAILED;EEACB6F5627E;9;0;167055;25000;7;CONNECT_FAILED;TEST');
    });
    assert.equal(await start.isDisabled(),true);
    assert.equal(await page.evaluate(()=>sensorRelay.state),'FAILED');
    assert.match(await page.locator('#sensor-relay-timeline').textContent(),/BLE 清理中/);
    assert.match(await page.locator('#sensor-relay-timeline').innerText(),/BLE 清理中/);
    assert.match(await page.locator('#flash-viz-summary').innerText(),/BLE 清理中/);
    assert.equal(await page.evaluate(()=>Object.values(sensorRelay.stepStatuses).includes('ACTIVE')),false);
    await page.screenshot({path:'/tmp/et100_relay_cleanup_ui.png'});
    await page.evaluate(()=>parseGattType8('DC;EEACB6F5627E;'));
    assert.equal(await start.isDisabled(),false);
    // Persistent BUSY has a bounded retry window, no active/failed package step.
    await start.click();
    for(let i=0;i<15;i++) {
      await page.evaluate(()=>replyStart('0;4;BUSY'));
      await page.clock.runFor(2000);
    }
    assert.equal(await page.evaluate(()=>sensorRelay.state),'FAILED');
    assert.equal(await page.evaluate(()=>sensorRelayStart.timer),null);
    assert.equal(await page.evaluate(()=>sensorRelay.stepStatuses.PACKAGE_CHECK),'PENDING');
    assert.equal(await page.evaluate(()=>Object.values(sensorRelay.stepStatuses).includes('ACTIVE')),false);
    assert.match(await page.locator('#sensor-relay-timeline').textContent(),/裝置忙碌，請稍後再試/);
    assert.doesNotMatch(await page.locator('#sensor-relay-timeline').textContent(),/PACKAGE_INVALID/);
    // Canceling retries is local and prevents later sends.
    await start.click();await page.evaluate(()=>replyStart('0;4;BUSY'));
    await start.click();
    const count=await page.evaluate(()=>requests.length);await page.clock.runFor(4000);
    assert.equal(await page.evaluate(()=>requests.length),count);
    // A genuine non-package start rejection clears optimistic ACTIVE steps too.
    await start.click();await page.evaluate(()=>replyStart('0;5;BAD_MAC'));
    assert.equal(await page.evaluate(()=>Object.values(sensorRelay.stepStatuses).includes('ACTIVE')),false);
    assert.equal(await page.evaluate(()=>sensorRelay.stepStatuses.PACKAGE_CHECK),'PENDING');
    await page.screenshot({path:'/tmp/et100_relay_handoff_ui.png'});
    assert.deepEqual(errors,[]);
    console.log('PASS relay UI: BUSY retry/success/30s/cancel, package classification, matching-target cleanup, FAILED and DC recovery');
  } finally { await browser.close(); }
})().catch(e=>{console.error(e);process.exitCode=1;});
