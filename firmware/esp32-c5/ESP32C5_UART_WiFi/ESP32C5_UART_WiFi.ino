/* ND Pico2DMX ESP32-C5 production Wi-Fi gateway.
   V3.4A uses SPI2 slave for all RP2350 inter-processor traffic. */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_wifi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <driver/spi_slave.h>
#include "esp_attr.h"
#include "../ND_SPI_Protocol.h"

#ifndef ND_SPI_FORENSIC_DIAGNOSTICS
#define ND_SPI_FORENSIC_DIAGNOSTICS 0
#endif
#define ND_ESP_FW_VERSION "V3.6A-C5"
static constexpr spi_host_device_t SPI_HOST = SPI2_HOST;
static constexpr int C5_SCK=25, C5_MOSI=26, C5_MISO=8, C5_CS=9, C5_READY=10;
static constexpr int WIFI_AP_CHANNEL_5G=36;
static constexpr int WIFI_AP_CHANNEL_2G=6;
static constexpr size_t LINE=768, REQ=512, RESP=16384, SPI_QUEUE_DEPTH=16;
WiFiServer server(80); Preferences nvs;
WiFiUDP artWifi, sacnUnicastWifi, sacnMulticastWifi[4];
const char* apName="ND-DMX-C5"; const char* apPass="nddmx1234";
const IPAddress apIp(192,168,4,1), apMask(255,255,255,0);
char request[REQ], mode[4]="AP", band[4]="5G", bandFallback[4]="", ssid[65]="", pass[65]="";
uint8_t wifiPacket[768];
uint8_t response[RESP]; size_t responseLen=0, expectedResponse=0; uint16_t responseId=0; volatile bool responseDone=false;
uint32_t staStart=0,lastHb=0,lastDiag=0; bool nvsReady=false,ap=false,sta=false,staConnectedReported=false;
uint32_t wifiArtRx=0,wifiArtDrops=0,wifiSacnRx=0,wifiSacnDrops=0;
uint32_t proxyRequestsStarted=0,proxyReqFramesSent=0,lastProxyRequestId=0; char lastProxyPath[96]="";
uint32_t spiTransactions=0,spiValidFrames=0,spiCrcErrors=0,spiMagicErrors=0,spiVersionErrors=0,spiLengthErrors=0,spiSequenceErrors=0,spiRxDrops=0,spiTxDrops=0,spiLightingDrops=0,spiLinkResyncs=0,spiReadyHighCount=0,spiReadyMismatchCount=0;
volatile bool spiSynced=false; volatile uint32_t spiLastSeen=0; uint32_t spiTxSequence=1; bool spiInitOk=false; esp_err_t spiInitError=ESP_OK;
static constexpr uint32_t SPI_LINK_TIMEOUT_MS=1500;
enum C5OtaState : uint8_t { C5_OTA_IDLE, C5_OTA_RECEIVING, C5_OTA_VERIFYING, C5_OTA_READY_TO_REBOOT, C5_OTA_REBOOTING, C5_OTA_SUCCESS, C5_OTA_FAILED };
static C5OtaState c5OtaState=C5_OTA_IDLE; static bool c5OtaBusy=false; static uint32_t c5OtaReceived=0,c5OtaTotal=0; static char c5OtaLastError[96]="";
static size_t httpContentLength=0; static bool httpUpdateConfirm=false; static char httpContentType[48]="";
static volatile uint16_t cancelledResponseId=0; static volatile bool cancelledResponsePending=false;
#if ND_SPI_FORENSIC_DIAGNOSTICS
static bool firstC5TransactionCaptured=false, firstC5InvalidCaptured=false;
#endif

struct PendingLighting { bool ready=false; char type[5]={0}; uint16_t universe=0; uint8_t priority=100; uint8_t source[4]={0}; uint16_t length=0; uint8_t data[512]; };
PendingLighting pendingLighting[4]; uint8_t nextLightingSlot=0; uint32_t lightingNextUs=0;

