// Playwright：只使用模擬 Web Serial，不存取 USB／EVB。
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || '/Users/stanley/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const root=path.resolve(__dirname,'../..');
(async()=>{
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
            if(mock.uploaded&&!mock.reset)response.images.push({slot:1,version:mock.file.version,hash:mock.file.tlvHash,bootable:true});
          } else if(key==='2/1/1') {
            if(mock.failRc)response={rc:mock.failRc};
            else {mock.uploaded=body.off+body.data.length;response={rc:0,off:mock.uploaded};}
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
    await page.locator('[data-tab="ota"]').click();await page.locator('[data-ota-mode="et100"]').click();
    await page.locator('#connect-btn').click();await page.locator('#et100-query').click();
    await page.waitForFunction(()=>document.querySelector('#et100-device').textContent.includes('0.1.0.0'));
    assert.match(await page.locator('#log').textContent(),/BLEGW\$GFWINFO;[A-Z0-9]{4}/);
    assert.match(await page.locator('#log').textContent(),/AT\$PD=BLEGW\$GFWINFO;1;EUP_ET100;ET100_EVB_DEV1.4;/);
    await page.locator('#et100-file').setInputFiles(path.join(root,'fw/build_serial/output/dfu_application.zip'));
    await page.waitForFunction(()=>!document.querySelector('#et100-start').disabled);
    assert.match(await page.locator('#et100-file-info').textContent(),/board／soc 通過/);
    await page.screenshot({path:'/tmp/et100_smp_desktop.png',fullPage:true});
    await page.setViewportSize({width:390,height:844});
    await page.screenshot({path:'/tmp/et100_smp_mobile.png',fullPage:true});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'horizontal page overflow');
    await page.setViewportSize({width:1440,height:1000});
    await page.evaluate(()=>{
      mockSerial.file=et100File;et100Client.io.now=()=>mockSerial.clock;
      et100Client.io.sleep=async n=>{mockSerial.clock+=n;};
    });
    await page.locator('#et100-start').click();
    await page.waitForFunction(()=>document.querySelector('#et100-result').dataset.kind==='success');
    assert.match(await page.locator('#et100-result').textContent(),/更新成功/);
    assert.match(await page.locator('#et100-file-info').textContent(),/裝置／檔案硬體一致/);
    let state=await page.evaluate(()=>({opens:mockSerial.opens,requests:mockSerial.requests,polls:mockSerial.polls}));
    assert.deepEqual(state.opens,[9600,9600,9600,115200,9600]);
    assert.ok(state.requests.includes('2/0/5'));assert.equal(state.polls,2);
    assert.ok(!state.requests.some(s=>s.includes('ZVERO')||s.includes('SOTASTAGE')));
    await page.evaluate(()=>{mockSerial.failRc=10;mockSerial.reset=false;mockSerial.uploaded=0;});
    await page.locator('#et100-start').click();await page.waitForFunction(()=>document.querySelector('#et100-result').dataset.kind==='error');
    assert.match(await page.locator('#et100-result').textContent(),/EBUSY/);
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
    await page.locator('[data-ota-mode="gateway"]').click();assert.equal(await page.locator('#ota-rs232-panel').isVisible(),true);
    await page.locator('[data-ota-mode="sensor"]').click();assert.equal(await page.locator('#ota-device-submode').isVisible(),true);
    assert.deepEqual(errors,[],'uncaught browser errors');
    console.log('PASS browser mock Serial: GFWINFO match/project-hardware-soc mismatch/pre-0.8.7, ZIP select, GCHK/state, close/open baud lifecycle, full update/confirm, EBUSY, original mode switching, desktop/mobile layout');
  } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
