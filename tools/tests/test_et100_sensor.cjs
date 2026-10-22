// 離線驗證 v0.8.8；不開 UART，不寫 NOR。
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {webcrypto}=require('node:crypto');
const fixture=require('./et100_sensor_fixture.cjs');
const html=fs.readFileSync(path.join(__dirname,'../rs232_tester.html'),'utf8');
const context=vm.createContext({Uint8Array,DataView,TextEncoder,TextDecoder,crypto:webcrypto,btoa,atob,setTimeout,clearTimeout});
vm.runInContext(html.match(/<script id="et100-smp-core">([\s\S]*?)<\/script>/)[1]+'\nthis.S=ET100Smp;',context);
const S=context.S;
class Device {
  constructor(file,options={}) {
    this.file=file;this.options=options;this.opens=[];this.commands=[];this.requests=[];
    this.off=options.resume||0;this.state='EMPTY';this.clock=0;
    this.client=new S.Client({write:async bytes=>this.write(bytes),reopen:async baud=>this.opens.push(baud),token:()=> 'TEST',
      now:()=>this.clock,sleep:async n=>{this.clock+=n;},stage:()=>{},identity:()=>{},progress:()=>{if(options.cancel)this.client.cancel();}});
    this.decoder=new S.Decoder(p=>this.handle(p));
  }
  line(text){this.client.decoder.feed(new TextEncoder().encode(text+'\r\n'));}
  write(bytes) {
    const text=new TextDecoder().decode(bytes).trim();
    if(!text.startsWith('BLEGW$')) {this.decoder.feed(bytes);return;}
    this.commands.push(text);const [cmd,...f]=text.split(';'),token=f.at(-1);
    if(cmd==='BLEGW$GCHK')this.line(`AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.8-dev;0;${token}`);
    else if(cmd==='BLEGW$GFWINFO')this.line(`AT$PD=BLEGW$GFWINFO;1;${this.options.project||'EUP_ET100'};ET100_EVB_DEV1.4;0.8.8-dev;fr3068e_c;${token}`);
    else if(cmd==='BLEGW$OTABAUD')this.line(`AT$PD=BLEGW$OTABAUD;1;${f[0]};${token}`);
    else if(cmd==='BLEGW$GSOTAPKG') {
      if(this.options.unsupported)return this.line(`AT$PD=BLEGW$GSOTAPKG;0;UNSUPPORTED;${token}`);
      if(this.options.malformed)return this.line(`AT$PD=BLEGW$GSOTAPKG;1;VALID;bad;${token}`);
      const m=this.file.metadata;
      const sha=this.options.badSha?'0'.repeat(64):m.upload_sha256;
      const fields=this.state==='VALID'?`1;VALID;1;${m.package_length};${m.image_length};${m.product};${m.board};${m.soc};${m.firmware};${sha};${m.build_id}`:'1;EMPTY;0;-;-;-;-;-;-;-;-';
      this.line(`AT$PD=BLEGW$GSOTAPKG;${fields};${token}`);
    } else if(cmd==='BLEGW$DSOTAPKG') {
      if(!this.options.deleteBusy)this.state='EMPTY';
      this.line(`AT$PD=BLEGW$DSOTAPKG;${this.options.deleteBusy?'0;BUSY':'1;OK'};${token}`);
    } else throw Error('unexpected ASCII '+cmd);
  }
  respond(p,body){this.client.decoder.feed(S.frame(S.packet((p[0]&7)+1,p[4]*256+p[5],p[7],p[6],body)));}
  handle(p) {
    const body=S.uncbor(p.slice(8)),key=`${p[0]&7}/${p[4]*256+p[5]}/${p[7]}`;
    this.requests.push({key,body});
    if(key==='0/1/0')return this.respond(p,{images:[{slot:0,version:'0.8.8.0',hash:new Uint8Array(32),confirmed:true}]});
    assert.equal(key,'2/1/1','sensor storage must never send Test/reset');
    assert.equal(body.image,1,'every chunk identifies image 1');assert.equal(body.upgrade,undefined);
    assert.ok(body.data.length<=600);assert.ok(p.length<=1024);
    if(this.options.rc)return this.respond(p,{rc:this.options.rc});
    if(body.off===0) {
      assert.equal(body.len,this.file.bytes.length);assert.ok(S.equal(body.sha,this.file.sha));
      assert.notEqual(Buffer.from(body.sha).toString('hex').toUpperCase(),this.file.metadata.upload_sha256,'whole package and inner image hashes differ');
      if(this.off)return this.respond(p,{off:this.off});
    }
    assert.equal(body.off,this.off);assert.ok(S.equal(body.data,this.file.bytes.slice(body.off,body.off+body.data.length)));
    this.off+=body.data.length;if(this.off===this.file.bytes.length)this.state='VALID';
    this.respond(p,{off:this.off});
  }
}
(async()=>{
  let file=await fixture();
  if(process.argv[2]) {
    file=await require('../sensor_ota_package.js').buildFromZip(new Uint8Array(fs.readFileSync(process.argv[2])));
    file.sha=new Uint8Array(await webcrypto.subtle.digest('SHA-256',file.bytes));
  }
  for(const options of [{},{resume:4096}]) {
    const d=new Device(file,options),out=await d.client.update(file,1);
    assert.equal(out.kind,'stored');assert.equal(out.stored.state,'VALID');assert.equal(d.client.testSent,false);
    assert.deepEqual(d.opens,[9600,115200,9600]);
    assert.equal((await d.client.sensorPackage()).state,'VALID','new query reads persisted state');
    assert.equal((await d.client.deleteSensorPackage()).state,'EMPTY');
  }
  for(const options of [{badSha:true},{unsupported:true},{malformed:true},{rc:10},{rc:9},{project:'OTHER'},{cancel:true}]) {
    const d=new Device(file,options);await assert.rejects(d.client.update(file,1));
    assert.equal(d.client.testSent,false);assert.ok(!d.requests.some(r=>r.key==='2/0/5'||r.key==='2/1/0'));
  }
  const busy=new Device(file,{deleteBusy:true});await assert.rejects(busy.client.deleteSensorPackage(),/BUSY/);
  const large=new Device(file);await assert.rejects(large.client.update({...file,bytes:new Uint8Array(225281)},1),/225,280/);
  assert.equal(large.commands.length,0,'oversize rejected before device access');
  console.log('PASS Sensor SMP image 1: full upload/resume, package SHA vs inner SHA, VALID/read/delete EMPTY, wrong SHA/unsupported/malformed/busy/corrupt/target/cancel/oversize, no Test/reset');
})().catch(e=>{console.error(e);process.exitCode=1;});