DRAM_DMA_ALIGNED_ATTR uint8_t spiRxFrame[ND_SPI::FRAME_BYTES], spiTxFrame[ND_SPI::FRAME_BYTES];
alignas(4) uint8_t spiQueue[SPI_QUEUE_DEPTH][ND_SPI::FRAME_BYTES]; uint8_t spiQueueHead=0,spiQueueTail=0;
portMUX_TYPE spiMux=portMUX_INITIALIZER_UNLOCKED;
char pendingSubscription[64]=""; volatile bool subscriptionPending=false;
char pendingWifiConfig[512]=""; volatile bool wifiConfigPending=false; uint16_t pendingWifiConfigId=0;
static bool queueSpiFrame(uint8_t type,uint32_t seq,uint16_t flags,uint32_t channel,const uint8_t*payload,uint16_t length,bool lighting=false){
  portENTER_CRITICAL(&spiMux); uint8_t n=uint8_t((spiQueueHead+1u)%SPI_QUEUE_DEPTH); if(n==spiQueueTail){portEXIT_CRITICAL(&spiMux);if(lighting)++spiLightingDrops;else++spiTxDrops;return false;} ND_SPI::build(spiQueue[spiQueueHead],type,seq,flags,channel,payload,length);spiQueueHead=n;portEXIT_CRITICAL(&spiMux);return true;
}
static bool popSpiFrame(uint8_t*out){portENTER_CRITICAL(&spiMux);if(spiQueueTail==spiQueueHead){portEXIT_CRITICAL(&spiMux);return false;}uint8_t selected=spiQueueTail;for(uint8_t p=spiQueueTail;p!=spiQueueHead;p=uint8_t((p+1u)%SPI_QUEUE_DEPTH)){if(ND_SPI::get16(spiQueue[p]+10)&ND_SPI::FLAG_HIGH_PRIORITY){selected=p;break;}}memcpy(out,spiQueue[selected],ND_SPI::FRAME_BYTES);if(selected!=spiQueueTail){uint8_t p=selected;while(p!=spiQueueTail){uint8_t prev=p==0?SPI_QUEUE_DEPTH-1:p-1;memcpy(spiQueue[p],spiQueue[prev],ND_SPI::FRAME_BYTES);p=prev;}}spiQueueTail=uint8_t((spiQueueTail+1u)%SPI_QUEUE_DEPTH);portEXIT_CRITICAL(&spiMux);return true;}
static void discardQueuedApiRequests(uint16_t requestId=0, bool all=false){
  portENTER_CRITICAL(&spiMux);
  uint8_t read=spiQueueTail, write=spiQueueTail;
  while(read!=spiQueueHead){
    uint8_t next=uint8_t((read+1u)%SPI_QUEUE_DEPTH);
    bool remove=ND_SPI::get16(spiQueue[read])==ND_SPI::MAGIC && spiQueue[read][3]==ND_SPI::SPI_MSG_API_REQUEST && (all || ND_SPI::get32(spiQueue[read]+12)==requestId);
    if(!remove){ if(write!=read) memcpy(spiQueue[write],spiQueue[read],ND_SPI::FRAME_BYTES); write=uint8_t((write+1u)%SPI_QUEUE_DEPTH); }
    read=next;
  }
  spiQueueHead=write;
  portEXIT_CRITICAL(&spiMux);
}
static bool queueLightingFrame(const PendingLighting&f){uint8_t p[517];p[0]=f.priority;memcpy(p+1,f.source,4);memcpy(p+5,f.data,f.length);return queueSpiFrame(!strcmp(f.type,"WART")?ND_SPI::SPI_MSG_ARTNET_UNIVERSE:ND_SPI::SPI_MSG_SACN_UNIVERSE,spiTxSequence++,ND_SPI::FLAG_FIRST|ND_SPI::FLAG_LAST|ND_SPI::FLAG_HIGH_PRIORITY,f.universe,p,uint16_t(5+f.length),true);}

