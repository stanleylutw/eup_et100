// Mock RS232 only. No serial hardware is requested or opened.
const assert = require('node:assert/strict');
const path = require('node:path');
const {pathToFileURL} = require('node:url');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || '/Users/stanley/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async () => {
  const browser = await chromium.launch({headless:true,channel:'chrome'});
  try {
    const page = await browser.newPage({viewport:{width:1440,height:1000}});
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(() => Object.defineProperty(navigator,'serial',{value:{getPorts:async()=>[]}}));
    await page.goto(pathToFileURL(path.resolve(__dirname,'../rs232_tester.html')).href);
    await page.locator('[data-tab="dash"]').click();
    assert.equal(await page.locator('#dash-device-filters').isVisible(),true);
    await page.evaluate(() => {
      window.dashRequests = [];
      window.dashFailMax = false;
      writer = {write:async bytes => {
        const request = new TextDecoder().decode(bytes).trim();
        dashRequests.push(request);
        const [name,token] = request.replace('BLEGW$','').split(';');
        const payloads = {
          GCHK:'C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.9.2-dev;0',
          GFWINFO:'EUP_ET100;ET100_EVB_DEV1.4;0.9.2-dev;fr3068e_c',
          GCID:'0639;;;',GLIST:'EEACB6F5627E;112233445566;;;;;;',GGTIME:'15',GMAXRPT:'3',
          GSOTAPKG:`VALID;12;167311;167055;EUP_AIoT_P1;p1_nrf52832;nrf52832;v3.6.15;${'A'.repeat(64)};20261020174340`
        };
        if (!(name in payloads) || (name === 'GMAXRPT' && dashFailMax)) return;
        const reply = `AT$PD=BLEGW$${name};1;${payloads[name]};${token}`;
        setTimeout(() => {
          if (name === 'GSOTAPKG' && et100TransportMode()) et100Client.decoder.feed(new TextEncoder().encode(reply+'\r\n'));
          else processFrame(reply);
        },5);
      }};
      setConnected(true);
      processFrame('AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.9.2-dev;0;INIT');
    });
    assert.equal(await page.locator('#dash-device-filters').isVisible(),false);
    assert.equal(await page.locator('#dash-et100-info').isVisible(),true);
    const system = page.locator('#dash-system-rows');
    const settings = page.locator('#dash-settings-rows');
    assert.equal(await system.locator('tr').count(),8);
    assert.equal(await settings.locator('tr').count(),4);
    assert.equal(await settings.locator('tr').nth(2).locator('td').innerText(),'—','must not show default GTIME as a read value');
    await page.locator('#dash-system-refresh').click();
    await page.waitForFunction(() => dashInfo.pending === null);
    assert.match(await system.innerText(),/EUP_ET100/);
    assert.match(await system.innerText(),/ET100_EVB_DEV1.4/);
    assert.match(await system.innerText(),/fr3068e_c/);
    assert.equal(await system.locator('tr').nth(7).locator('td').innerText(),'v3.6.15');
    for (const index of [5,6]) assert.equal(await system.locator('tr').nth(index).locator('td').innerText(),'—');
    await page.locator('#dash-settings-refresh').click();
    await page.waitForFunction(() => dashInfo.pending === null);
    assert.match(await settings.innerText(),/0639/);
    assert.match(await settings.innerText(),/15 s/);
    assert.equal(await settings.locator('tr').nth(3).locator('td').innerText(),'3');
    await settings.locator('summary').click();
    assert.deepEqual(await settings.locator('.dash-mac').allTextContents(),['EEACB6F5627E','112233445566']);
    await page.evaluate(() => processFrame('AT$PD=BLEGW$GSOTAPKG;1;EMPTY;13;-;-;-;-;-;-;-;-;TEST'));
    assert.equal(await system.locator('tr').nth(7).locator('td').innerText(),'未儲存');
    await page.evaluate(() => processFrame('AT$PD=BLEGW$GSOTAPKG;1;INVALID;14;-;-;-;-;-;-;-;-;TEST'));
    assert.equal(await system.locator('tr').nth(7).locator('td').innerText(),'無效');
    await page.evaluate(() => processFrame('AT$PD=BLEGW$DSOTAPKG;1;TEST'));
    assert.equal(await system.locator('tr').nth(7).locator('td').innerText(),'未儲存');
    await page.evaluate(() => {dashFailMax = true;});
    await page.locator('#dash-settings-refresh').click();
    await page.waitForFunction(() => dashInfo.pending === null);
    assert.equal(await settings.locator('tr').nth(3).locator('td').innerText(),'讀取失敗');
    for (const theme of ['light','dark']) {
      await page.evaluate(theme => document.documentElement.dataset.theme=theme,theme);
      for (const width of [1440,390]) {
        await page.setViewportSize({width,height:1000});
        assert.equal(await page.locator('.sidebar-content').evaluate(el => el.scrollWidth > el.clientWidth),false);
        await page.screenshot({path:`/tmp/et100_dash_info_${theme}_${width}.png`});
      }
    }
    await page.evaluate(() => processFrame(ET100System.encode('ET100$SYS;2').trim()));
    assert.equal(await page.evaluate(() => Object.keys(dashInfo.fields).length),0,'boot must invalidate the sidebar cache');
    await page.evaluate(() => setConnected(false));
    assert.equal(await page.evaluate(() => Object.keys(dashInfo.fields).length),0);
    await page.evaluate(() => {
      setConnected(true);
      processFrame('AT$PD=BLEGW$GCHK;1;AABBCCDDEEFF;EUP;V1_nrf52840;3.0.0;0;INIT');
    });
    assert.equal(await page.locator('#dash-device-filters').isVisible(),true);
    assert.equal(await page.locator('#dash-et100-info').isVisible(),false);
    assert.ok(await page.evaluate(() => dashRequests.every(r => /^BLEGW\$(?:GCHK|GFWINFO|GSOTAPKG|GCID|GLIST|GGTIME|GMAXRPT|GSYSSTAT);/.test(r))));
    assert.deepEqual(errors,[]);
    console.log('PASS Dash sidebar: ASCII system/settings, NOR states, MAC list, timeout, disconnect reset, non-ET100 filters, responsive light/dark');
  } finally {await browser.close();}
})().catch(error => {console.error(error);process.exitCode=1;});
