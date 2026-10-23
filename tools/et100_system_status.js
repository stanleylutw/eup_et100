/* SYS protocol codec shared by the offline HTML and Node regression tests. */
(function(root,factory) {
  const api=factory();
  if(typeof module==='object' && module.exports) module.exports=api;
  else root.ET100System=api;
})(typeof globalThis!=='undefined'?globalThis:this, function() {
  'use strict';
  const registry=[
  {
    "index": 1,
    "name": "SYSTEM",
    "statuses": {
      "1": "STARTING",
      "2": "RUNNING",
      "3": "SHUTTING_DOWN"
    }
  },
  {
    "index": 2,
    "name": "SETTINGS",
    "statuses": {
      "1": "INITIALIZING",
      "2": "READY",
      "3": "READ_ERROR",
      "4": "INTEGRITY_ERROR"
    }
  },
  {
    "index": 3,
    "name": "NOR",
    "statuses": {
      "1": "INITIALIZING",
      "2": "READY",
      "3": "ID_MISMATCH",
      "4": "TIMEOUT",
      "5": "IO_ERROR"
    }
  },
  {
    "index": 4,
    "name": "BLE",
    "statuses": {
      "1": "INITIALIZING",
      "2": "IDLE",
      "3": "SCANNING",
      "4": "ADVERTISING",
      "5": "CONNECTED",
      "6": "MULTI_ROLE"
    }
  },
  {
    "index": 5,
    "name": "MODEM",
    "statuses": {
      "1": "POWERING",
      "2": "WAIT_AT",
      "3": "AT_READY",
      "4": "INITIALIZING",
      "5": "ONLINE",
      "6": "RECOVERING",
      "7": "TIMEOUT"
    }
  },
  {
    "index": 6,
    "name": "SIM",
    "statuses": {
      "1": "CHECKING",
      "2": "ABSENT",
      "3": "READY",
      "4": "PIN_REQUIRED",
      "5": "SIM_ERROR"
    }
  },
  {
    "index": 7,
    "name": "NETWORK",
    "statuses": {
      "1": "SEARCHING",
      "2": "REGISTERING",
      "3": "REGISTERED",
      "4": "DATA_CONNECTING",
      "5": "DATA_CONNECTED",
      "6": "DENIED",
      "7": "NO_SERVICE"
    }
  },
  {
    "index": 8,
    "name": "GNSS",
    "statuses": {
      "1": "WAIT_DATA",
      "2": "ACQUIRING",
      "3": "FIX_VALID",
      "4": "FIX_LOST",
      "5": "DATA_TIMEOUT",
      "6": "INVALID_DATA"
    }
  },
  {
    "index": 9,
    "name": "IMU",
    "statuses": {
      "1": "INITIALIZING",
      "2": "READY",
      "3": "SAMPLING",
      "4": "ID_MISMATCH",
      "5": "TIMEOUT",
      "6": "DATA_ERROR"
    }
  },
  {
    "index": 10,
    "name": "NFC",
    "statuses": {
      "1": "INITIALIZING",
      "2": "WAIT_CARD",
      "3": "CARD_PRESENT",
      "4": "TIMEOUT",
      "5": "TRANSACTING",
      "6": "IO_ERROR"
    }
  },
  {
    "index": 11,
    "name": "IOEXP",
    "statuses": {
      "1": "INITIALIZING",
      "2": "READY",
      "3": "ID_MISMATCH",
      "4": "TIMEOUT",
      "5": "CONFIG_ERROR"
    }
  },
  {
    "index": 12,
    "name": "CAN",
    "statuses": {
      "1": "INITIALIZING",
      "2": "LISTENING",
      "3": "RECEIVING",
      "4": "BUS_OFF",
      "5": "RECOVERING",
      "6": "INIT_ERROR"
    }
  },
  {
    "index": 13,
    "name": "RS232_0",
    "statuses": {
      "1": "INITIALIZING",
      "2": "OPEN",
      "3": "RECEIVING",
      "4": "TRANSMITTING",
      "5": "DUPLEX",
      "6": "IO_ERROR"
    }
  },
  {
    "index": 14,
    "name": "RS232_1",
    "statuses": {
      "1": "INITIALIZING",
      "2": "OPEN",
      "3": "RECEIVING",
      "4": "TRANSMITTING",
      "5": "DUPLEX",
      "6": "IO_ERROR"
    }
  },
  {
    "index": 15,
    "name": "RS232_2",
    "statuses": {
      "1": "INITIALIZING",
      "2": "OPEN",
      "3": "RECEIVING",
      "4": "TRANSMITTING",
      "5": "DUPLEX",
      "6": "IO_ERROR"
    }
  },
  {
    "index": 16,
    "name": "ACC",
    "statuses": {
      "1": "INITIALIZING",
      "2": "INACTIVE",
      "3": "ACTIVE",
      "4": "READ_ERROR"
    }
  },
  {
    "index": 17,
    "name": "DIGITAL_IN",
    "statuses": {
      "1": "INITIALIZING",
      "2": "READY",
      "3": "READ_ERROR"
    }
  },
  {
    "index": 18,
    "name": "DIGITAL_OUT",
    "statuses": {
      "1": "INITIALIZING",
      "2": "COMMANDED_OFF",
      "3": "COMMANDED_ON",
      "4": "DRIVER_ERROR"
    }
  },
  {
    "index": 19,
    "name": "ACC_OUT",
    "statuses": {
      "1": "INITIALIZING",
      "2": "COMMANDED_OFF",
      "3": "COMMANDED_ON",
      "4": "DRIVER_ERROR"
    }
  },
  {
    "index": 20,
    "name": "ANALOG_IN",
    "statuses": {
      "1": "INITIALIZING",
      "2": "VALID",
      "3": "OUT_OF_RANGE",
      "4": "READ_ERROR"
    }
  },
  {
    "index": 21,
    "name": "BATTERY",
    "statuses": {
      "1": "CHECKING",
      "2": "NORMAL",
      "3": "LOW",
      "4": "ABSENT",
      "5": "READ_ERROR"
    }
  },
  {
    "index": 22,
    "name": "BATTERY_TEMP",
    "statuses": {
      "1": "CHECKING",
      "2": "NORMAL",
      "3": "TOO_COLD",
      "4": "TOO_HOT",
      "5": "SENSOR_ERROR"
    }
  },
  {
    "index": 23,
    "name": "ONEWIRE",
    "statuses": {
      "1": "DISCOVERING",
      "2": "NO_PROBE",
      "3": "READY",
      "4": "SAMPLING",
      "5": "CRC_ERROR",
      "6": "TIMEOUT"
    }
  },
  {
    "index": 24,
    "name": "LED",
    "statuses": {
      "1": "INITIALIZING",
      "2": "COMMANDED_OFF",
      "3": "COMMANDED_ON",
      "4": "PATTERN_ACTIVE",
      "5": "DRIVER_ERROR"
    }
  },
  {
    "index": 25,
    "name": "BUZZER",
    "statuses": {
      "1": "INITIALIZING",
      "2": "COMMANDED_OFF",
      "3": "COMMANDED_ON",
      "4": "PATTERN_ACTIVE",
      "5": "DRIVER_ERROR"
    }
  },
  {
    "index": 26,
    "name": "CHARGER",
    "statuses": {
      "1": "CHECKING",
      "2": "IDLE",
      "3": "CHARGING",
      "4": "FULL",
      "5": "FAULT"
    }
  },
  {
    "index": 27,
    "name": "POWER",
    "statuses": {
      "1": "CHECKING",
      "2": "EXTERNAL",
      "3": "BACKUP",
      "4": "TRANSITION",
      "5": "LOW_VOLTAGE"
    }
  },
  {
    "index": 28,
    "name": "TIME",
    "statuses": {
      "1": "UNSYNCED",
      "2": "SYNCING",
      "3": "VALID",
      "4": "CLOCK_ERROR"
    }
  },
  {
    "index": 29,
    "name": "WATCHDOG",
    "statuses": {
      "1": "ARMING",
      "2": "ARMED",
      "3": "REFRESHING",
      "4": "CONFIG_ERROR"
    }
  },
  {
    "index": 30,
    "name": "RESET",
    "statuses": {
      "1": "POWER_ON",
      "2": "SOFTWARE",
      "3": "WATCHDOG",
      "4": "EXTERNAL",
      "5": "BROWNOUT",
      "6": "OTHER"
    }
  },
  {
    "index": 31,
    "name": "LTE_RF",
    "statuses": {
      "1": "CHECKING",
      "2": "DIAG_PASSED",
      "3": "DIAG_FAILED"
    }
  },
  {
    "index": 32,
    "name": "BLE_RF",
    "statuses": {
      "1": "CHECKING",
      "2": "DIAG_PASSED",
      "3": "DIAG_FAILED"
    }
  },
  {
    "index": 33,
    "name": "NFC_RF",
    "statuses": {
      "1": "CHECKING",
      "2": "DIAG_PASSED",
      "3": "DIAG_FAILED"
    }
  }
];
  // Board-design labels only; runtime availability still comes from Status.
  const displayNames=[
    '主系統 SYSTEM', '設定儲存 SETTINGS', '外部儲存 NOR Flash',
    '藍牙 FR3068 BLE', '行動通訊 EG800Q-EU', '用戶識別卡 SIM',
    '行動網路 NETWORK', '衛星定位 AG3352Q', '六軸感測器 SC7U22',
    '讀卡器 PN7160', '輸入輸出擴充 AW9523B', '車輛通訊 CAN',
    '串列埠 RS232_0', '串列埠 RS232_1', '串列埠 RS232_2',
    '點火輸入 ACC', '數位輸入 DIGITAL_IN', '數位輸出 DIGITAL_OUT',
    '點火輸出 ACC_OUT', '類比輸入 ANALOG_IN', '備援電池 BATTERY',
    '電池溫度 BATTERY_TEMP', '單線探頭 ONEWIRE', '指示燈 LED',
    '蜂鳴器 BUZZER', '充電管理 CHARGER', '供電來源 POWER',
    '系統時間 TIME', '看門狗 WATCHDOG', '重啟原因 RESET',
    '行動通訊射頻 LTE_RF', '藍牙射頻 BLE_RF', '讀卡器射頻 NFC_RF'
  ];
  registry.forEach((item,i)=>{item.label=displayNames[i];});
  const common={0:'UNKNOWN',240:'DISABLED',241:'NOT_FITTED',242:'UNSUPPORTED',243:'STALE',244:'ERROR'};
  const types={0:'BOOT_UNKNOWN',1:'POWER_ON',2:'OTA_TRIAL',3:'OTA_ROLLBACK',
    4:'SOFTWARE_RESET',5:'WATCHDOG_RESET',6:'EXTERNAL_RESET',7:'BROWNOUT_RESET',
    10:'PERIPHERAL_STATUS',11:'PERIPHERAL_CHANGED'};
  function crc(text) {
    let c=0;
    for(const ch of text) {
      c^=ch.charCodeAt(0)<<8;
      for(let i=0;i<8;i++)c=((c<<1)^((c&0x8000)?0x1021:0))&0xffff;
    }
    return c.toString(16).toUpperCase().padStart(4,'0');
  }
  function encode(body) {return body+'*'+crc(body)+'\r\n';}
  function parse(raw) {
    if(typeof raw!=='string')return null;
    const line=raw.endsWith('\r\n')?raw.slice(0,-2):raw;
    if(line.length+2>320 || !/^[\x21-\x7E]+\*[0-9A-F]{4}$/.test(line))return null;
    const star=line.length-5,body=line.slice(0,star);
    if(body.includes('*') || crc(body)!==line.slice(star+1))return null;
    let fields=body.split(';'),token=null,type;
    if(fields[0]==='AT$PD=BLEGW$GSYSSTAT') {
      if(!/^[A-Z0-9]{4}$/.test(fields[2] || ''))return null;
      token=fields[2];
      if(fields[1]==='0') {
        if(fields.length!==4 || !/^[1-4]$/.test(fields[3]))return null;
        return {error:Number(fields[3]),token};
      }
      if(fields[1]!=='1' || fields[3]!=='10')return null;
      type=10;fields=fields.slice(4);
    } else if(fields[0]==='ET100$SYS') {
      if(!/^(?:[0-7]|10|11)$/.test(fields[1] || ''))return null;
      type=Number(fields[1]);fields=fields.slice(2);
    } else return null;
    if(type<8)return fields.length ? null : {type,pairs:[],token};
    if(type===10 && fields.length!==33)return null;
    if(type===11 && (!fields.length || fields.length>33))return null;
    let previous=0;
    const pairs=[];
    for(const field of fields) {
      const match=/^(0[1-9]|[12][0-9]|3[0-3]),(0|[1-9][0-9]{0,2})$/.exec(field);
      if(!match)return null;
      const index=Number(match[1]),status=Number(match[2]);
      if(index<=previous || status>255 || (type===10 && index!==previous+1))return null;
      pairs.push({index,status});previous=index;
    }
    return {type,pairs,token};
  }
  function describe(index,status) {
    const item=registry[index-1];
    if(!item)return {name:'UNKNOWN_INDEX',status:'UNKNOWN_CODE',kind:'unknown'};
    const text=common[status] || item.statuses[status] || 'UNKNOWN_CODE';
    let kind='ready';
    if(['UNKNOWN','UNSUPPORTED','NOT_FITTED','DISABLED','STALE','UNKNOWN_CODE'].includes(text))kind='unknown';
    else if(/ERROR|MISMATCH|TIMEOUT|FAILED|FAULT|DENIED/.test(text))kind='error';
    else if(/LOW|TOO_COLD|TOO_HOT|OUT_OF_RANGE|ABSENT|NO_SERVICE|NO_PROBE|FIX_LOST|PIN_REQUIRED/.test(text))kind='warning';
    else if(/INITIALIZING|STARTING|POWERING|WAIT_AT|CHECKING|SEARCHING|REGISTERING|CONNECTING|WAIT_DATA|ACQUIRING|DISCOVERING|SYNCING|UNSYNCED|ARMING|RECOVERING|SHUTTING/.test(text))kind='waiting';
    return {name:item.name,status:text,kind};
  }
  class State {
    constructor(){this.reset();}
    reset(){this.values=Array(33).fill(null);this.complete=false;this.boot=null;}
    apply(message) {
      if(message.error)return;
      if(message.type<8){this.reset();this.boot=types[message.type];return;}
      if(message.type===10)this.complete=true;
      for(const pair of message.pairs)this.values[pair.index-1]=pair.status;
    }
  }
  return {registry,common,types,crc,encode,parse,describe,State};
});
