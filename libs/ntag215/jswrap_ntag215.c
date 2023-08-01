#ifdef USE_NTAG215
#include "jswrap_ntag215.h"
#include "jsinteractive.h"
#include "hal_t2t/hal_nfc_t2t.h"
#include "bluetooth_utils.c"

/*JSON{
  "type" : "class",
  "class" : "NTAG215",
  "ifdef" : "USE_NTAG215"
}
Functions for emulating a NTAG215 NFC tag.
*/
/*JSON{
  "type" : "staticproperty",
  "class" : "NTAG215",
  "name" : "version",
  "generate_full" : "(1 << 16) + (0 << 8) + 0",
  "return" : ["int","The API version as a 24-bit number."]
}*/

static unsigned char *tag = 0;
static bool nfcStarted = false;

/*JSON{
  "type" : "staticmethod",
  "class" : "NTAG215",
  "name" : "setTagData",
  "generate" : "jswrap_ntag215_setTagData",
  "ifdef" : "USE_NTAG215",
  "params" : [
    ["v","JsVar","A UInt8Array at least 572 bytes long."]
  ]
}*/
void jswrap_ntag215_setTagData(JsVar *v){
  tag = 0;

  if (nfcStarted) {
    jsExceptionHere(JSET_ERROR, "NFC cannot be active.");
    return;
  }

  if (!jsvIsArrayBuffer(v)) {
    jsExceptionHere(JSET_ERROR, "Variable is not an array buffer.");
    return;
  }

  size_t len=0;
  char *pointer = jsvGetDataPointer(v, &len);

  if (len < 572) {
    jsExceptionHere(JSET_ERROR, "Array buffer is not at least 572 bytes.");
    return;
  }

  if (pointer == 0) {
    jsExceptionHere(JSET_ERROR, "Failed to get flat string from array buffer.");
    return;
  }

  tag = pointer;
}

static unsigned char *tagUid = 0;
static unsigned char *tx = 0;

/*JSON{
  "type" : "staticmethod",
  "class" : "NTAG215",
  "name" : "setTagBuffer",
  "generate" : "jswrap_ntag215_setTagBuffer",
  "ifdef" : "USE_NTAG215",
  "params" : [
    ["v","JsVar","A UInt8Array at least 572 bytes long."]
  ]
}*/
void jswrap_ntag215_setTagBuffer(JsVar *v){
  tagUid = 0;
  tx = 0;

  if (nfcStarted) {
    jsExceptionHere(JSET_ERROR, "NFC cannot be active.");
    return;
  }

  if (!jsvIsArrayBuffer(v)) {
    jsExceptionHere(JSET_ERROR, "Variable is not an array buffer.");
    return;
  }

  size_t len=0;
  char *pointer = jsvGetDataPointer(v, &len);

  if (len < 32) {
    jsExceptionHere(JSET_ERROR, "Array buffer is not at least 32 bytes.");
    return;
  }

  if (pointer == 0) {
    jsExceptionHere(JSET_ERROR, "Failed to get flat string from array buffer.");
    return;
  }

  tagUid = &pointer[0];
  tx = &pointer[7];
}

static const unsigned char version[] IN_FLASH_MEMORY = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x11, 0x03};
static const unsigned char password_success[] IN_FLASH_MEMORY = {0x80, 0x80};
static const unsigned char puck_success[] IN_FLASH_MEMORY = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
static const unsigned char zero_page[] IN_FLASH_MEMORY = {0x00, 0x00, 0x00, 0x00};

static bool authenticated = false;
static void setAuthenticated(bool value){ authenticated = value; }
static bool getAuthenticated(){ return authenticated; }

static bool backdoor = false;
static void setBackdoor(bool value){ backdoor = value; }
static bool getBackdoor() { return backdoor; }

static bool tagWritten = false;

/*JSON{
  "type" : "staticmethod",
  "class" : "NTAG215",
  "name" : "setTagWritten",
  "generate" : "jswrap_ntag215_setTagWritten",
  "ifdef" : "USE_NTAG215",
  "params" : [
    ["value","bool","The value."]
  ]
}*/
void jswrap_ntag215_setTagWritten(bool value){ tagWritten = value; }

/*JSON{
  "type" : "staticmethod",
  "class" : "NTAG215",
  "name" : "getTagWritten",
  "generate" : "jswrap_ntag215_getTagWritten",
  "ifdef" : "USE_NTAG215",
  "return" : ["bool", "Whether the tag was written to."]
}*/
bool jswrap_ntag215_getTagWritten(){ return tagWritten; }

