"use strict";function _instanceof(left,right){if(right!=null&&typeof Symbol!=="undefined"&&right[Symbol.hasInstance]){return!!right[Symbol.hasInstance](left)}else{return left instanceof right}}function _classCallCheck(instance,Constructor){if(!_instanceof(instance,Constructor)){throw new TypeError("Cannot call a class as a function")}}function _defineProperties(target,props){for(var i=0;i<props.length;i++){var descriptor=props[i];descriptor.enumerable=descriptor.enumerable||false;descriptor.configurable=true;if("value"in descriptor)descriptor.writable=true;Object.defineProperty(target,descriptor.key,descriptor)}}function _createClass(Constructor,protoProps,staticProps){if(protoProps)_defineProperties(Constructor.prototype,protoProps);if(staticProps)_defineProperties(Constructor,staticProps);return Constructor}var UINT32_MAX=Math.pow(2,32)-1;var MP4=function(){function MP4(){_classCallCheck(this,MP4)}_createClass(MP4,null,[{key:"init",value:function init(){MP4.types={avc1:[],avcC:[],btrt:[],dinf:[],dref:[],esds:[],ftyp:[],hdlr:[],mdat:[],mdhd:[],mdia:[],mfhd:[],minf:[],moof:[],moov:[],mp4a:[],".mp3":[],mvex:[],mvhd:[],pasp:[],sdtp:[],stbl:[],stco:[],stsc:[],stsd:[],stsz:[],stts:[],tfdt:[],tfhd:[],traf:[],trak:[],trun:[],trex:[],tkhd:[],vmhd:[],smhd:[]};var i;for(i in MP4.types){if(MP4.types.hasOwnProperty(i)){MP4.types[i]=[i.charCodeAt(0),i.charCodeAt(1),i.charCodeAt(2),i.charCodeAt(3)]}}var videoHdlr=new Uint8Array([0,0,0,0,0,0,0,0,118,105,100,101,0,0,0,0,0,0,0,0,0,0,0,0,86,105,100,101,111,72,97,110,100,108,101,114,0]);var audioHdlr=new Uint8Array([0,0,0,0,0,0,0,0,115,111,117,110,0,0,0,0,0,0,0,0,0,0,0,0,83,111,117,110,100,72,97,110,100,108,101,114,0]);MP4.HDLR_TYPES={video:videoHdlr,audio:audioHdlr};var dref=new Uint8Array([0,0,0,0,0,0,0,1,0,0,0,12,117,114,108,32,0,0,0,1]);var stco=new Uint8Array([0,0,0,0,0,0,0,0]);MP4.STTS=MP4.STSC=MP4.STCO=stco;MP4.STSZ=new Uint8Array([0,0,0,0,0,0,0,0,0,0,0,0]);MP4.VMHD=new Uint8Array([0,0,0,1,0,0,0,0,0,0,0,0]);MP4.SMHD=new Uint8Array([0,0,0,0,0,0,0,0]);MP4.STSD=new Uint8Array([0,0,0,0,0,0,0,1]);var majorBrand=new Uint8Array([105,115,111,109]);var avc1Brand=new Uint8Array([97,118,99,49]);var minorVersion=new Uint8Array([0,0,0,1]);MP4.FTYP=MP4.box(MP4.types.ftyp,majorBrand,minorVersion,majorBrand,avc1Brand);MP4.DINF=MP4.box(MP4.types.dinf,MP4.box(MP4.types.dref,dref))}},{key:"box",value:function box(type){var payload=Array.prototype.slice.call(arguments,1),size=8,i=payload.length,len=i,result;while(i--){size+=payload[i].byteLength}result=new Uint8Array(size);result[0]=size>>24&255;result[1]=size>>16&255;result[2]=size>>8&255;result[3]=size&255;result.set(type,4);for(i=0,size=8;i<len;i++){result.set(payload[i],size);size+=payload[i].byteLength}return result}},{key:"hdlr",value:function hdlr(type){return MP4.box(MP4.types.hdlr,MP4.HDLR_TYPES[type])}},{key:"mdat",value:function mdat(data){return MP4.box(MP4.types.mdat,data)}},{key:"mdhd",value:function mdhd(timescale,duration){duration*=timescale;var upperWordDuration=Math.floor(duration/(UINT32_MAX+1));var lowerWordDuration=Math.floor(duration%(UINT32_MAX+1));return MP4.box(MP4.types.mdhd,new Uint8Array([1,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,3,timescale>>24&255,timescale>>16&255,timescale>>8&255,timescale&255,upperWordDuration>>24,upperWordDuration>>16&255,upperWordDuration>>8&255,upperWordDuration&255,lowerWordDuration>>24,lowerWordDuration>>16&255,lowerWordDuration>>8&255,lowerWordDuration&255,85,196,0,0]))}},{key:"mdia",value:function mdia(track){return MP4.box(MP4.types.mdia,MP4.mdhd(track.timescale,track.duration),MP4.hdlr(track.type),MP4.minf(track))}},{key:"mfhd",value:function mfhd(sequenceNumber){return MP4.box(MP4.types.mfhd,new Uint8Array([0,0,0,0,sequenceNumber>>24,sequenceNumber>>16&255,sequenceNumber>>8&255,sequenceNumber&255]))}},{key:"minf",value:function minf(track){if(track.type==="audio"){return MP4.box(MP4.types.minf,MP4.box(MP4.types.smhd,MP4.SMHD),MP4.DINF,MP4.stbl(track))}else{return MP4.box(MP4.types.minf,MP4.box(MP4.types.vmhd,MP4.VMHD),MP4.DINF,MP4.stbl(track))}}},{key:"moof",value:function moof(sn,baseMediaDecodeTime,track){return MP4.box(MP4.types.moof,MP4.mfhd(sn),MP4.traf(track,baseMediaDecodeTime))}},{key:"moov",value:function moov(tracks){var i=tracks.length,boxes=[];while(i--){boxes[i]=MP4.trak(tracks[i])}return MP4.box.apply(null,[MP4.types.moov,MP4.mvhd(tracks[0].timescale,tracks[0].duration)].concat(boxes).concat(MP4.mvex(tracks)))}},{key:"mvex",value:function mvex(tracks){var i=tracks.length,boxes=[];while(i--){boxes[i]=MP4.trex(tracks[i])}return MP4.box.apply(null,[MP4.types.mvex].concat(boxes))}},{key:"mvhd",value:function mvhd(timescale,duration){duration*=timescale;var upperWordDuration=Math.floor(duration/(UINT32_MAX+1));var lowerWordDuration=Math.floor(duration%(UINT32_MAX+1));var bytes=new Uint8Array([1,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,3,timescale>>24&255,timescale>>16&255,timescale>>8&255,timescale&255,upperWordDuration>>24,upperWordDuration>>16&255,upperWordDuration>>8&255,upperWordDuration&255,lowerWordDuration>>24,lowerWordDuration>>16&255,lowerWordDuration>>8&255,lowerWordDuration&255,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255]);return MP4.box(MP4.types.mvhd,bytes)}},{key:"sdtp",value:function sdtp(track){var samples=track.samples||[],bytes=new Uint8Array(4+samples.length),flags,i;for(i=0;i<samples.length;i++){flags=samples[i].flags;bytes[i+4]=flags.dependsOn<<4|flags.isDependedOn<<2|flags.hasRedundancy}return MP4.box(MP4.types.sdtp,bytes)}},{key:"stbl",value:function stbl(track){return MP4.box(MP4.types.stbl,MP4.stsd(track),MP4.box(MP4.types.stts,MP4.STTS),MP4.box(MP4.types.stsc,MP4.STSC),MP4.box(MP4.types.stsz,MP4.STSZ),MP4.box(MP4.types.stco,MP4.STCO))}},{key:"avc1",value:function avc1(track){var sps=[],pps=[],i,data,len;for(i=0;i<track.sps.length;i++){data=track.sps[i];len=data.byteLength;sps.push(len>>>8&255);sps.push(len&255);sps=sps.concat(Array.prototype.slice.call(data))}for(i=0;i<track.pps.length;i++){data=track.pps[i];len=data.byteLength;pps.push(len>>>8&255);pps.push(len&255);pps=pps.concat(Array.prototype.slice.call(data))}var avcc=MP4.box(MP4.types.avcC,new Uint8Array([1,sps[3],sps[4],sps[5],252|3,224|track.sps.length].concat(sps).concat([track.pps.length]).concat(pps))),width=track.width,height=track.height,hSpacing=track.pixelRatio[0],vSpacing=track.pixelRatio[1];return MP4.box(MP4.types.avc1,new Uint8Array([0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,width>>8&255,width&255,height>>8&255,height&255,0,72,0,0,0,72,0,0,0,0,0,0,0,1,18,100,97,105,108,121,109,111,116,105,111,110,47,104,108,115,46,106,115,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,17,17]),avcc,MP4.box(MP4.types.btrt,new Uint8Array([0,28,156,128,0,45,198,192,0,45,198,192])),MP4.box(MP4.types.pasp,new Uint8Array([hSpacing>>24,hSpacing>>16&255,hSpacing>>8&255,hSpacing&255,vSpacing>>24,vSpacing>>16&255,vSpacing>>8&255,vSpacing&255])))}},{key:"esds",value:function esds(track){var configlen=track.config.length;return new Uint8Array([0,0,0,0,3,23+configlen,0,1,0,4,15+configlen,64,21,0,0,0,0,0,0,0,0,0,0,0,5].concat([configlen]).concat(track.config).concat([6,1,2]))}},{key:"mp4a",value:function mp4a(track){var samplerate=track.samplerate;return MP4.box(MP4.types.mp4a,new Uint8Array([0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,track.channelCount,0,16,0,0,0,0,samplerate>>8&255,samplerate&255,0,0]),MP4.box(MP4.types.esds,MP4.esds(track)))}},{key:"mp3",value:function mp3(track){var samplerate=track.samplerate;return MP4.box(MP4.types[".mp3"],new Uint8Array([0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,track.channelCount,0,16,0,0,0,0,samplerate>>8&255,samplerate&255,0,0]))}},{key:"stsd",value:function stsd(track){if(track.type==="audio"){if(!track.isAAC&&track.codec==="mp3"){return MP4.box(MP4.types.stsd,MP4.STSD,MP4.mp3(track))}return MP4.box(MP4.types.stsd,MP4.STSD,MP4.mp4a(track))}else{return MP4.box(MP4.types.stsd,MP4.STSD,MP4.avc1(track))}}},{key:"tkhd",value:function tkhd(track){var id=track.id,duration=track.duration*track.timescale,width=track.width,height=track.height,upperWordDuration=Math.floor(duration/(UINT32_MAX+1)),lowerWordDuration=Math.floor(duration%(UINT32_MAX+1));return MP4.box(MP4.types.tkhd,new Uint8Array([1,0,0,7,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,3,id>>24&255,id>>16&255,id>>8&255,id&255,0,0,0,0,upperWordDuration>>24,upperWordDuration>>16&255,upperWordDuration>>8&255,upperWordDuration&255,lowerWordDuration>>24,lowerWordDuration>>16&255,lowerWordDuration>>8&255,lowerWordDuration&255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,0,0,width>>8&255,width&255,0,0,height>>8&255,height&255,0,0]))}},{key:"traf",value:function traf(track,baseMediaDecodeTime){var sampleDependencyTable=MP4.sdtp(track),id=track.id,upperWordBaseMediaDecodeTime=Math.floor(baseMediaDecodeTime/(UINT32_MAX+1)),lowerWordBaseMediaDecodeTime=Math.floor(baseMediaDecodeTime%(UINT32_MAX+1));return MP4.box(MP4.types.traf,MP4.box(MP4.types.tfhd,new Uint8Array([0,0,0,0,id>>24,id>>16&255,id>>8&255,id&255])),MP4.box(MP4.types.tfdt,new Uint8Array([1,0,0,0,upperWordBaseMediaDecodeTime>>24,upperWordBaseMediaDecodeTime>>16&255,upperWordBaseMediaDecodeTime>>8&255,upperWordBaseMediaDecodeTime&255,lowerWordBaseMediaDecodeTime>>24,lowerWordBaseMediaDecodeTime>>16&255,lowerWordBaseMediaDecodeTime>>8&255,lowerWordBaseMediaDecodeTime&255])),MP4.trun(track,sampleDependencyTable.length+16+20+8+16+8+8),sampleDependencyTable)}},{key:"trak",value:function trak(track){track.duration=track.duration||4294967295;return MP4.box(MP4.types.trak,MP4.tkhd(track),MP4.mdia(track))}},{key:"trex",value:function trex(track){var id=track.id;return MP4.box(MP4.types.trex,new Uint8Array([0,0,0,0,id>>24,id>>16&255,id>>8&255,id&255,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,1]))}},{key:"trun",value:function trun(track,offset){var samples=track.samples||[],len=samples.length,arraylen=12+16*len,array=new Uint8Array(arraylen),i,sample,duration,size,flags,cts;offset+=8+arraylen;array.set([0,0,15,1,len>>>24&255,len>>>16&255,len>>>8&255,len&255,offset>>>24&255,offset>>>16&255,offset>>>8&255,offset&255],0);for(i=0;i<len;i++){sample=samples[i];duration=sample.duration;size=sample.size;flags=sample.flags;cts=sample.cts;array.set([duration>>>24&255,duration>>>16&255,duration>>>8&255,duration&255,size>>>24&255,size>>>16&255,size>>>8&255,size&255,flags.isLeading<<2|flags.dependsOn,flags.isDependedOn<<6|flags.hasRedundancy<<4|flags.paddingValue<<1|flags.isNonSync,flags.degradPrio&240<<8,flags.degradPrio&15,cts>>>24&255,cts>>>16&255,cts>>>8&255,cts&255],12+16*i)}return MP4.box(MP4.types.trun,array)}},{key:"initSegment",value:function initSegment(tracks){if(!MP4.types){MP4.init()}var movie=MP4.moov(tracks),result;result=new Uint8Array(MP4.FTYP.byteLength+movie.byteLength);result.set(MP4.FTYP);result.set(movie,MP4.FTYP.byteLength);return result}}]);return MP4}();