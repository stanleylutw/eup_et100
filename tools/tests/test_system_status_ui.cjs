// Only mocked writers and injected frames; no serial hardware is opened.
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
    await page.evaluate(()=>{
      window.sysRequests=[];window.sysAutoReply=true;
      writer={write:async bytes=>{
        const text=new TextDecoder().decode(bytes).trim();sysRequests.push(text);
        if(text.startsWith('BLEGW$GSYSSTAT;') && sysAutoReply) {
          const token=text.split(';')[1];
          const pairs=ET100System.registry.map(r=>`${String(r.index).padStart(2,'0')},${r.index===3?2:242}`).join(';');
          setTimeout(()=>processFrame(ET100System.encode(`AT$PD=BLEGW$GSYSSTAT;1;${token};10;${pairs}`).trim()),10);
        }
      }};
      setConnected(true);
      processFrame('AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.9.0-dev;0;ABCD');
    });
    await page.locator('[data-tab="dash"]').click();
    await page.waitForFunction(()=>systemStatus.complete);
    assert.equal(await page.locator('.system-status-row').count(),33);
    assert.equal(await page.locator('.system-status-row[data-index="5"] .system-status-name').textContent(),'行動通訊 EG800Q-EU');
    assert.equal(await page.locator('.system-status-row[data-index="8"] .system-status-name').textContent(),'衛星定位 AG3352Q');
    assert.equal(await page.locator('.system-status-row[data-index="29"] .system-status-name').textContent(),'看門狗 WATCHDOG');
    assert.match(await page.locator('.system-status-row[data-index="3"]').textContent(),/2 · READY/);
    assert.match(await page.locator('.system-status-row[data-index="5"]').textContent(),/242 · UNSUPPORTED/);
    await page.evaluate(()=>{
      const bytes=new TextEncoder().encode(ET100System.encode('ET100$SYS;11;04,3'));
      et100Client.decoder.feed(bytes.slice(0,9));et100Client.decoder.feed(bytes.slice(9));
    });
    assert.match(await page.locator('.system-status-row[data-index="4"]').textContent(),/SCANNING/);
    await page.evaluate(()=>processFrame(ET100System.encode('ET100$SYS;11;04,99').trim()));
    assert.match(await page.locator('.system-status-row[data-index="4"]').textContent(),/UNKNOWN_CODE/);
    await page.evaluate(()=>processFrame('ET100$SYS;11;03,5*0000'));
    assert.match(await page.locator('#system-status-summary').textContent(),/異常/);
    assert.equal(await page.locator('.system-status-row[data-index="3"] .system-status-value').getAttribute('data-kind'),'unknown');
    await page.locator('#system-status-refresh').click();await page.waitForFunction(()=>systemStatusMessage==='已讀取 · 33 項');
    const before=await page.evaluate(()=>({values:[...systemStatus.values],count:sysRequests.length}));
    await page.evaluate(()=>processFrame(ET100System.encode('AT$PD=BLEGW$GSYSSTAT;0;ZZZZ;2').trim()));
    assert.deepEqual(await page.evaluate(()=>systemStatus.values),before.values);
    await page.screenshot({path:'/tmp/et100_system_status_desktop.png'});
    for(const width of [1920,1400,390]) {
      await page.setViewportSize({width,height:900});
      const fits=await page.evaluate(()=>{
        const section=document.getElementById('system-status-section');
        return section.scrollWidth<=section.clientWidth &&
          [...section.querySelectorAll('.system-status-row')].every(r=>r.scrollWidth<=r.clientWidth);
      });
      assert.ok(fits,'SYS table overflow at '+width);
    }
    await page.screenshot({path:'/tmp/et100_system_status_mobile.png'});
    await page.evaluate(()=>{sysAutoReply=false;processFrame(ET100System.encode('ET100$SYS;2').trim());});
    assert.equal(await page.evaluate(()=>systemStatus.complete),false);
    assert.ok(await page.evaluate(()=>systemStatus.values.every(v=>v===null)));
    await page.evaluate(()=>querySystemStatus());
    await page.waitForFunction(()=>systemStatusMessage==='查詢逾時');
    await page.evaluate(()=>setConnected(false));
    assert.equal(await page.locator('#system-status-refresh').isDisabled(),true);
    assert.ok(await page.evaluate(()=>systemStatus.values.every(v=>v===null)));
    await page.evaluate(()=>{
      setConnected(true);gatewayVersionInfo={hwVer:'DEV1.4_fr3068ep-c',fwVer:'0.8.12-dev'};renderSystemStatus();
    });
    assert.equal(await page.locator('#system-status-refresh').isDisabled(),true);
    const noProbe=await page.evaluate(async()=>{const n=sysRequests.length;await querySystemStatus();return n===sysRequests.length;});
    assert.ok(noProbe);
    await page.evaluate(()=>{gatewayVersionInfo={hwVer:'V1_nrf52840',fwVer:'v0.9.0'};renderSystemStatus();});
    assert.equal(await page.locator('#system-status-section').isVisible(),false);
    assert.deepEqual(errors,[]);
    console.log('PASS SYS UI: Dash 33 items, query token/CRC/delta/unknown, boot/offline/timeout, firmware gate and responsive layouts');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