/*JSON{
  "type" : "staticmethod",
  "class" : "NTAG215",
  "name" : "fixUid",
  "generate" : "jswrap_ntag215_fixUid",
  "ifdef" : "USE_NTAG215",
  "return" : ["bool", "Whether the UID was fixed or not."]
}*/
bool jswrap_ntag215_fixUid(){
  if (tag == 0){
    jsExceptionHere(JSET_ERROR, "No tag pointer set\n");
    return false;
  }

  if (tagUid == 0 || tx == 0){
    jsExceptionHere(JSET_ERROR, "No tag buffer set\n");
    return false;
  }

  unsigned char bcc0 = tag[0] ^ tag[1] ^ tag[2] ^ 0x88;
  unsigned char bcc1 = tag[4] ^ tag[5] ^ tag[6] ^ tag[7];

  if (tag[3] != bcc0 || tag[8] != bcc1){
    tag[3] = bcc0;
    tag[8] = bcc1;

    return true;
  }

  return false;
}

static bool isLocked(int page){
  if (page == 0 || page == 1) return true;
  // Static Lock Bytes
  int bit;

  for (bit = 0; bit < 8; bit++){
    if (tag[11] & (1 << bit)){
      if ((bit + 8) == page){
        return true;
      }
    }

    if (tag[10] & (1 << bit)){
      switch (bit){
        case 0: //BL-CC
        case 1: //BL-9-4
        case 2: //BL-15-10
        case 3: //L-CC
          break;
        default: {
          if ((bit + 4) == page){
            return true;
          }
        } break;
      }
    }
  }

  if (authenticated == false){
    if (tag[520] & 0b00000001 > 0 && (page >= 16 && page <= 31))
      return true;

    if (tag[520] & 0b00000010 > 0 && (page >= 32 && page <= 47))
      return true;

    if (tag[520] & 0b00000100 > 0 && (page >= 48 && page <= 63))
      return true;

    if (tag[520] & 0b00001000 > 0 && (page >= 64 && page <= 79))
      return true;

    if (tag[520] & 0b00010000 > 0 && (page >= 80 && page <= 95))
      return true;

    if (tag[520] & 0b00100000 > 0 && (page >= 96 && page <= 111))
      return true;

    if (tag[520] & 0b01000000 > 0 && (page >= 112 && page <= 127))
      return true;

    if (tag[520] & 0b10000000 > 0 && (page >= 128 && page <= 129))
      return true;
  }

  return false;
}

static unsigned char *readBlock(uint8_t block){

}

static void processRx(unsigned char *rx, int rxLen){
  if (rx == 0){
    jsiConsolePrintf("NTAG215 NFC RX pointer is 0\n");
    return;
  }

  if (rxLen == 0){
    hal_nfc_send_rsp(0, 0);
    jsiConsolePrintf("NTAG215 NFC RX length is 0\n");
    return;
  }

  switch (rx[0]) {
    case 0x30: { // 48 - Read
      if (rxLen < 2){
        jsiConsolePrintf("NTAG215 READ: bad rxlen - %d\n", rxLen);
        hal_nfc_send_rsp(0, 0);
        return;
      }

      hal_nfc_send(&tag[rx[1] * 4], 16);
      return;
    }

    case 0xA2: { //  162 - Write
      if (backdoor == false && (rx[1] < 0 || rx[1] > 134 || isLocked(rx[1]))) {
        hal_nfc_send_rsp(0, 4);
        return;
      }

      if (backdoor == false) {
        if (rx[1] == 2) {
          tag[10] = tag[10] | rx[4];
          tag[11] = tag[11] | rx[5];
          tagWritten = true;
          hal_nfc_send_rsp(0x0a, 4);
          return;
        }

        if (rx[1] == 3) {
          tag[16] = tag[16] | rx[2];
          tag[17] = tag[17] | rx[3];
          tag[18] = tag[18] | rx[4];
          tag[19] = tag[19] | rx[5];
          tagWritten = true;
          hal_nfc_send_rsp(0x0a, 4);
          return;
        }

        if (rx[1] == 130) {
          // TODO: Dynamic lock bits
        }
      }

      int index = rx[1] * 4;
      if ((index > 568) || (backdoor == false && index > 536)) {
        hal_nfc_send_rsp(0, 4);
        jsiConsolePrintf("NTAG215 WRITE: page oob - %d\n", rx[1]);
        return;
      } else {
        memcpy(&tag[index], &rx[2], 4);
        tagWritten = true;
        hal_nfc_send_rsp(0xA, 4);
        return;
      }
    }

    case 0x60: { // 96 - Version
      hal_nfc_send(version, 8);
      return;
    }

    case 0x3A: { // 58 - Fast Read
      if (rxLen < 3){
        hal_nfc_send_rsp(0, 4);
        jsiConsolePrintf("NTAG215 FAST READ: Invalid rx length - %d\n", rxLen);
        return;
      }

      if (rx[1] > rx[2] || rx[1] < 0 || rx[2] > 134) {
        hal_nfc_send_rsp(0, 4);
        jsiConsolePrintf("NTAG215 FAST READ: Invalid address - %d:%d\n", rx[1], rx[2]);
        return;
      }

      if (rx[1] == 133 && rx[2] == 134) {
        backdoor = true;
        jsiConsolePrintf("NTAG215 FAST READ: Backdoor enabled\n");
        hal_nfc_send(puck_success, 8);
        return;
      }

      unsigned int tag_location = rx[1] * 4;
      unsigned int tx_len = (rx[2] - rx[1] + 1) * 4;
      hal_nfc_send(&tag[tag_location], tx_len);
      return;
    }

    case 0x1B: { // 27 - Password Auth
      hal_nfc_send(password_success, 2);
      authenticated = true;
      return;
    }

    case 0x3C: { // 60 - Read Signature
      hal_nfc_send(&tag[540], 32);
      return;
    }

    case 0x88: { // 136 - CUSTOM: Restart NFC
      hal_nfc_send_rsp(0xA, 4);
      jswrap_ntag215_restartNfc();
      return;
    }

    default: { // Unknown command
      hal_nfc_send_rsp(0, 0);
      jsiConsolePrintf("Unknown Command: %d\n", rx[0]);
      return;
    }
  }
}

