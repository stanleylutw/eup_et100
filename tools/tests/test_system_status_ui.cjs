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
    for (const selector of ['.dash-topline','.dash-can-strip','#dash-sensors-heading','#dash-devices']) {
      assert.equal(await page.locator(selector).isVisible(),false,selector+' must not appear on the ET-100 Dash');
    }
    assert.equal(await page.locator('.system-status-row[data-index="5"] .system-status-name-secondary').textContent(),'EG800Q-EU');
    assert.equal(await page.locator('.system-status-row[data-index="8"] .system-status-name-secondary').textContent(),'AG3352Q');
    assert.equal(await page.locator('.system-status-row[data-index="29"] .system-status-name-primary').textContent(),'看門狗');
    assert.equal(await page.locator('.system-status-row[data-index="9"] .system-status-schematic').textContent(),'3');
    assert.equal(await page.locator('.system-status-row[data-index="27"] .system-status-schematic').textContent(),'6/7/8');
    assert.equal(await page.locator('.system-status-row[data-index="2"] .system-status-schematic').textContent(),'—');
    assert.equal(await page.locator('.system-status-row[data-index="28"] .system-status-schematic').textContent(),'—');
    assert.match(await page.locator('.system-status-row[data-index="27"] .system-status-schematic').getAttribute('title'),/電路圖 Sheet 6\/7\/8/);
    assert.equal(await page.locator('.system-status-name .system-status-schematic').count(),0);
    assert.equal(await page.locator('.system-status-schematic').count(),33);
    const pdf = page.locator('#system-status-pdf');
    assert.equal(await pdf.getAttribute('target'),'_blank');
    const pdfUrl=await pdf.evaluate(el=>el.href);
    assert.equal(require('node:fs').existsSync(require('node:url').fileURLToPath(pdfUrl)),true);
    assert.match(pdfUrl,/QEM800QQA-FYN01AA_MB_SCH_V1\.1_20260923_1630\.pdf$/);
    assert.equal(await pdf.locator('svg').count(),1);
    assert.deepEqual(await page.locator('.system-status-row[data-index="9"]').evaluate(row=>({
      name:getComputedStyle(row.querySelector('.system-status-name-primary')).fontSize,
      english:getComputedStyle(row.querySelector('.system-status-name-secondary')).fontSize,
      sheet:getComputedStyle(row.querySelector('.system-status-schematic')).fontSize,
      status:getComputedStyle(row.querySelector('.system-status-value')).fontSize
    })),{name:'13px',english:'12px',sheet:'12px',status:'12px'});
    assert.ok(await page.locator('.system-status-row').evaluateAll(rows=>rows.every(row=>{
      const status=row.querySelector('.system-status-value').getBoundingClientRect();
      const sheet=row.querySelector('.system-status-schematic').getBoundingClientRect();
      return status.width===88 && sheet.width===52 && status.height===40 && sheet.height===40 && status.top===sheet.top;
    })));
    const wrapping=await page.locator('.system-status-row').first().evaluate(row=>{
      const chips=[row.querySelector('.system-status-value'),row.querySelector('.system-status-schematic')];
      const before=chips.map(chip=>({width:chip.offsetWidth,height:chip.offsetHeight}));
      chips[0].firstElementChild.textContent='這是兩行狀態文字';
      chips[1].firstElementChild.textContent='1/2/3/4/5/6';
      return chips.map((chip,i)=>({fixed:chip.offsetWidth===before[i].width && chip.offsetHeight===before[i].height,
        lines:chip.firstElementChild.getBoundingClientRect().height/16,
        fits:chip.firstElementChild.scrollHeight<=32}));
    });
    assert.ok(wrapping.every(chip=>chip.fixed && chip.lines===2 && chip.fits));
    await page.evaluate(()=>renderSystemStatus());
    assert.equal(await page.locator('.system-status-row[data-index="3"] .system-status-value').textContent(),'就緒');
    assert.equal(await page.locator('.system-status-row[data-index="5"] .system-status-value').textContent(),'未支援');
    assert.ok((await page.locator('.system-status-value').allTextContents()).every(text=>!/[A-Za-z]/.test(text)));
    assert.match(await page.locator('.system-status-row[data-index="3"] .system-status-value').getAttribute('title'),/狀態碼 2 · READY/);
    assert.equal(await page.locator('[data-count="ready"]').textContent(),'正常 1');
    assert.equal(await page.locator('[data-count="unsupported"]').textContent(),'未支援 32');
    assert.ok(await page.evaluate(()=>ET100System.registry.every(item=>Object.values(item.statuses).every(status=>systemStatusLabels[status]))));
    const columns=await page.locator('.system-status-row').evaluateAll(rows=>rows.map(row=>{
      const r=row.getBoundingClientRect();return {index:Number(row.dataset.index),x:r.x,y:r.y,height:r.height};
    }));
    assert.equal(columns[0].x,columns[10].x);
    assert.equal(columns[0].y,columns[11].y);
    assert.equal(columns[0].y,columns[22].y);
    assert.ok(columns.every(row=>row.height===52));
    await page.evaluate(()=>{
      const bytes=new TextEncoder().encode(ET100System.encode('ET100$SYS;11;04,3'));
      et100Client.decoder.feed(bytes.slice(0,9));et100Client.decoder.feed(bytes.slice(9));
    });
    assert.equal(await page.locator('.system-status-row[data-index="4"] .system-status-value').textContent(),'掃描中');
    await page.evaluate(()=>processFrame(ET100System.encode('ET100$SYS;11;04,99').trim()));
    assert.equal(await page.locator('.system-status-row[data-index="4"] .system-status-value').textContent(),'未知代碼');
    await page.evaluate(()=>processFrame('ET100$SYS;11;03,5*0000'));
    assert.match(await page.locator('#system-status-summary').textContent(),/異常/);
    assert.equal(await page.locator('.system-status-row[data-index="3"] .system-status-value').getAttribute('data-kind'),'unknown');
    assert.equal(await page.locator('.system-status-row[data-index="3"] .system-status-value').textContent(),'資料失效');
    await page.locator('#system-status-refresh').click();await page.waitForFunction(()=>systemStatusMessage==='已讀取 · 33 項');
    const before=await page.evaluate(()=>({values:[...systemStatus.values],count:sysRequests.length}));
    await page.evaluate(()=>processFrame(ET100System.encode('AT$PD=BLEGW$GSYSSTAT;0;ZZZZ;2').trim()));
    assert.deepEqual(await page.evaluate(()=>systemStatus.values),before.values);
    await page.screenshot({path:'/tmp/et100_system_status_desktop.png'});
    for(const width of [1920,1400,1050,390]) {
      await page.setViewportSize({width,height:900});
      const fits=await page.evaluate(()=>{
        const section=document.getElementById('system-status-section');
        return section.scrollWidth<=section.clientWidth &&
          [...section.querySelectorAll('.system-status-row')].every(r=>r.scrollWidth<=r.clientWidth &&
            [...r.querySelectorAll('span,small')].every(el=>el.scrollWidth<=el.clientWidth+1)) &&
          [...section.querySelectorAll('.system-status-schematic')].every(sheet=>{
            const a=sheet.previousElementSibling.getBoundingClientRect(),b=sheet.getBoundingClientRect();
            return b.left>=a.right && b.top===a.top && b.width===52 && b.height===40 && a.width===88 && a.height===40;
          });
      });
      assert.ok(fits,'SYS table overflow at '+width);
      const order=await page.locator('.system-status-row').evaluateAll(rows=>rows.map(row=>{
        const r=row.getBoundingClientRect();return {x:r.x,y:r.y};
      }));
      assert.ok(order.slice(1).every((r,i)=>r.x>order[i].x || (r.x===order[i].x && r.y>order[i].y)),
        'responsive layout must preserve top-to-bottom column order');
    }
    await page.screenshot({path:'/tmp/et100_system_status_mobile.png'});
    await page.setViewportSize({width:1440,height:1000});
    await page.evaluate(()=>{
      const pairs=ET100System.registry.map(r=>`${String(r.index).padStart(2,'0')},${({1:2,2:2,3:2,4:6,13:2,29:2,30:0})[r.index]??242}`).join(';');
      processFrame(ET100System.encode(`ET100$SYS;10;${pairs}`).trim());
    });
    assert.equal(await page.locator('[data-count="ready"]').textContent(),'正常 6');
    assert.equal(await page.locator('[data-count="unknown"]').textContent(),'未知 1');
    assert.equal(await page.locator('[data-count="unsupported"]').textContent(),'未支援 26');
    for(const theme of ['light','dark']) {
      await page.evaluate(theme=>document.documentElement.dataset.theme=theme,theme);
      await page.waitForTimeout(100);
      await page.screenshot({path:`/tmp/et100_system_status_${theme}.png`});
    }
    await page.evaluate(()=>processFrame(ET100System.encode('ET100$SYS;11;02,3;03,1;21,3;22,241;23,240;24,243').trim()));
    assert.equal(await page.locator('[data-count="error"]').textContent(),'異常 2');
    assert.equal(await page.locator('[data-count="waiting"]').textContent(),'進行中 1');
    assert.equal(await page.locator('[data-count="notFitted"]').textContent(),'未裝配 1');
    assert.equal(await page.locator('[data-count="disabled"]').textContent(),'已停用 1');
    assert.equal(await page.locator('[data-count="stale"]').textContent(),'資料失效 1');
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
    for (const selector of ['.dash-topline','.dash-can-strip','#dash-sensors-heading','#dash-devices']) {
      assert.equal(await page.locator(selector).isVisible(),true,selector+' must remain on other Gateway dashboards');
    }
    assert.deepEqual(errors,[]);
    console.log('PASS SYS UI: Dash 33 items, query token/CRC/delta/unknown, boot/offline/timeout, firmware gate and responsive layouts');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
