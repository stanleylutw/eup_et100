// 離線協定／狀態機測試；不開啟 UART，不寫入 EVB。
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {webcrypto} = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const html = fs.readFileSync(path.join(root, 'tools/rs232_tester.html'), 'utf8');
const core = html.match(/<script id="et100-smp-core">([\s\S]*?)<\/script>/)[1];
const context = vm.createContext({Uint8Array, DataView, TextEncoder, TextDecoder, crypto:webcrypto,
  btoa, atob, console, setTimeout, clearTimeout});
vm.runInContext(core + '\nthis.S = ET100Smp;', context);
const S = context.S;
const zip = require('../vendor/fflate.min.js');
const image = new Uint8Array(fs.readFileSync(process.argv[2] || path.join(root, 'fw/build_serial/output/et100.signed.bin')));
const metadataAt = Buffer.from(image).indexOf('EUPFWV1\0');
const board = {type:'application', file:'et100.signed.bin',board:'dev14_fr3068ep_c',soc:'fr3068e_c',size:image.length};
async function reseal(bytes) {
  const end=bytes.length-40;
  bytes.set(new Uint8Array(await webcrypto.subtle.digest('SHA-256',bytes.slice(0,end))),end+8);
  return bytes;
}
class Device {
  constructor(file,options={}) {
    this.file=file; this.options=options; this.at=0; this.clock=0; this.baud=9600; this.writes=[];
    this.received=new Uint8Array(file.bytes.length); this.off=options.resume || 0;
    this.received.set(file.bytes.slice(0,this.off)); this.opens=[]; this.polls=0;
    this.client=new S.Client({write:async b=>this.write(b),reopen:async n=>{this.baud=n;this.opens.push(n);},
      token:()=> 'AB12',now:()=>this.clock,sleep:async n=>{this.clock+=n;},
      stage:()=>{},identity:result=>{this.identity=result;},progress:()=>{if(options.cancel)this.client.cancel();}});
    this.decoder=new S.Decoder(p=>this.handle(p));
  }
  slot(version='0.1.0.0',hash=new Uint8Array(32),confirmed=true) {return {slot:0,version,hash,confirmed,active:true,bootable:true};}
  respond(p,body) {
    const packet=S.packet((p[0]&7)+1,p[4]*256+p[5],p[7],p[6],body);
    // 非 SMP 行、任意 USB chunk 邊界都不能影響解碼。
    const data=S.concat(new TextEncoder().encode('installer log ignored\r\n'),S.frame(packet));
    for(let at=0;at<data.length;at+=17)this.client.decoder.feed(data.slice(at,at+17));
  }
  write(b) {
    const txt=new TextDecoder().decode(b);
    if(txt.startsWith('BLEGW$')) {
      const fields=txt.trim().split(';');
      if(fields[0]==='BLEGW$GCHK') {assert.equal(this.baud,9600);this.client.decoder.feed(new TextEncoder().encode('AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.6-dev;0;'+fields[1]+'\r\n'));}
      else if(fields[0]==='BLEGW$GFWINFO') {
        assert.equal(this.baud,9600);
        const info={project:this.file.project,hardware:this.file.hardware,soc:'fr3068e_c',...this.options.fwInfo};
        const body=this.options.pre087 ? '0;' : this.options.pre087Long ? '0;;;;;' :
          `1;${info.project};${info.hardware};0.8.7-dev;${info.soc};`;
        this.client.decoder.feed(new TextEncoder().encode('AT$PD=BLEGW$GFWINFO;'+body+fields[1]+'\r\n'));
      }
      else if(fields[0]==='BLEGW$OTABAUD')this.client.decoder.feed(new TextEncoder().encode('AT$PD='+txt.trim().replace(';115200;',';1;115200;').replace(';9600;',';1;9600;')+'\r\n'));
      return;
    }
    this.decoder.feed(b);
  }
  handle(p) {
    assert.ok(p.length<=1024);const body=S.uncbor(p.slice(8));
    const key=`${p[0]&7}/${p[4]*256+p[5]}/${p[7]}`;this.writes.push({key,body,baud:this.baud});
    if(key==='0/1/0') {
      if(this.reset) {
        assert.equal(this.baud,9600);this.polls++;
        if(this.options.unknown)return this.respond(p,{images:[this.slot('9.9.9.0',new Uint8Array(32),false)]});
        if(this.options.rollback)return this.respond(p,{images:[this.slot()]});
        return this.respond(p,{images:[this.slot(this.file.version,this.file.tlvHash,this.polls>(this.options.confirmAfter||1))]});
      }
      if(!this.writes.some(w=>w.key==='2/1/1'))return this.respond(p,{images:[this.slot()]});
      return this.respond(p,{images:[this.slot(),{slot:1,version:this.file.version,hash:this.file.tlvHash,bootable:true,pending:false}]});
    }
    if(key==='2/1/1') {
      assert.equal(this.baud,115200);assert.equal(body.upgrade,undefined);assert.ok(body.data.length<=600);
      if(this.options.rc)return this.respond(p,{rc:this.options.rc});
      if(body.off===0) {
        assert.equal(body.len,this.file.bytes.length);assert.ok(S.equal(body.sha,this.file.sha));
        if(this.off)return this.respond(p,{rc:0,off:this.off});
      }
      assert.equal(body.off,this.off);this.received.set(body.data,this.off);this.off+=body.data.length;
      return this.respond(p,{rc:0,off:this.off});
    }
    if(key==='2/1/0') {assert.ok(S.equal(body.hash,this.file.tlvHash));assert.equal(body.confirm,false);return this.respond(p,{images:[]});}
    if(key==='2/0/5') {this.reset=true;return this.respond(p,{});}
    throw Error('Unexpected request '+key);
  }
}
(async()=>{
  assert.equal(S.crc16(new TextEncoder().encode('123456789')),0x31c3);
  assert.equal(S.crc32(new TextEncoder().encode('123456789')),0xcbf43926);
  for(const n of [8,100,700,1024]) {
    const p=Uint8Array.from({length:n},(_,i)=>i&255);let got;
    const d=new S.Decoder(b=>got=b);for(const b of S.frame(p))d.feed(Uint8Array.of(b));
    assert.ok(S.equal(p,got));
  }
  let packets=0;const bad=new S.Decoder(()=>packets++), corrupt=S.frame(S.packet(2,1,1,1,{}));
  corrupt[8]=corrupt[8]===65?66:65;bad.feed(corrupt);assert.equal(packets,0);
  const file=await S.validate(image,board);assert.notDeepEqual(file.sha,file.tlvHash);
  const zipped=zip.zipSync({'manifest.json':new TextEncoder().encode(JSON.stringify({files:[board]})),[board.file]:image});
  assert.equal((await S.loadFile('dfu_application.zip',zipped,zip.unzipSync)).version,file.version);
  await assert.rejects(S.validate(image,{...board,board:'nrf52840'}),/board/);
  await assert.rejects(S.validate(image,{...board,soc:'nrf52'}),/soc/);
  const zero=image.slice();new DataView(zero.buffer).setUint32(metadataAt+282,0,true);
  await assert.rejects(S.validate(await reseal(zero)),/CRC/);
  const wrong=image.slice();wrong[metadataAt+282]^=1;
  await assert.rejects(S.validate(await reseal(wrong)),/CRC/);
  const foreign=image.slice();foreign[metadataAt+10]=78;
  await assert.rejects(S.validate(await reseal(foreign)),/ET-100/);
  const broken=image.slice();broken[700]^=1;await assert.rejects(S.validate(broken),/SHA/);
  assert.throws(()=>S.uncbor(Uint8Array.of(0xa1,0x61)),/截斷/);
  for(const resume of [0,4096]) {
    const d=new Device(file,{resume});const result=await d.client.update(file);
    assert.equal(result.kind,'success');assert.ok(S.equal(d.received,file.bytes));
    assert.deepEqual(d.opens,[9600,115200,9600]);assert.equal(d.polls,2);
    assert.ok(d.writes.some(w=>w.key==='2/0/5'));
    assert.equal(d.identity.hardwareCheck,'裝置／檔案硬體一致');
  }
  // 三個欄位都必須逐一比對；在切速／upload／Test 之前拒絕。
  for(const fwInfo of [{project:'OTHER'},{hardware:'ET100_EVB_OTHER'},{soc:'nrf52840'}]) {
    const d=new Device(file,{fwInfo});
    await assert.rejects(d.client.update(file),/裝置／檔案硬體不符/);
    assert.deepEqual(d.opens,[9600]);assert.equal(d.writes.length,0);
  }
  for(const option of ['pre087','pre087Long']) {
    const d=new Device(file,{[option]:true});assert.equal((await d.client.update(file)).kind,'success');
    assert.equal(d.identity.fwInfo,null);assert.equal(d.identity.hardwareCheck,'裝置韌體早於 0.8.7，無法比對硬體');
  }
  // BIN 沒有 manifest，仍以固定 FR3068 SoC 與 EUPFWV1 比對。
  const bin=await S.validate(image), binDevice=new Device(bin);assert.equal((await binDevice.client.update(bin)).kind,'success');
  for(const timedOut of ['GCHK','GFWINFO']) {
    const d=new Device(file), original=d.client.ascii.bind(d.client);
    d.client.ascii=(command,...args)=>command.startsWith('BLEGW$'+timedOut+';')?
      Promise.reject(Error('ASCII 回應逾時')):original(command,...args);
    await assert.rejects(d.client.identify(file),/若剛中斷更新，請等 30 秒後再查詢/);
  }
  const late=new Device(file,{confirmAfter:30});assert.equal((await late.client.update(file)).kind,'success');
  assert.equal(late.clock,155000); // 超過原本 120 秒，但仍在新 180 秒期限內。
  const cancelled=new Device(file,{cancel:true});await assert.rejects(cancelled.client.update(file),/取消/);
  assert.ok(!cancelled.writes.some(w=>w.key==='2/1/0'||w.key==='2/0/5'));
  assert.equal(cancelled.opens.at(-1),9600);
  for(const rc of [10,6,9]) {
    const d=new Device(file,{rc});await assert.rejects(d.client.update(file),new RegExp(['', '', '', '', '', '', 'EBADSTATE','','','ECORRUPT','EBUSY'][rc]));
    assert.ok(!d.writes.some(w=>w.key==='2/1/0'));
  }
  assert.equal((await new Device(file,{rollback:true}).client.update(file)).kind,'rollback');
  const unknown=new Device(file,{unknown:true});await assert.rejects(unknown.client.update(file),/180 秒.*結果未知/);assert.equal(unknown.clock,180000);
  const d=new Device(file);const request=d.client.request(0,1,0);await request;
  console.log('PASS CRC/CBOR/framing, ZIP/BIN validation, GFWINFO match/project-hardware-soc mismatch/pre-0.8.7, ASCII timeout hint, full upload/resume, Test hash/reset=5, 180s confirm/rollback/timeout, cancel and error messages');
})().catch(e=>{console.error(e);process.exitCode=1;});