static void nfc_callback(void * p_context, hal_nfc_event_t event, const uint8_t * p_data, size_t data_length) {
  switch (event){
    case HAL_NFC_EVENT_FIELD_ON:
      bleQueueEventAndUnLock(JS_EVENT_PREFIX"NFCon", 0);
      break;

    case HAL_NFC_EVENT_FIELD_OFF:
      authenticated = false;
      backdoor = false;

      bleQueueEventAndUnLock(JS_EVENT_PREFIX"NFCoff", 0);
      break;

    case HAL_NFC_EVENT_DATA_RECEIVED:
      processRx(p_data, data_length);
      break;

    case HAL_NFC_EVENT_DATA_TRANSMITTED: break;
    default:
      jsiConsolePrintf("Unknown nfc event: %d\n", event);
  }
}

/*JSON{
  "type" : "staticmethod",
  "class" : "NTAG215",
  "name" : "nfcStop",
  "generate" : "jswrap_ntag215_stopNfc",
  "ifdef" : "USE_NTAG215"
}*/

void jswrap_ntag215_stopNfc(){
  hal_nfc_stop();
  hal_nfc_done();
  nfcStarted = false;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "NTAG215",
  "name" : "nfcStart",
  "generate" : "jswrap_ntag215_startNfc",
  "ifdef" : "USE_NTAG215",
  "return" : ["int32", "The return code."]
}*/
int jswrap_ntag215_startNfc(){
  int ret_val = 0;

  if (tag == 0){
    jsExceptionHere(JSET_ERROR, "No tag pointer set\n");
    return 100;
  }

  if (tagUid == 0 || tx == 0){
    jsExceptionHere(JSET_ERROR, "No tag buffer set\n");
    return 101;
  }

  jswrap_ntag215_stopNfc();

  tagUid[0] = tag[0];
  tagUid[1] = tag[1];
  tagUid[2] = tag[2];
  tagUid[3] = tag[4];
  tagUid[4] = tag[5];
  tagUid[5] = tag[6];
  tagUid[6] = tag[7];
  ret_val = hal_nfc_parameter_set(HAL_NFC_PARAM_ID_NFCID1, tagUid, 7);
  ret_val = hal_nfc_setup(nfc_callback, 0);
  ret_val = hal_nfc_start();

  nfcStarted = true;

  return ret_val;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "NTAG215",
  "name" : "nfcRestart",
  "generate" : "jswrap_ntag215_restartNfc",
  "ifdef" : "USE_NTAG215",
  "return" : ["int32", "The return code."]
}*/
int jswrap_ntag215_restartNfc(){
  jswrap_ntag215_stopNfc();
  return jswrap_ntag215_startNfc();
}
#endif
