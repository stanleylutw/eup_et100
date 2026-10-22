// 測試用 Sensor image，僅有合法格式與 hash，並非經私鑰簽章的量產映像。
const {webcrypto}=require('node:crypto');
const zip=require('../vendor/fflate.min.js');
const relay=require('../sensor_ota_package.js');
module.exports=async function sensorFixture(payloadLength=8192) {
  const image=new Uint8Array(512+payloadLength+40), v=new DataView(image.buffer);
  v.setUint32(0,0x96f3b83d,true);v.setUint16(8,512,true);v.setUint32(12,payloadLength,true);
  image[20]=3;image[21]=6;v.setUint16(22,14,true);
  const at=544, text=new TextEncoder();
  const put=(offset,value)=>image.set(text.encode(value),at+offset);
  put(0,'EUPFWV1\0');v.setUint16(at+8,1,true);
  put(10,'EUP_AIoT_P1');put(34,'P1_HW_1.0.0');put(54,'3.6.14');
  put(86,'2026-10-23');put(106,'202610230001');
  v.setUint32(at+282,relay.crc32(image.slice(at,at+282)),true);
  const end=512+payloadLength;
  image.set([7,105,40,0,16,0,32,0],end);
  image.set(new Uint8Array(await webcrypto.subtle.digest('SHA-256',image.slice(0,end))),end+8);
  const manifest={'format-version':1,files:[{type:'application',file:'p1.signed.bin',board:'p1_nrf52832',soc:'nrf52832',size:image.length}]};
  const archive=zip.zipSync({'manifest.json':text.encode(JSON.stringify(manifest)),'p1.signed.bin':image});
  if(image.length>relay.IMAGE_MAX) return {zip:archive};
  const result=await relay.buildFromZip(archive);
  return {...result,zip:archive,sha:new Uint8Array(await webcrypto.subtle.digest('SHA-256',result.bytes))};
};
