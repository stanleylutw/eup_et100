// Shared header regression: simulated state and Web Serial only, no hardware.
const assert=require('node:assert/strict'),path=require('node:path');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || '/Users/stanley/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async()=>{
  const browser=await chromium.launch({headless:true,channel:'chrome'});
  try {
    const page=await browser.newPage({viewport:{width:1440,height:1000},acceptDownloads:true}),errors=[];
    page.on('pageerror',error=>errors.push(error.message));
    await page.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:{getPorts:async()=>[]}}));
    await page.goto(pathToFileURL(path.resolve(__dirname,'../rs232_tester.html')).href);
    const geometry=()=>page.evaluate(()=>{
      const rect=e=>{const r=e.getBoundingClientRect();return [r.x,r.y,r.width,r.height];};
      return [...document.querySelectorAll('#gateway-summary,#header-section-2,#header-section-3,#header-actions')].map(rect);
    });
    assert.equal(await page.locator('#app-header').count(),1);
    assert.equal(await page.locator('#app-header').evaluate(e=>getComputedStyle(e).backgroundColor),'rgb(229, 231, 235)');
    await page.locator('.gateway-summary-icon img').evaluate(img=>img.decode());
    assert.ok(await page.locator('.gateway-summary-icon img').evaluate(img=>img.naturalWidth>0),'local EUP logo must load');
    assert.equal(await page.locator('#logo-dark-outline feMorphology').getAttribute('radius'),'1');
    assert.equal(await page.locator('#logo-dark-outline feMorphology').getAttribute('in'),'SourceAlpha','outline includes red as well as black');
    assert.equal(await page.locator('#logo-dark-outline feFlood').getAttribute('flood-color'),'#ffffff');
    for(const id of ['connect-btn','theme-btn','frame-counter','lte-counter','gateway-summary-hardware','gateway-summary-firmware'])
      assert.equal(await page.locator(`[id="${id}"]`).count(),1,`duplicate moved control ${id}`);
    assert.equal(await page.locator('.main-panel > header').count(),0);
    assert.equal(await page.locator('#header-section-2').innerText(),'');
    for(const width of [1920,1440,1024,768,390,360]) {
      await page.setViewportSize({width,height:1000});
      const baseline=await geometry();
      assert.equal(await page.locator('#app-header').evaluate(e=>getComputedStyle(e).borderBottomWidth),'0px');
      assert.equal(await page.locator('#gateway-summary').evaluate(e=>getComputedStyle(e).borderRightWidth),'0px');
      assert.equal(await page.locator('#header-actions').evaluate(e=>getComputedStyle(e).borderLeftWidth),'0px');
      if(width>700) assert.equal(baseline[0][2],350,'desktop identity must align with the 350px sidebar');
      for(const tab of ['dash','ble','can','ota','gatt','lte']) {
        await page.locator(`[data-tab="${tab}"]`).click();
        assert.deepEqual(await geometry(),baseline,`${width}/${tab}: fixed header tracks moved`);
        assert.equal(await page.locator('#app-header').evaluate(e=>e.getBoundingClientRect().height),42);
        assert.equal(await page.locator('#uart-log-tools').isVisible(),!['dash','lte'].includes(tab));
        assert.equal(await page.locator('#at-log-tools').isVisible(),tab==='lte');
        assert.equal(await page.locator('#connect-btn').isVisible(),true);
        assert.ok(await page.locator('.sidebar-content .section').evaluateAll(sections=>
          sections.every(section=>getComputedStyle(section).borderBottomWidth==='0px')),
          `${width}/${tab}: sidebar sections must not have bottom dividers`);
        const fit=await page.locator('#app-header').evaluate(e=>{
          const r=e.getBoundingClientRect();
          return [...e.querySelectorAll('button,.counter,.gateway-summary-copy,.gateway-summary-icon img')].filter(el=>el.getClientRects().length)
            .every(el=>{const b=el.getBoundingClientRect();return b.left>=r.left && b.right<=r.right+.5 && b.top>=r.top && b.bottom<=r.bottom;});
        });
        assert.ok(fit,`${width}/${tab}: header control escaped the fixed bar`);
      }
    }
    await page.setViewportSize({width:1440,height:1000});
    await page.evaluate(()=>{
      setConnected(true);
      gatewayVersionInfo={hwVer:'V1_nrf52840',fwVer:'v0.6.0'};renderGatewaySummary();
    });
    assert.equal(await page.locator('#gateway-summary-hardware').innerText(),'V1_nrf52840');
    assert.equal(await page.locator('#gateway-summary-firmware').innerText(),'v0.6.0');
    const typography=await page.locator('#gateway-summary').evaluate(e=>{
      const lines=[...e.querySelectorAll('.header-device-line')],brand=e.querySelector('.gateway-summary-icon').getBoundingClientRect();
      const a=lines[0].getBoundingClientRect(),b=lines[1].getBoundingClientRect();
      return {first:parseFloat(getComputedStyle(lines[0]).fontSize),second:parseFloat(getComputedStyle(lines[1]).fontSize),
        color1:getComputedStyle(lines[0]).color,color2:getComputedStyle(lines[1]).color,
        brandCenter:brand.y+brand.height/2,linesCenter:(a.top+b.bottom)/2};
    });
    assert.ok(typography.first>typography.second && typography.color1!==typography.color2);
    assert.ok(Math.abs(typography.brandCenter-typography.linesCenter)<1);
    await page.locator('[data-tab="ble"]').click();
    await page.evaluate(()=>{processFrame('AT$PD=G1;1;112233445566;3906;0050*00');});
    const count=await page.locator('#frame-counter').innerText();
    assert.notEqual(count,'0 筆');
    await page.screenshot({path:'/tmp/et100_header_normal_desktop.png'});
    await page.locator('#app-header').screenshot({path:'/tmp/et100_header_normal_bar.png'});
    let downloadEvent=page.waitForEvent('download');
    await page.locator('#export-btn-log').click();
    assert.match((await downloadEvent).suggestedFilename(),/^rs232_log_/);
    await page.locator('[data-tab="lte"]').click();
    await page.evaluate(()=>lteAddLogEntry('4G RX','+CSQ: 20,99','tag-modem','modem'));
    assert.equal(await page.locator('#lte-counter').innerText(),'1 筆');
    downloadEvent=page.waitForEvent('download');
    await page.getByRole('button',{name:'匯出 AT Log',exact:true}).click();
    assert.match((await downloadEvent).suggestedFilename(),/\.txt$/);
    await page.getByRole('button',{name:'清除 AT Log',exact:true}).click();
    assert.equal(await page.locator('#lte-counter').innerText(),'0 筆');
    assert.equal(await page.locator('#frame-counter').innerText(),count,'AT clear must not clear the UART counter');
    await page.locator('[data-tab="ble"]').click();
    await page.locator('#clear-btn-log').click();
    assert.equal(await page.locator('#frame-counter').innerText(),'0 筆');
    await page.locator('#log-size-btn').click();assert.equal(await page.locator('#log-size-btn').getAttribute('aria-pressed'),'true');
    await page.locator('#filter-ble').click();assert.equal(await page.locator('#filter-ble').getAttribute('class'),'filter-chip');
    const baseline=await geometry();
    await page.evaluate(()=>{
      gatewayVersionInfo={hwVer:'VERY_LONG_DEVICE_NAME_'.repeat(10),fwVer:'0.9.4-long-build-'.repeat(12)};
      renderGatewaySummary();counter.textContent='999999999999999999 筆';
    });
    assert.deepEqual(await geometry(),baseline);
    assert.match(await page.locator('#gateway-summary-hardware').getAttribute('title'),/VERY_LONG/);
    await page.evaluate(()=>{
      const fixture=document.createElement('div');fixture.id='table-theme-fixture';fixture.hidden=true;
      fixture.innerHTML=`<table class="lte-status-table"><tr><th>SIM</th><td>READY</td></tr></table>
        <table class="packed-table"><tr><th>RPM</th></tr><tr><td>1200</td></tr></table>
        <table class="ota-debug-table"><tr><td>Hash</td><td>OK</td></tr></table>
        <div class="flash-viz-info"><div class="flash-viz-info-label">Stage</div><div class="flash-viz-info-value">Ready</div><div class="flash-viz-info-value error">Failed</div></div>
        <table class="gatt-list"><tr><td>GATT event</td></tr></table>
        <div class="can-type-card"><div class="can-type-hdr">CAN</div><div class="can-field"><span class="fl">Speed</span><span class="fv">60</span></div></div>
        <div class="sensor-card"><div class="sensor-card-hdr">BLE</div><div class="sensor-focus-field"><span class="label">RPM</span><span class="value">1200</span></div></div>
        <span class="lte-status-card-val val-ok">OK</span>`;
      document.body.append(fixture);
    });
    for(const theme of ['light','dark']) {
      await page.evaluate(t=>document.documentElement.dataset.theme=t,theme);
      const logoFilter=await page.locator('.gateway-summary-icon img').evaluate(e=>getComputedStyle(e).filter);
      if(theme==='dark') assert.match(logoFilter,/#logo-dark-outline/);
      else assert.equal(logoFilter,'none');
      const tableTheme=await page.locator('#table-theme-fixture').evaluate(fixture=>{
        const style=selector=>getComputedStyle(fixture.querySelector(selector));
        return {labels:['.lte-status-table th','.packed-table th','.ota-debug-table td:first-child','.flash-viz-info-label','.can-type-hdr','.sensor-card-hdr'].map(s=>style(s).backgroundColor),
          values:['.lte-status-table td','.packed-table td','.ota-debug-table td:last-child','.flash-viz-info-value','.gatt-list','.can-type-card','.sensor-card'].map(s=>style(s).backgroundColor),
          text:['.lte-status-table th','.can-field .fl','.sensor-focus-field .label'].map(s=>style(s).color),
          error:style('.flash-viz-info-value.error').color,ok:style('.val-ok').color};
      });
      if(theme==='dark') {
        assert.ok(tableTheme.labels.every(c=>c==='rgb(41, 43, 48)'),JSON.stringify(tableTheme));
        assert.ok(tableTheme.values.every(c=>c==='rgb(32, 33, 36)'),JSON.stringify(tableTheme));
        assert.ok(tableTheme.text.every(c=>c==='rgb(184, 189, 199)'),JSON.stringify(tableTheme));
        assert.equal(tableTheme.error,'rgb(232, 112, 112)');assert.equal(tableTheme.ok,'rgb(108, 207, 114)');
      } else {
        assert.equal(tableTheme.labels[0],'rgb(244, 247, 251)');
        assert.equal(tableTheme.labels[1],'rgb(242, 246, 250)');
      }
      assert.equal(await page.locator('.gateway-summary-icon img').evaluate(e=>getComputedStyle(e).backgroundColor),'rgba(0, 0, 0, 0)');
      const shell=await page.evaluate(()=>{
        const style=selector=>getComputedStyle(document.querySelector(selector));
        return {header:style('#app-header').backgroundColor,rail:style('.tab-bar').backgroundColor,
          sidebar:style('.sidebar-content').backgroundColor,main:style('.main-panel').backgroundColor,
          radius:style('.sidebar-content').borderTopLeftRadius,
          sidebarBorder:style('.sidebar').borderRightWidth,railBorder:style('.tab-bar').borderRightWidth};
      });
      assert.equal(shell.header,shell.rail,'header and icon rail must form one shell');
      assert.equal(shell.header,theme==='light'?'rgb(229, 231, 235)':'rgb(39, 40, 45)');
      assert.notEqual(shell.sidebar,shell.main,'sidebar controls and main data need distinct surfaces');
      assert.equal(shell.sidebar,theme==='light'?'rgb(247, 248, 250)':'rgb(32, 33, 36)');
      assert.notEqual(shell.header,shell.sidebar);
      assert.equal(shell.radius,'24px');
      assert.equal(shell.sidebarBorder,'0px');assert.equal(shell.railBorder,'0px');
      for(const width of [1440,390]) {
        await page.setViewportSize({width,height:1000});
        const divider=await page.locator('.sidebar-content').evaluate(e=>{
          const s=getComputedStyle(e);return {right:s.borderRightWidth,bottom:s.borderBottomWidth,
            color:innerWidth>700?s.borderRightColor:s.borderBottomColor};
        });
        assert.equal(divider.right,width>700?'1px':'0px');
        assert.equal(divider.bottom,width>700?'0px':'1px');
        assert.equal(divider.color,theme==='light'?'rgb(225, 228, 232)':'rgb(52, 54, 59)');
        await page.screenshot({path:`/tmp/et100_header_${theme}_${width}.png`});
      }
    }
    await page.locator('#table-theme-fixture').evaluate(e=>e.remove());
    await page.emulateMedia({colorScheme:'dark'});
    await page.evaluate(()=>document.documentElement.removeAttribute('data-theme'));
    assert.match(await page.locator('.gateway-summary-icon img').evaluate(e=>getComputedStyle(e).filter),/#logo-dark-outline/);
    await page.evaluate(()=>document.documentElement.dataset.theme='light');
    assert.equal(await page.locator('.gateway-summary-icon img').evaluate(e=>getComputedStyle(e).filter),'none');
    await page.emulateMedia({colorScheme:'light'});
    await page.locator('#theme-btn').click();
    assert.ok(await page.locator('html').getAttribute('data-theme'));
    await page.evaluate(()=>setConnected(false));
    assert.equal(await page.locator('#gateway-summary-hardware').innerText(),'—');
    assert.equal(await page.locator('#gateway-summary-firmware').innerText(),'—');
    assert.deepEqual(errors,[]);
    console.log('PASS shared two-line header: fixed tracks, six views, responsive bounds, UART/AT tools, exports, identity/reset and themes');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