#if ND_SPI_FORENSIC_DIAGNOSTICS
static const char* spiValidationReason(const uint8_t*f){
  if(ND_SPI::get16(f)!=ND_SPI::MAGIC)return "MAGIC";
  if(f[2]!=ND_SPI::VERSION)return "VERSION";
  if(f[3]>ND_SPI::SPI_MSG_ERROR)return "TYPE";
  uint16_t len=ND_SPI::get16(f+8);
  if(!ND_SPI::validLength(len))return "LENGTH";
  if(ND_SPI::get32(f+ND_SPI::HEADER_BYTES+len)!=ND_SPI::crc32(f,ND_SPI::HEADER_BYTES+len))return "CRC";
  return nullptr;
}
static void printSpiHex(const uint8_t*p,size_t n){for(size_t i=0;i<n;++i){if(i)Serial.print(' ');if(p[i]<16)Serial.print('0');Serial.print(p[i],HEX);}}
static void captureFirstC5Transaction(const spi_slave_transaction_t&t){
  if(firstC5TransactionCaptured)return;
  firstC5TransactionCaptured=true;
  const uint8_t*rx=static_cast<const uint8_t*>(t.rx_buffer);
  const uint8_t*tx=static_cast<const uint8_t*>(t.tx_buffer);
  Serial.println(F("[C5 SPI FIRST TRANSACTION]"));
  Serial.print(F("result=")); Serial.println(ESP_OK);
  Serial.print(F("transLenBits=")); Serial.println((unsigned long)t.trans_len);
  Serial.print(F("RX32=")); printSpiHex(rx,32); Serial.println();
  Serial.print(F("TX32=")); printSpiHex(tx,32); Serial.println();
  Serial.print(F("synced=")); Serial.println(spiSynced?1:0);
  Serial.print(F("transaction=")); Serial.println((unsigned long)spiTransactions);
}
static void captureFirstC5Invalid(const uint8_t*f){
  if(firstC5InvalidCaptured)return;
  const char*reason=spiValidationReason(f);
  if(!reason)return;
  firstC5InvalidCaptured=true;
  Serial.println(F("[C5 SPI FIRST INVALID]"));
  Serial.print(F("reason=")); Serial.println(reason);
  Serial.print(F("magicObserved=0x")); Serial.println(ND_SPI::get16(f),HEX);
  Serial.print(F("versionObserved=")); Serial.println(f[2]);
  Serial.print(F("typeObserved=")); Serial.println(f[3]);
  Serial.print(F("lengthObserved=")); Serial.println(ND_SPI::get16(f+8));
}
#endif
static void processSpiRx(const uint8_t*f){uint8_t e=0;if(!ND_SPI::validate(f,&e)){++spiRxDrops;if(e==1)++spiMagicErrors;else if(e==2)++spiVersionErrors;else if(e==3)++spiLengthErrors;else++spiCrcErrors;return;}++spiValidFrames;spiLastSeen=millis();spiSynced=true;const uint8_t type=f[3];const uint16_t len=ND_SPI::get16(f+8);const uint16_t flags=ND_SPI::get16(f+10);const uint32_t channel=ND_SPI::get32(f+12);const uint8_t*p=f+ND_SPI::HEADER_BYTES;
  if(type==ND_SPI::SPI_MSG_HELLO){uint8_t id[4]={'C','5','S','P'};queueSpiFrame(ND_SPI::SPI_MSG_HELLO_ACK,spiTxSequence++,ND_SPI::FLAG_FIRST|ND_SPI::FLAG_LAST,0,id,4);return;}
  if(type==ND_SPI::SPI_MSG_API_RESPONSE){if(cancelledResponsePending&&uint16_t(channel)==cancelledResponseId){if(flags&ND_SPI::FLAG_LAST)cancelledResponsePending=false;return;}if(flags&ND_SPI::FLAG_FIRST){responseLen=0;responseId=uint16_t(channel);responseDone=false;}if(channel!=responseId||responseLen+len>RESP){++spiRxDrops;return;}memcpy(response+responseLen,p,len);responseLen+=len;if(flags&ND_SPI::FLAG_LAST){responseDone=true;expectedResponse=responseLen;}return;}
  if(type==ND_SPI::SPI_MSG_STATUS_REQUEST&&len<sizeof(pendingWifiConfig)){portENTER_CRITICAL(&spiMux);memcpy(pendingWifiConfig,p,len);pendingWifiConfig[len]=0;pendingWifiConfigId=uint16_t(channel);wifiConfigPending=true;portEXIT_CRITICAL(&spiMux);return;}
  if(type==ND_SPI::SPI_MSG_STATUS_RESPONSE&&len<sizeof(pendingSubscription)){memcpy(pendingSubscription,p,len);pendingSubscription[len]=0;subscriptionPending=true;return;}
}

