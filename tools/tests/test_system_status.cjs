const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const sys=require('../et100_system_status.js');
const {execFileSync}=require('node:child_process');
execFileSync('bash',['fw/tools/test_system_status_host.sh'],{cwd:path.resolve(__dirname,'../..')});
const cLine=execFileSync('/tmp/et100_system_status_test',['--vector'],{encoding:'utf8'});
const cMessage=sys.parse(cLine);
assert.equal(cMessage.token,'C001');assert.equal(cMessage.pairs.length,33);
assert.equal(cMessage.pairs[2].status,2);
const md=fs.readFileSync(path.join(__dirname,'../../fw/docs/02_protocol/08_system_status.md'),'utf8');
assert.equal(sys.registry.length,33);
assert.deepEqual(sys.registry.map(r=>r.index),Array.from({length:33},(_,i)=>i+1));
assert.equal(sys.crc('123456789'),'31C3');
const vectors=md.split('\n').filter(l=>/^(?:ET100\$SYS|AT\$PD=BLEGW\$GSYSSTAT);.*\*[0-9A-F]{4}$/.test(l));
assert.equal(vectors.length,5);
for(const line of vectors)assert.ok(sys.parse(line),line);
const full=Array.from({length:33},(_,i)=>`${String(i+1).padStart(2,'0')},242`).join(';');
const message=body=>sys.parse(sys.encode(body));
assert.equal(message('ET100$SYS;10;'+full).pairs.length,33);
assert.equal(message('ET100$SYS;11;04,3').pairs[0].status,3);
for(const item of sys.registry) {
  assert.match(item.label,/^[\u4e00-\u9fff]+ [A-Za-z0-9_ -]+$/);
  for(const [code,name] of Object.entries(item.statuses))assert.equal(sys.describe(item.index,Number(code)).status,name);
  for(const [code,name] of Object.entries(sys.common))assert.equal(sys.describe(item.index,Number(code)).status,name);
}
assert.equal(sys.registry[4].label,'行動通訊 EG800Q-EU');
assert.equal(sys.registry[7].label,'衛星定位 AG3352Q');
assert.equal(sys.registry[4].name,'MODEM','UI labels must not replace protocol identifiers');
assert.equal(sys.describe(8,2).status,'ACQUIRING');
assert.equal(sys.describe(3,2).status,'READY');
assert.equal(sys.describe(3,99).status,'UNKNOWN_CODE');
assert.equal(sys.describe(3,99).kind,'unknown');
assert.equal(sys.describe(10,4).kind,'error');
for(const body of [
  'ET100$SYS;10;03,2','ET100$SYS;11','ET100$SYS;11;03,2;03,4',
  'ET100$SYS;11;04,2;03,2','ET100$SYS;11;3,2','ET100$SYS;11;34,2',
  'ET100$SYS;11;00,2','ET100$SYS;11;03,256','ET100$SYS;11;03,02',
  'ET100$SYS;1;03,2','ET100$SYS;12;03,2','ET100$SYS;01',
  'AT$PD=BLEGW$GSYSSTAT;1;bad!;10;'+full,
  'AT$PD=BLEGW$GSYSSTAT;0;A001;0',
  'ET100$SYS;11;03,2;'+ 'A'.repeat(320)
]) assert.equal(message(body),null,body);
const line=sys.encode('ET100$SYS;11;03,2');
assert.equal(sys.parse(line.replace('03,2','03,3')),null);
assert.equal(sys.parse('noise'+line),null);
assert.equal(message('ET100$SYS;11;03,99').pairs[0].status,99);
const state=new sys.State();state.apply(message('ET100$SYS;10;'+full));
assert.equal(state.complete,true);state.apply(message('ET100$SYS;11;03,2'));
assert.equal(state.values[2],2);assert.equal(state.values[3],242);
state.apply(message('ET100$SYS;2'));assert.equal(state.boot,'OTA_TRIAL');
assert.equal(state.complete,false);assert.ok(state.values.every(v=>v===null));
console.log('PASS SYS JS: real C formatter interop, protocol vectors/CRC, 33 tables, full/delta, errors, unknown codes, bounds and boot reset');
