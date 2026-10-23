// Navigation-only browser test, no hardware access.
const assert=require('node:assert/strict'),path=require('node:path');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || '/Users/stanley/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async()=>{
  const browser=await chromium.launch({headless:true,channel:'chrome'});
  try {
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:{getPorts:async()=>[]}}));
    await page.goto(pathToFileURL(path.resolve(__dirname,'../rs232_tester.html')).href);
    assert.deepEqual(await page.locator('.tab-bar .tab-btn').evaluateAll(buttons=>buttons.map(button=>button.dataset.tab)),
      ['dash','ble','can','ota','gatt','lte']);
    assert.equal(await page.locator('.tab-btn[aria-current="page"]').getAttribute('data-tab'),'dash');
    assert.equal(await page.locator('#dash-sidebar').isVisible(),true);
    assert.equal(await page.locator('#dash-page').isVisible(),true);
    assert.equal(await page.locator('#ble-sidebar').isVisible(),false);
    for (const theme of ['light','dark']) {
      await page.evaluate(theme=>document.documentElement.dataset.theme=theme,theme);
      assert.deepEqual(await page.locator('.sidebar-content').evaluate(el=>({
        width:getComputedStyle(el,'::-webkit-scrollbar').width,
        track:getComputedStyle(el,'::-webkit-scrollbar-track').backgroundColor,
        thumb:getComputedStyle(el,'::-webkit-scrollbar-thumb').backgroundColor,
        border:getComputedStyle(el).getPropertyValue('--border').trim()
      })).then(({width,track})=>({width,track})),{width:'5px',track:'rgba(0, 0, 0, 0)'});
    }
    for(const width of [1440,1920,390]) {
      await page.setViewportSize({width,height:1000});
      for(const tab of ['ble','can','ota','gatt','dash','lte']) {
        const button=page.locator(`[data-tab="${tab}"]`);
        await button.click();
        assert.equal(await button.getAttribute('aria-current'),'page');
        assert.equal(await page.locator('.tab-btn[aria-current="page"]').count(),1);
        assert.equal(await button.locator('svg').count(),1);
        assert.equal(await page.locator(`#${tab}-sidebar`).isVisible(),true);
        const geometry=await page.evaluate(()=>{
          const nav=document.querySelector('.tab-bar').getBoundingClientRect();
          const content=document.querySelector('.sidebar-content').getBoundingClientRect();
          return {nav:nav.width,right:nav.right,left:content.left,overflow:document.querySelector('.sidebar-content').scrollWidth>content.width+1};
        });
        assert.equal(geometry.nav,48);assert.ok(geometry.left>=geometry.right-1);
        assert.equal(geometry.overflow,false,`${width}/${tab} controls overflow`);
        await page.mouse.move(350,10);await button.hover();
        assert.equal(await page.locator('#nav-tooltip').isVisible(),true);
        assert.equal(await page.locator('#nav-tooltip').innerText(),await button.getAttribute('aria-label'));
        await button.press('Escape');assert.equal(await page.locator('#nav-tooltip').isVisible(),false);
      }
      await page.locator('[data-tab="ota"]').click();
      await page.waitForTimeout(200);
      await page.screenshot({path:`/tmp/et100_nav_${width}.png`});
    }
    await page.setViewportSize({width:1440,height:1000});
    await page.locator('[data-tab="lte"]').click();
    assert.equal(await page.locator('#lte-baud-select').inputValue(),'9600');
    assert.equal(await page.locator('#lte-connect-btn').isDisabled(),true);
    const row = await page.locator('.lte-connection-row').evaluate(el => {
      const rects = [...el.children].map(child => child.getBoundingClientRect());
      return rects.every(rect => Math.abs(rect.top - rects[0].top) < 10) && el.scrollWidth <= el.clientWidth;
    });
    assert.equal(row,true,'LTE connection controls must fit on one row');
    await page.evaluate(() => {
      window.mockLtePorts = [];
      navigator.serial.requestPort = async () => {
        const port = {
          async open(options) {
            this.options = options;
            this.readable = new ReadableStream();
            this.writable = new WritableStream();
            this.closed = false;
          },
          async close() {
            if (this.readable.locked || this.writable.locked) throw new Error('Port is locked');
            this.closed = true;
          }
        };
        window.mockLtePorts.push(port);
        return port;
      };
    });
    await page.locator('#lte-channel-tx').click();
    await page.waitForFunction(()=>!document.getElementById('lte-channel-tx').disabled);
    assert.equal(await page.locator('#lte-channel-tx').getAttribute('aria-pressed'),'true');
    await page.locator('#lte-channel-rx').click();
    await page.waitForFunction(()=>!document.getElementById('lte-channel-rx').disabled);
    assert.deepEqual(await page.evaluate(()=>mockLtePorts.map(port=>port.options)),[undefined,undefined],
      'choosing ports must not open them');
    await page.locator('#lte-connect-btn').click();
    await page.waitForFunction(() => !document.getElementById('lte-connect-btn').disabled);
    assert.equal(await page.locator('#lte-connect-btn').innerText(),'中斷');
    await page.mouse.move(600,10);
    await page.waitForTimeout(300);
    assert.equal(await page.locator('#lte-connect-btn').evaluate(el=>getComputedStyle(el).backgroundColor),
      await page.evaluate(()=>{const el=document.createElement('span');el.style.color='var(--err)';document.body.append(el);const color=getComputedStyle(el).color;el.remove();return color;}));
    assert.deepEqual(await page.evaluate(()=>mockLtePorts.map(port=>port.options.baudRate)),[9600,9600]);
    await page.screenshot({path:'/tmp/et100_lte_connected.png'});
    await page.locator('#lte-connect-btn').click();
    await page.waitForFunction(() => !document.getElementById('lte-connect-btn').disabled);
    assert.equal(await page.locator('#lte-connect-btn').innerText(),'連線');
    assert.deepEqual(await page.evaluate(()=>mockLtePorts.map(port=>port.closed)),[true,true]);
    await page.locator('#lte-mode-select').selectOption('single');
    assert.equal(await page.locator('#lte-channel-tx').isDisabled(),true);
    assert.equal(await page.locator('#lte-channel-rx').isDisabled(),false);
    await page.locator('#lte-connect-btn').click();
    await page.waitForFunction(() => !document.getElementById('lte-connect-btn').disabled);
    assert.equal(await page.evaluate(()=>mockLtePorts.length),2,'connect must not open a chooser');
    assert.equal(await page.evaluate(()=>mockLtePorts[0].closed),true,'single mode must not open GSM_TX');
    await page.locator('#lte-connect-btn').click();
    await page.waitForFunction(() => !document.getElementById('lte-connect-btn').disabled);
    await page.locator('#lte-mode-select').selectOption('dual');
    await page.evaluate(()=>navigator.serial.requestPort=async()=>{throw new DOMException('Cancelled','NotFoundError');});
    await page.locator('#lte-channel-tx').click();
    await page.waitForFunction(() => !document.getElementById('lte-channel-tx').disabled);
    assert.equal(await page.locator('#lte-channel-tx').getAttribute('aria-pressed'),'true','cancel keeps the previous selection');
    assert.equal(await page.locator('#lte-connect-btn').innerText(),'連線');
    assert.equal(await page.locator('.lte-status-table tbody tr').count(),9);
    await page.evaluate(()=>{
      lteParseATStatus('+CPIN: READY');lteParseATStatus('+CSQ: 20,99');
      lteParseATStatus('+CEREG: 0,1');lteParseATStatus('+QCCID: 89880000123456789012');
      lteParseATStatus('Revision: EG800QEU_VERSION_WITH_LONG_IDENTIFIER');
    });
    assert.equal(await page.locator('#lte-hud-sim').innerText(),'READY');
    assert.equal(await page.locator('#lte-hud-iccid').innerText(),'89880000123456789012');
    assert.doesNotMatch(await page.locator('.lte-status-table').innerText(),/\p{Extended_Pictographic}/u);
    assert.equal(await page.locator('.sidebar-content').evaluate(el=>el.scrollWidth>el.clientWidth),false);
    await page.screenshot({path:'/tmp/et100_lte_table.png'});
    await page.evaluate(()=>lteResetHud());
    assert.equal(await page.locator('#lte-hud-fw').innerText(),'--');
    assert.deepEqual(errors,[]);
    console.log('PASS icon rail: six views, active state, tooltips/Escape, desktop/mobile geometry and no control overflow');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