static void spiSlaveTask(void*){spi_slave_transaction_t t={};memset(spiTxFrame,0,sizeof(spiTxFrame));memset(spiRxFrame,0,sizeof(spiRxFrame));ND_SPI::build(spiTxFrame,ND_SPI::SPI_MSG_NOP,0,0,0,nullptr,0);for(;;){memset(&t,0,sizeof(t));t.length=ND_SPI::FRAME_BYTES*8;t.rx_buffer=spiRxFrame;t.tx_buffer=spiTxFrame;esp_err_t r=spi_slave_transmit(SPI_HOST,&t,portMAX_DELAY);if(r!=ESP_OK){++spiRxDrops;spiSynced=false;discardQueuedApiRequests(0,true);vTaskDelay(1);continue;}++spiTransactions;
#if ND_SPI_FORENSIC_DIAGNOSTICS
captureFirstC5Transaction(t);captureFirstC5Invalid(spiRxFrame);
#endif
processSpiRx(spiRxFrame);if(popSpiFrame(spiTxFrame)){digitalWrite(C5_READY,HIGH);++spiReadyHighCount;}else{ND_SPI::build(spiTxFrame,ND_SPI::SPI_MSG_NOP,spiTxSequence++,0,0,nullptr,0);digitalWrite(C5_READY,LOW);} }}

