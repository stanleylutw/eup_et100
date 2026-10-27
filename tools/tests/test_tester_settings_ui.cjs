// Mock Web Serial only; settings never issue device commands.
const assert=require('node:assert/strict'),path=require('node:path'),fs=require('node:fs');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || '/Users/stanley/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async()=>{
  const browser=await chromium.launch({headless:true,channel:'chrome'});
  try {
    const page=await browser.newPage({viewport:{width:1440,height:1000},acceptDownloads:true}),errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:{getPorts:async()=>[]}}));
    await page.goto(pathToFileURL(path.resolve(__dirname,'../rs232_tester.html')).href);
    const open=()=>page.locator('#settings-open').click();
    const close=()=>page.locator('[data-tab="ble"]').click();
    await open();
    assert.equal(await page.locator('.sidebar-content #settings-sidebar').isVisible(),true);
    assert.equal(await page.locator('dialog').count(),0);
    assert.equal(await page.locator('#settings-open').getAttribute('aria-current'),'page');
    assert.equal(await page.locator('#settings-log-limit').inputValue(),'4000');
    assert.equal(await page.locator('#settings-clear-on-connect').isChecked(),false);
    assert.equal(await page.locator('#settings-about').isVisible(),true);
    assert.equal(await page.locator('#settings-tool-version').textContent(),await page.locator('#tester-version').textContent());
    assert.equal(await page.locator('#settings-records').isVisible(),true);
    for(const tab of ['dash','ble','can','ota','gatt','lte']) {
      await page.locator(`[data-tab="${tab}"]`).click();
      const right=await page.locator('.main-panel').getAttribute('class');
      const tools=await page.locator('#header-section-3').innerText();
      await open();
      assert.equal(await page.locator(`#${tab}-sidebar`).isVisible(),false);
      assert.equal(await page.locator('.main-panel').getAttribute('class'),right,'settings must not change the right-hand view');
      assert.equal(await page.locator('#header-section-3').innerText(),tools);
      await page.locator(`[data-tab="${tab}"]`).click();
      assert.equal(await page.locator('#settings-sidebar').isVisible(),false);
      assert.equal(await page.locator(`#${tab}-sidebar`).isVisible(),true,'same previous tab must be restorable');
    }
    await open();
    await page.evaluate(()=>{
      clearSerialLog();lteClearLog();
      for(let i=0;i<1100;i++) {addLog('sys',`UART-${i}`);lteAddLogEntry('SYS',`AT-${i}`,'tag-sys','sys');}
    });
    await page.locator('#settings-log-limit').selectOption('1000');
    assert.deepEqual(await page.evaluate(()=>({uart:logEl.children.length,at:lteLogEntries.length,dom:document.getElementById('lte-log-container').children.length})),{uart:1000,at:1000,dom:1000});
    assert.match(await page.locator('#log .log-entry').first().innerText(),/UART-100$/);
    await page.evaluate(()=>{
      for(let i=1100;i<1150;i++) {addLog('sys',`UART-${i}`);lteAddLogEntry('SYS',`AT-${i}`,'tag-sys','sys');}
    });
    assert.equal(await page.locator('#log .log-entry').count(),1000);
    assert.equal(await page.evaluate(()=>lteLogEntries.length),1000);
    await page.evaluate(async()=>{
      writer={write:async()=>{}};
      for(let i=0;i<20;i++) await send(`BLEGW$GCHK;${i}`);
      writer=null;
    });
    assert.equal(await page.locator('#log .log-entry').count(),1000,'TX entries must obey the same retention limit');
    await close();
    const downloadPromise=page.waitForEvent('download');
    await page.evaluate(()=>lteExportLog());
    const download=await downloadPromise,exported=fs.readFileSync(await download.path(),'utf8');
    assert.equal(exported.split('\n').length,1000);
    assert.ok(!exported.includes('AT-100\n') && exported.includes('AT-1149'));
    await page.evaluate(()=>{
      lteClearLog();lteAddLogEntry('RX','old modem','tag-t0','modem');
      lteActiveFilter='modem';lteFilterRender();
      for(let i=0;i<1001;i++) lteAddLogEntry('TX',`mcu-${i}`,'tag-tx','mcu');
    });
    assert.equal(await page.locator('#lte-log-container .log-entry').count(),0,'filtered-out new entries must still evict old rendered entries');
    await page.evaluate(()=>{lteActiveFilter='all';lteFilterRender();});
    assert.equal(await page.locator('#lte-log-container .log-entry').count(),1000);
    await open();await page.locator('#settings-clear-on-connect').check();await close();
    await page.evaluate(()=>{
      clearLog=()=>{throw new Error('automatic clear must not reset telemetry');};
      setConnected(false);addLog('sys','old session');setConnected(true);
    });
    assert.equal(await page.locator('#log .log-entry').count(),0);
    await page.evaluate(()=>{addLog('sys','same session');setConnected(true);});
    assert.equal(await page.locator('#log .log-entry').count(),1,'same-session refresh must not clear log');
    await page.evaluate(()=>{
      window.makeSettingsPort=()=>({async open(){},async close(){},readable:new ReadableStream(),writable:new WritableStream()});
      lteClearLog();lteAddLogEntry('SYS','previous AT session','tag-sys');
      lteSelectedPort2=makeSettingsPort();lteSelectedPort1=makeSettingsPort();
    });
    assert.equal(await page.evaluate(()=>lteConnectPort(2)),true);
    assert.ok(!await page.locator('#lte-log-container').innerText().then(t=>t.includes('previous AT')));
    assert.equal(await page.evaluate(()=>lteConnectPort(1)),true);
    assert.equal(await page.evaluate(()=>lteLogEntries.length),2,'second channel must retain the first channel connection log');
    await page.evaluate(async()=>{
      await lteDisconnectPort(1);await lteDisconnectPort(2);
      lteAddLogEntry('SYS','keep on failed open','tag-sys');
      lteSelectedPort1={async open(){throw new Error('mock denied');}};
      if(await lteConnectPort(1)) throw new Error('expected open failure');
    });
    assert.ok((await page.locator('#lte-log-container').innerText()).includes('keep on failed open'));
    await page.reload();await open();
    assert.equal(await page.locator('#settings-log-limit').inputValue(),'1000');
    assert.equal(await page.locator('#settings-clear-on-connect').isChecked(),true);
    for(const theme of ['light','dark']) {
      await page.evaluate(t=>document.documentElement.dataset.theme=t,theme);
      for(const width of [1440,390]) {
        await close();await page.setViewportSize({width,height:1000});
        const button=await page.locator('#settings-open').boundingBox();
        const rail=await page.locator('.tab-bar').boundingBox();
        assert.ok(button.y>=rail.y && button.y+button.height<=rail.y+rail.height+1,'settings must stay anchored within the rail');
        await open();
        assert.ok(await page.locator('#settings-sidebar').evaluate(panel=>{
          const parent=panel.closest('.sidebar-content'),r=parent.getBoundingClientRect();
          return r.left>=0 && r.right<=innerWidth && parent.scrollWidth<=parent.clientWidth &&
            [...panel.querySelectorAll('select,input,table')].every(e=>{const b=e.getBoundingClientRect();return b.left>=r.left && b.right<=r.right;});
        }));
        await page.screenshot({path:`/tmp/et100_settings_${theme}_${width}.png`});
      }
    }
    await page.keyboard.press('Escape');
    assert.equal(await page.locator('#settings-sidebar').isVisible(),true,'settings is a normal sidebar, not a modal');
    await close();
    assert.equal(await page.locator('#settings-sidebar').isVisible(),false);
    await page.evaluate(()=>localStorage.setItem(TOOL_SETTINGS_KEY,'{bad-json'));
    await page.reload();await open();
    assert.equal(await page.locator('#settings-log-limit').inputValue(),'4000');
    assert.equal(await page.locator('#settings-clear-on-connect').isChecked(),false);
    await page.evaluate(()=>{Storage.prototype.setItem=()=>{throw new Error('storage disabled');};});
    await page.locator('#settings-log-limit').selectOption('1000');
    assert.match(await page.locator('#settings-storage-error').innerText(),/僅於本次/);
    assert.deepEqual(errors,[]);
    console.log('PASS settings sidebar: seven-way navigation, preserved right view, themes/mobile, persistence/fallback, UART RX/TX/AT retention/export/filter eviction, per-session clear');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
