// Browser en-US integer-money profile, independent of upstream translation language.
import fs from 'node:fs';
if (!process.versions.node.startsWith('22.')) throw Error('Node22 required');
const N=100000,seed=Number(process.env.MONEY_NUMBER_SEED??777),b=new ArrayBuffer(8),v=new DataView(b);
let state=seed>>>0;const random=()=>state=(Math.imul(state,1664525)+1013904223)>>>0;
const special=[0,-0,1,-1,9,10,999,1000,2147483647,2147483648,9007199254740991,9007199254740992,1e18+128,1e21,1e23,Number.MAX_VALUE,Infinity,-Infinity,NaN];
const fd=fs.openSync(process.env.MONEY_NUMBER_OUT??'/tmp/money-number.txt','w');
fs.writeSync(fd,`MONEY_NUMBER 1 ${N} ${seed}\n`);
for(let i=0;i<N;i++){
 let value;
 if(i<special.length)value=special[i];
 else if(i<special.length+1024){const e=(i-special.length)%1024;value=2**e;}
 else {v.setUint32(0,((1023+(random()%1024))<<20)|(random()&0xfffff));v.setUint32(4,random());value=Math.floor(v.getFloat64(0));if(i%2)value=-value;}
 v.setFloat64(0,value);const bits=v.getBigUint64(0).toString(16).padStart(16,'0');
 fs.writeSync(fd,`C ${i} ${bits} ${Buffer.from(value.toLocaleString('en-US')).toString('hex')}\n`);
}
fs.writeSync(fd,`MONEY_NUMBER_COMPLETE ${N}\n`);fs.closeSync(fd);
console.log(`MONEY_NUMBER ${N} native integer/non-finite cases seed${seed}`);