static const char B64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int bv(char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;}
static size_t enc(const uint8_t*i,size_t n,char*o,size_t c){size_t z=0;for(size_t p=0;p<n;p+=3){if(z+4>=c)return 0;uint32_t v=(uint32_t)i[p]<<16|(p+1<n?(uint32_t)i[p+1]<<8:0)|(p+2<n?i[p+2]:0);o[z++]=B64[v>>18&63];o[z++]=B64[v>>12&63];o[z++]=p+1<n?B64[v>>6&63]:'=';o[z++]=p+2<n?B64[v&63]:'=';}o[z]=0;return z;}
static size_t dec(const char*i,uint8_t*o,size_t c){size_t z=0;int v=0,b=-8;for(;*i&&*i!='=';++i){int x=bv(*i);if(x<0)return 0;v=(v<<6)|x;b+=6;if(b>=0){if(z>=c)return 0;o[z++]=(v>>b)&255;b-=8;}}return z;}
static const char* state(){if(ap)return "AP";if(WiFi.status()==WL_CONNECTED)return "STA_CONNECTED";return sta?"STA_CONNECTING":"STA_DISABLED";}
static bool validBand(const char*v){return !strcmp(v,"AUTO")||!strcmp(v,"2G")||!strcmp(v,"5G");}
static const char* defaultBand(){return !strcmp(mode,"STA")?"AUTO":"5G";}
static wifi_band_mode_t bandMode(){return !strcmp(band,"5G")?WIFI_BAND_MODE_5G_ONLY:!strcmp(band,"2G")?WIFI_BAND_MODE_2G_ONLY:WIFI_BAND_MODE_AUTO;}
static bool applyBandMode(){return esp_wifi_set_band_mode(bandMode())==ESP_OK;}
static int apChannel(){return !strcmp(band,"5G")?WIFI_AP_CHANNEL_5G:!strcmp(band,"2G")?WIFI_AP_CHANNEL_2G:0;}
static int effectiveChannel(){return (ap||sta)?int(WiFi.channel()):0;}
static const char* effectiveBand(){const int ch=effectiveChannel();return ch==0?"UNKNOWN":ch>14?"5G":"2G";}
static void configureWifiUdp(const char*subs="1,2,3,4"){artWifi.stop();sacnUnicastWifi.stop();for(auto&u:sacnMulticastWifi)u.stop();artWifi.begin(6454);sacnUnicastWifi.begin(5568);char list[64];strncpy(list,subs,sizeof(list)-1);list[sizeof(list)-1]=0;char*p=list;for(int i=0;i<4;i++){char*e=strchr(p,',');if(e)*e=0;uint16_t u=strtoul(p,nullptr,10);if(u){IPAddress g(239,255,u>>8,u&255);sacnMulticastWifi[i].beginMulticast(g,5568);}if(!e)break;p=e+1;}}
static bool queueLighting(const char*type,uint16_t universe,uint8_t priority,const IPAddress&source,const uint8_t*d,uint16_t length,bool art){if(length>512)return false;int slot=-1;for(int i=0;i<4;i++)if(pendingLighting[i].ready&&!strcmp(pendingLighting[i].type,type)&&pendingLighting[i].universe==universe){slot=i;break;}if(slot<0)for(int i=0;i<4;i++)if(!pendingLighting[i].ready){slot=i;break;}if(slot<0){if(art)++wifiArtDrops;else++wifiSacnDrops;return false;}PendingLighting&f=pendingLighting[slot];if(f.ready){if(art)++wifiArtDrops;else++wifiSacnDrops;}strncpy(f.type,type,sizeof(f.type)-1);f.type[sizeof(f.type)-1]=0;f.universe=universe;f.priority=priority;for(int i=0;i<4;i++)f.source[i]=source[i];f.length=length;memcpy(f.data,d,length);f.ready=true;return true;}
static void serviceLightingTx(){if((int32_t)(micros()-lightingNextUs)<0)return;int slot=-1;for(int n=0;n<4;n++){int i=(nextLightingSlot+n)%4;if(pendingLighting[i].ready){slot=i;nextLightingSlot=(i+1)%4;break;}}if(slot<0)return;PendingLighting&f=pendingLighting[slot];if(queueLightingFrame(f))f.ready=false;lightingNextUs=micros()+500;}
static bool forwardArtPacket(WiFiUDP&u){int n=u.parsePacket();if(n<=0)return false;if(n>int(sizeof(wifiPacket))){u.clear();++wifiArtDrops;return false;}u.read(wifiPacket,n);if(n<18||memcmp(wifiPacket,"Art-Net",8)!=0)return false;if((wifiPacket[8]|wifiPacket[9]<<8)!=0x5000)return false;uint16_t len=(wifiPacket[16]<<8)|wifiPacket[17];if(len>512||18+len>n)return false;uint16_t un=wifiPacket[14]|wifiPacket[15]<<8;if(queueLighting("WART",un,100,u.remoteIP(),wifiPacket+18,len,true))++wifiArtRx;return true;}
static bool forwardSacnPacket(WiFiUDP&u){int n=u.parsePacket();if(n<=0)return false;if(n>int(sizeof(wifiPacket))){u.clear();++wifiSacnDrops;return false;}u.read(wifiPacket,n);if(n<126||memcmp(wifiPacket+4,"ASC-E1.17",9)!=0||wifiPacket[21]!=4||wifiPacket[43]!=2)return false;uint16_t un=(wifiPacket[113]<<8)|wifiPacket[114],raw=(wifiPacket[123]<<8)|wifiPacket[124];if(raw<1||raw>513||126+raw-1>n||wifiPacket[125]!=0)return false;if(queueLighting("WSAC",un,wifiPacket[108],u.remoteIP(),wifiPacket+126,raw-1,false))++wifiSacnRx;return true;}
static void serviceWifiUdp(){forwardArtPacket(artWifi);forwardSacnPacket(sacnUnicastWifi);for(auto&u:sacnMulticastWifi)forwardSacnPacket(u);serviceLightingTx();}
static void startWifi(){
  WiFi.disconnect(true); staConnectedReported=false; bandFallback[0]=0;
  if(!strcmp(mode,"STA")&&ssid[0]){
    WiFi.mode(WIFI_STA);
    if(!applyBandMode()){ strcpy(bandFallback,"AUTO"); esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO); }
    WiFi.begin(ssid,pass); sta=true; ap=false; staStart=millis();
    Serial.printf("[WIFI] mode=STA band=%s connecting_to=%s\n",band,ssid);
  }else{
    WiFi.mode(WIFI_AP); WiFi.softAPConfig(apIp,apIp,apMask);
    bool bandOk=applyBandMode(); ap=WiFi.softAP(apName,apPass,apChannel()); sta=false;
    if((!bandOk||!ap)&&!strcmp(band,"5G")){
      strcpy(bandFallback,"2G");
      esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY); ap=WiFi.softAP(apName,apPass,WIFI_AP_CHANNEL_2G);
    }
    Serial.printf("[WIFI] AP %s SSID=%s IP=%s band=%s channel=%d\n",ap?"START OK":"START FAILED",apName,WiFi.softAPIP().toString().c_str(),band,effectiveChannel());
  }
  configureWifiUdp();
}
static bool queryValue(const char*req,const char*key,char*out,size_t cap){const char*p=strstr(req,key);if(!p)return false;p+=strlen(key);const char*e=strchr(p,'&');const char*sp=strstr(p," HTTP/");if(!e||(sp&&sp<e))e=sp;if(!e)e=p+strlen(p);size_t n=size_t(e-p);if(n>=cap)return false;memcpy(out,p,n);out[n]=0;return true;}
static void serviceWifiConfig(){if(!wifiConfigPending)return;char q[512];uint16_t id;portENTER_CRITICAL(&spiMux);strncpy(q,pendingWifiConfig,sizeof(q)-1);q[sizeof(q)-1]=0;id=pendingWifiConfigId;wifiConfigPending=false;portEXIT_CRITICAL(&spiMux);char v[80];if(queryValue(q,"mode=",v,sizeof(v)))strncpy(mode,!strcmp(v,"STA")?"STA":"AP",sizeof(mode)-1);if(queryValue(q,"band=",v,sizeof(v))){if(validBand(v))strncpy(band,v,sizeof(band)-1);else strcpy(band,defaultBand());}if(queryValue(q,"ssid=",v,sizeof(v)))strncpy(ssid,v,sizeof(ssid)-1);if(queryValue(q,"password=",v,sizeof(v)))strncpy(pass,v,sizeof(pass)-1);mode[3]=0;band[3]=0;ssid[64]=0;pass[64]=0;if(!validBand(band))strcpy(band,defaultBand());if(nvsReady){nvs.putString("mode",mode);nvs.putString("band",band);nvs.putString("ssid",ssid);nvs.putString("pass",pass);}startWifi();char ack[32];int n=snprintf(ack,sizeof(ack),"ACK|%u|wifi-applied",id);queueSpiFrame(ND_SPI::SPI_MSG_STATUS_RESPONSE,spiTxSequence++,ND_SPI::FLAG_FIRST|ND_SPI::FLAG_LAST,id,(uint8_t*)ack,uint16_t(n));}
static void sendWifiStatus(){char s[384];IPAddress ip=ap?WiFi.softAPIP():WiFi.localIP();snprintf(s,sizeof(s),"state=%s;mode=%s;ssid=%s;ip=%s;rssi=%d;clients=%u;fw=%s;configuredBand=%s;effectiveBand=%s;effectiveChannel=%d;bandFallback=%s;spi_init=%s;spi_tx=%lu;spi_valid=%lu;spi_crc=%lu;spi_drop=%lu;spi_resync=%lu;spi_sync=%d;art_rx=%lu;art_drop=%lu;sacn_rx=%lu;sacn_drop=%lu",state(),mode,ap?apName:ssid,ip.toString().c_str(),WiFi.status()==WL_CONNECTED?WiFi.RSSI():0,ap?WiFi.softAPgetStationNum():0,ND_ESP_FW_VERSION,band,effectiveBand(),effectiveChannel(),bandFallback[0]?bandFallback:"NONE",spiInitOk?"OK":"FAIL",(unsigned long)spiTransactions,(unsigned long)spiValidFrames,(unsigned long)spiCrcErrors,(unsigned long)(spiRxDrops+spiTxDrops+spiLightingDrops),(unsigned long)spiLinkResyncs,spiSynced?1:0,(unsigned long)wifiArtRx,(unsigned long)wifiArtDrops,(unsigned long)wifiSacnRx,(unsigned long)wifiSacnDrops);queueSpiFrame(ND_SPI::SPI_MSG_STATUS_RESPONSE,spiTxSequence++,ND_SPI::FLAG_FIRST|ND_SPI::FLAG_LAST,0,(uint8_t*)s,uint16_t(strlen(s)));}
static const char* c5OtaStateName(){switch(c5OtaState){case C5_OTA_RECEIVING:return "RECEIVING";case C5_OTA_VERIFYING:return "VERIFYING";case C5_OTA_READY_TO_REBOOT:return "READY_TO_REBOOT";case C5_OTA_REBOOTING:return "REBOOTING";case C5_OTA_SUCCESS:return "SUCCESS";case C5_OTA_FAILED:return "FAILED";default:return "IDLE";}}
static void c5OtaSetError(const char*msg){strncpy(c5OtaLastError,msg?msg:"update failed",sizeof(c5OtaLastError)-1);c5OtaLastError[sizeof(c5OtaLastError)-1]=0;c5OtaState=C5_OTA_FAILED;c5OtaBusy=false;}
static void c5OtaSendStatus(WiFiClient&c,bool ok){char body[360];const esp_partition_t*next=esp_ota_get_next_update_partition(nullptr);const size_t maxSize=next?next->size:0;const unsigned percent=c5OtaTotal?unsigned((uint64_t(c5OtaReceived)*100u)/c5OtaTotal):0;int n=snprintf(body,sizeof(body),"{\"ok\":%s,\"target\":\"ESP32-C5\",\"installed\":\"%s\",\"state\":\"%s\",\"receivedBytes\":%lu,\"totalBytes\":%lu,\"percent\":%u,\"maxBytes\":%lu,\"lastError\":\"%s\"}",ok?"true":"false",ND_ESP_FW_VERSION,c5OtaStateName(),(unsigned long)c5OtaReceived,(unsigned long)c5OtaTotal,percent,(unsigned long)maxSize,c5OtaLastError);c.print(ok?"HTTP/1.1 200 OK\r\n":"HTTP/1.1 400 Bad Request\r\n");c.print("Content-Type: application/json\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n");c.write((const uint8_t*)body,n);}
static bool readUpdateBody(WiFiClient&c){uint8_t buf[4096];uint32_t last=millis();while(c5OtaReceived<c5OtaTotal){if(!c.connected())return false;size_t want=c5OtaTotal-c5OtaReceived;if(want>sizeof(buf))want=sizeof(buf);int n=c.read(buf,want);if(n>0){last=millis();if(Update.write(buf,n)!=size_t(n))return false;c5OtaReceived+=uint32_t(n);}else{if(millis()-last>15000u)return false;serviceWifiUdp();delay(1);}}return true;}
static void sendC5OtaRejected(WiFiClient&c,const char*msg){c5OtaSetError(msg);c5OtaSendStatus(c,false);}
static bool handleC5OtaRequest(WiFiClient&c){if(!strncmp(request,"GET /api/ota/c5/status",22)){c5OtaSendStatus(c,true);return true;}if(strncmp(request,"POST /api/ota/c5 ",17)!=0)return false;if(c5OtaBusy){sendC5OtaRejected(c,"update already in progress");return true;}if(!httpUpdateConfirm){sendC5OtaRejected(c,"explicit update confirmation required");return true;}if(strcmp(httpContentType,"application/octet-stream")!=0){sendC5OtaRejected(c,"Content-Type must be application/octet-stream");return true;}const esp_partition_t*next=esp_ota_get_next_update_partition(nullptr);const size_t maxSize=next?next->size:0;if(!next){sendC5OtaRejected(c,"no inactive OTA partition");return true;}if(!httpContentLength||httpContentLength>maxSize){char msg[96];snprintf(msg,sizeof(msg),"image exceeds inactive OTA slot (%lu bytes)",(unsigned long)maxSize);sendC5OtaRejected(c,msg);return true;}c5OtaBusy=true;c5OtaState=C5_OTA_RECEIVING;c5OtaReceived=0;c5OtaTotal=uint32_t(httpContentLength);c5OtaLastError[0]=0;if(!Update.begin(c5OtaTotal,U_FLASH)){sendC5OtaRejected(c,Update.errorString());return true;}if(!readUpdateBody(c)){Update.abort();sendC5OtaRejected(c,"receive/write failed or timed out");return true;}c5OtaState=C5_OTA_VERIFYING;if(!Update.end()){sendC5OtaRejected(c,Update.errorString());return true;}c5OtaState=C5_OTA_READY_TO_REBOOT;c5OtaBusy=false;c5OtaSendStatus(c,true);delay(250);c5OtaState=C5_OTA_REBOOTING;ESP.restart();return true;}
static bool readReq(WiFiClient&c){size_t n=0,lineLen=0;bool first=true;char line[192];httpContentLength=0;httpUpdateConfirm=false;httpContentType[0]=0;uint32_t st=millis();while(millis()-st<500){while(c.available()){char x=c.read();if(x=='\n'){line[lineLen]=0;if(first){strncpy(request,line,sizeof(request)-1);request[sizeof(request)-1]=0;n=strlen(request);first=false;}else if(lineLen==0)return n>0;else if(!strncasecmp(line,"Content-Length:",15))httpContentLength=strtoul(line+15,nullptr,10);else if(!strncasecmp(line,"Content-Type:",13)){const char*v=line+13;while(*v==' ')++v;strncpy(httpContentType,v,sizeof(httpContentType)-1);httpContentType[sizeof(httpContentType)-1]=0;}else if(!strncasecmp(line,"X-ND-Update-Confirm:",20)){const char*v=line+20;while(*v==' ')++v;httpUpdateConfirm=!strcasecmp(v,"yes");}lineLen=0;}else if(x!='\r'&&lineLen<sizeof(line)-1)line[lineLen++]=x;}serviceWifiUdp();delay(1);}request[n]=0;return n>0;}
static bool queueApiRequest(const char*r,uint16_t id){if(!spiInitOk||!spiSynced)return false;return queueSpiFrame(ND_SPI::SPI_MSG_API_REQUEST,spiTxSequence++,ND_SPI::FLAG_FIRST|ND_SPI::FLAG_LAST,id,(const uint8_t*)r,uint16_t(strlen(r)));}
static void proxy(){WiFiClient c=server.accept();if(!c)return;if(!readReq(c)){c.stop();return;}if(handleC5OtaRequest(c)){c.stop();return;}uint16_t id=uint16_t(++proxyRequestsStarted);lastProxyRequestId=id;if(!spiInitOk){c.print("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nSPI link initialization failed");c.stop();return;}if(!spiSynced){c.print("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nRP2350 SPI link not synchronized");c.stop();return;}if(!queueApiRequest(request,id)){c.print("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nSPI queue full");c.stop();return;}++proxyReqFramesSent;responseDone=false;uint32_t st=millis();while(!responseDone&&millis()-st<1800){serviceWifiUdp();delay(1);}if(responseDone)c.write(response,responseLen);else{discardQueuedApiRequests(id);cancelledResponseId=id;cancelledResponsePending=true;c.print("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nRP2350 SPI unavailable");}c.stop();}
void setup(){Serial.begin(115200);delay(100);Serial.println("ND Pico2DMX ESP32-C5");pinMode(C5_READY,OUTPUT);digitalWrite(C5_READY,LOW);spi_bus_config_t b={};b.mosi_io_num=C5_MOSI;b.miso_io_num=C5_MISO;b.sclk_io_num=C5_SCK;b.quadwp_io_num=-1;b.quadhd_io_num=-1;b.max_transfer_sz=ND_SPI::FRAME_BYTES;spi_slave_interface_config_t s={};s.spics_io_num=C5_CS;s.queue_size=1;s.mode=0;spiInitError=spi_slave_initialize(SPI_HOST,&b,&s,SPI_DMA_CH_AUTO);spiInitOk=spiInitError==ESP_OK;Serial.printf("[SPI] host=SPI2 pins SCK=%d MOSI=%d MISO=%d CS=%d clock=%lu init=%s\n",C5_SCK,C5_MOSI,C5_MISO,C5_CS,(unsigned long)ND_SPI::CLOCK_HZ,spiInitOk?"PASS":"FAIL");nvsReady=nvs.begin("ndwifi",false);String m=nvsReady?nvs.getString("mode","AP"):"AP",bd=nvsReady?nvs.getString("band",""):"",ss=nvsReady?nvs.getString("ssid",""):"",pp=nvsReady?nvs.getString("pass",""):"";strncpy(mode,m.c_str(),3);mode[3]=0;strncpy(band,bd.c_str(),3);band[3]=0;if(!validBand(band))strcpy(band,defaultBand());strncpy(ssid,ss.c_str(),64);ssid[64]=0;strncpy(pass,pp.c_str(),64);pass[64]=0;if(nvsReady){nvs.putString("mode",mode);nvs.putString("band",band);}startWifi();server.begin();Serial.println("[HTTP] listening on port 80");if(spiInitOk)xTaskCreate(spiSlaveTask,"spi_slave_task",8192,nullptr,3,nullptr);lastHb=millis();}
void loop(){serviceWifiConfig();if(subscriptionPending){portENTER_CRITICAL(&spiMux);char sub[64];strncpy(sub,pendingSubscription,sizeof(sub));subscriptionPending=false;portEXIT_CRITICAL(&spiMux);if(!strncmp(sub,"SUB|",4))configureWifiUdp(sub+4);}if(spiSynced&&millis()-spiLastSeen>SPI_LINK_TIMEOUT_MS){spiSynced=false;++spiLinkResyncs;discardQueuedApiRequests(0,true);}serviceWifiUdp();proxy();if(!ap&&sta&&WiFi.status()==WL_CONNECTED&&!staConnectedReported){staConnectedReported=true;Serial.printf("[WIFI] STA CONNECTED IP=%s RSSI=%d\n",WiFi.localIP().toString().c_str(),WiFi.RSSI());}if(!ap&&sta&&WiFi.status()!=WL_CONNECTED&&millis()-staStart>10000){Serial.println("[WIFI] STA CONNECTION FAILED; FALLING BACK TO AP");strcpy(mode,"AP");startWifi();}if(millis()-lastHb>5000){lastHb=millis();sendWifiStatus();}if(millis()-lastDiag>5000){lastDiag=millis();Serial.printf("[SPI] synced=%d tx=%lu rx=%lu valid=%lu crc=%lu drops=%lu resync=%lu\n",spiSynced,spiTransactions,spiTransactions,spiValidFrames,spiCrcErrors,spiRxDrops+spiTxDrops+spiLightingDrops,spiLinkResyncs);}delay(1);}
