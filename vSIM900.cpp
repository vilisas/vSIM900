/*
 * VSIM900.cpp
 *
 *  Created on: 2017-08-05
 *      Author: vilisas / sutemos at sutemos dot lt
 *      free for commercial and non commercial use
 */

#include "vSIM900.h"
//#include <Arduino.h>
#include <HardwareSerial.h>
#include <avr/wdt.h>

VSIM900* VSIM900::_inst;

VSIM900::VSIM900(HardwareSerial& hwPort, int baud) {
	// TODO Auto-generated constructor stub
	_inst = this;
	_serialPort = &hwPort;
	_baudRate = baud;
//	debug= nullptr;

	resetModemStates();
}

VSIM900::~VSIM900() {
	// TODO Auto-generated destructor stub
}

void VSIM900::switchModem(uint8_t switchPin){
	wdt_reset();
	  digitalWrite(switchPin, LOW);
	  delay(500);
	  digitalWrite(switchPin, HIGH);
	  delay(1100);
	  digitalWrite(switchPin, LOW);
	wdt_reset();
}

void VSIM900::resetModem(uint8_t resetPin){
	wdt_reset();
	  digitalWrite(resetPin, LOW);
	  delay(500);
	  digitalWrite(resetPin, HIGH);
	  delay(500);
	  digitalWrite(resetPin, LOW);
	wdt_reset();
}

int VSIM900::sendATCommand(const String& cmd, unsigned int dWait=200, char retries=2, bool ignoreErrors=false){
// kai ignoreErrors = false, tai nutraukiam cikla ir aptike klaida
// kai ignoreErrors = true, tai cikla kartojam net ir aptike klaida. To reikia, kai tikrinam pvz PIN busena
  uint8_t i=0;
  modem.status=msUNKNOWN;
  _serialPort->setTimeout(1000);

  do {
	wdt_reset();
//    Serial.print("\n");
    _serialPort->println(cmd);
    delay(dWait);
//    serialEvent();
    readModemResponse();
  if (!ignoreErrors) {
      if (modem.status !=0) i=retries;
  } else {
      if ((modem.status != msERROR) and (modem.status != 0)) i= retries;
  }

    i++;
  } while (i<retries);
  return (modem.status);
}

int VSIM900::sendATCommandChar(char *cmd){
// kai ignoreErrors = false, tai nutraukiam cikla ir aptike klaida
// kai ignoreErrors = true, tai cikla kartojam net ir aptike klaida. To reikia, kai tikrinam pvz PIN busena
//  uint8_t i=0;
  modem.status=msUNKNOWN;
  _serialPort->setTimeout(1000);

    _serialPort->write(cmd, strlen(cmd));
    _serialPort->write("\r\n");
    delay(100);
//    serialEvent();
    readModemResponse();
  return (modem.status);
}



void VSIM900::serialEvent(){
	/*
	 * you can call this from main loop or serialEvent() in main skech
	 */
	if (_serialPort->available()) {
		readModemResponse();
	}
}

int VSIM900::initModem(){
  char counter, i = 0;
  String temp;
  modem.rssi = 99;
  modem.ber  = 99;
  wdt_reset();
  resetModemStates();
  i=sendATCommand(F("ATE0"));
  if ((modem.status == 0) or (modem.status == msERROR)) {
    modem.init_failure_counter++;
    debug(F("+MODEM:INIT_ERROR"));
    return(meINIT);
  } else {
    debug(F("+MODEM:ALIVE"));
    modem.init_failure_counter = 0;
  }
//  analogWrite(13, 30);
  i = sendATCommand(F("AT+CPIN?"),1000,5,true);

  if (!modem.pin_ok) { debug(F("PIN FAILED")); return(mePIN); } else debug(F("PIN OK"));
// reset connection by default
  delay(1000);
  i = sendATCommand(F("AT+DDET=1"),200, 3, true);
  if (i == msERROR) {
	  debug(F("DTMF detector init failed"));
#ifdef DTMF_REQUIRED
	  return(meDTMF);
#endif
  } else debug(F("DTMF OK"));

//  delay (500);
//  sendATCommand(F("ATH0"));  // if any..
//  delay(200);
  modemHangUp();
  sendATCommand(F("AT+CIPSHUT"));

  delay(2000);
//  temp = "NET";
  i=15;		// kiek kartu tikrinti tinklo busena, kol pasiduosim.

//   wdt_reset();
  do{
	  i--;
  //  delay(250);
    sendATCommand(F("AT+CREG?"),1000,1,true);
//    if (modem.response.substring(0,6) == "+CREG:"){
    if (strncmp(modem.response, "+CREG:", 6)== 0){
  // +CREG: 0,1
        temp = modem.response;		//.substring(9,1);
        modem.network_ok = (modem.response[9]=='1');	// arduino bug ??? (9,1) grazina "+CREG: 0,"
//        modem.network_ok = (modem.response.substring(9)=="1");	// arduino bug ??? (9,1) grazina "+CREG: 0,"
        if (modem.network_ok) i=0;
   }
//    temp = temp + ".";
#ifdef DEBUGMODE
   debug(temp);
#endif
  } while (i>0);
	temp = "";
//   blink(30,5);

  if (modem.network_ok) {debug(F("NETWORK OK")); } else {debug(F("Net Not Ready")); return(meNETWORK);}
   delay(2000);
counter=0;

do {
  sendATCommand(F("AT+CGATT?"), 4000, 1, true);
//  modem.gprs_ok = (modem.response.substring(8)=="1");
  modem.gprs_ok = (modem.response[8]=='1');
  if (modem.gprs_ok) {counter=8;} else counter++;  // counter=5
} while (counter<6); // counter<3
  if (modem.gprs_ok) {debug(F("GPRS OK")); } else {debug(F("GPRS Failed")); return(meGPRS);}


// duomenu gavimui hex formatu
//   analogWrite(13, 130);
i = sendATCommand(F("AT+CIPRXGET=1"), 1000, 2, true);
	if (i==msOK) {debug(F("+CIPRXGET: OK")); } else {debug(F("+CIPRXGET: FAIL?"));}
delay(200);
counter=0;
do {
//i = sendATCommand(F("AT+CSTT=" GPRS_APN), 2000, 2, true);
	i = sendATCommand("AT+CSTT=\"" + String(_modemAPN) + "\"", 2000, 2, true);
  if (i==msOK) {counter=3;} else counter++;
} while (counter<3);

  if (i==msOK) {debug(F("APN OK")); } else {debug(F("APN? SOFT RST?"));}

  i = sendATCommand(F("AT+CIICR"), 2000, 4, true);
  if (i==msOK) {debug(F("CIICR OK")); } else {debug(F("CIICR Failed?")); return(meCIICR);}

// jei negaunam ip adreso, tai kazkas tikrai negerai..
delay(1000);
i = sendATCommand(F("AT+CIFSR"), 1000, 1, true);
//	modem.ip_address = String(modem.response);
	strncpy(modem.ip_address, modem.response, sizeof(modem.ip_address));
	debug(F("po CIFSR:"));
	debug(modem.response);
	debug(modem.ip_address);
// modem.ip_address = String(modem.response);
 if (strlen(modem.ip_address) > 0) {
	 debug("IP:"+ String(modem.ip_address));
 } else {
	 debug(F("IP failed"));
	 return(meIP);
 }

  modem.initialized = true;
  // server port

  #ifndef TCP_CLIENT
  // jei softas sukonfiguruotas kaip TCP klientas, tuomet serverio nekuriame.
  sendATCommand(F("AT+CIPSERVER=1,223"));
  delay(500);
  sendATCommand(F("AT+CIPQSEND=1"));
  delay(500);
  debug F(("Server started"));
  #endif

  sendATCommand(F("AT+CSQ"),200);

  debug(F("INIT Finished.."));
  return(msINIT_COMPLETED);
//  delay (500);

}


void VSIM900::readModemResponse(){
// duomenu gavimui is serial porto, kai tik jie ateina.
// modem.status, serialText modemResponse
// modemLine - saugome paskutini modemo atsakyma tarp chr10 ir chr13, jis perrasomas kiekviena karta
// modem.response - string'as, kuriame modemo atsakymai, isskyus statuso busenas, kaip OK, ERR, RDY... pvz bus +CPIN:READY
// modemStatus globaliajame kintamajame saugoma modemo busena
// modemStatusChanged() funkcija kvieciame tada, kai modemo busena pasikeicia (t.y. kai modemas ismeta viena is pranesimu OK, ERROR, RDY,...)
//
// abstrakciai:
// reiktu skaityt po eilute, eilute nuskaicius - paziuret ka gavom, jei reikia skaityt dar eilute - pasizymet koks bus duomenu tipas
// tada ja nuskaityt, ir tuos duomenis padet kur reikia.
//
// kazkas blogo nutinka, kai nusiunciam du duomenu paketus su labai mazu laiko tarpu (pvz enter x2)
// tada nustoja skaityt duomenis. (+CIPRXGET:1 patampa +CIPRXGET:2 ??? )
//
// modemLine[BUFFER_SIZE * 2], nes cia ir hex buferis. (modemas atiduoda tcp paketa hex formatu, = dvigubai daugiau zenklu + cr,lf
//  int indx = 0;
  uint16_t indx = 0;
  uint16_t posEnd 	= 0;
  char simbolis = 0;
  uint16_t modemLineLength;
  int bytesReadFromPort = 0;
//  char modemLine[BUFFER_SIZE * 2 + 2];  // nes hex cia ir hex buferis + cr lf (+null ?)
  char modemLine[MODEM_BUFFER_SIZE + 2];  // modemLine nebus ilgesnis uz readBuffer dydi
  int tmpModemStatus;
  bool nextIsHex = false;
//  for (index = 0; index < BUFFER_SIZE + 2; index++) modemLine[index] = 0;

//modem.sendATResponseToClient = true;
  if (modem.sendATResponseToClient){
        modem.sendATResponseToClient = false;
        while(_serialPort->available()){
//          if (index < (BUFFER_SIZE * 2)) modemLine[index++] = Serial.read(); //  zr buferio dydzio aprasyma
        if (indx < (MODEM_BUFFER_SIZE-1)) {
        	modemLine[indx] = _serialPort->read(); //  paskutini simboli masyve rezervuojam nuliukui, todel BUFFER_SIZE-1
        	indx++;
        }
        }
          modemLine[indx++] = 0; // null terminate / nebutinas siuo atveju, nes naudojam indekso dydi
          //
          // index = buffer_size; data is available, index = vis dar buffer size....
          // data unavailable.. modemLine[index] = null; index = index +1; send reply
          //

          sendModemDataChar(modemLine, indx);
        return;
  }

    _readModemResponseStarted = true;
    while (_serialPort->available()){
//	  int indx= index;
	  /* eclipse + arduino workspace + avr-gcc 4.9.1
	   * 1) sitoje vietoje panaudoti esamo int index negalime, nes
	   * padarius index++ nebesikompiliuoja kodas. Jei vietoj int padarom uint16_t, tuomet viskas veikia
	   */

    simbolis=_serialPort->read();
	bytesReadFromPort++;
    if (simbolis == 10) { //new line
        indx = 0;
    } else if (simbolis == 13){
        //eilutes pabaiga
        //posEnd - paskutinio tinkamo simbolio vieta
         //index - eilutes terminatoriaus chr(13) vieta, sitoj vietoj buferyje irasom nuli, kad txt funkcijos zinotu kur teksto pabaiga
//        posEnd = index-1;
		posEnd = indx;
        _readBuffer[indx] = 0; // null
        modemLineLength = posEnd;
        if (posEnd >0) {
          strncpy(modemLine, _readBuffer, posEnd+1);
          modemLine[posEnd+1]=0;////////
          if (!nextIsHex) {
            tmpModemStatus = setModemStatus(modemLine);
            if (tmpModemStatus == msDATA_RECEIVING_HEX) {
              nextIsHex = true;
            } else {
					if (tmpModemStatus != 0) { // sekanti eilute tikrai bus tik hex duomenys..
						modemStatusChanged();
					} else strcpy(modem.response, modemLine);
						//modem.response = String(modemLine);
			  }
		  } else {
            nextIsHex = false;
            // kvieciam konvertavimo is hex i char masyva funkcija cia
            modem.rxpacketsize = hexToChar(modemLine, modem.rxpacket, modemLineLength);
            modem.rxpacket[modem.rxpacketsize]=0;
            if (modem.rxpacketsize > 0) {
				modem.received_new_packet = true;
				modem.data_bytes_received += modem.rxpacketsize;
				modem.last_dataReceivedTimestamp = getTimeStamp();
			}
          }
        } // else {vykdyti tai, jei gauta eilute yra tuscia (0 baitu tarp lf ir cr}

    } else if ((simbolis >31) and (simbolis <127)) {
        	_readBuffer[indx] = simbolis;
        	indx++;
        /* po index++; gaunam
         * ../gsm_valdiklis.ino: In function 'readModemResponse':
         * ../gsm_valdiklis.ino:1068:1: internal compiler error: Segmentation fault
		 */
    	}
// apsauga
//    if (index >= BUFFER_SIZE) index = 0;   // gal geriau index=BUFFER_SIZE-1
	if (indx >= MODEM_BUFFER_SIZE) indx = MODEM_BUFFER_SIZE-1;
  }
  _readModemResponseStarted = false;
}

int VSIM900::setModemStatus(char *eilute){
// jei tai buvo modemo statuso pranesimas - grazinam 1, kitu atveju 0

 if (strcmp(eilute, "OK") == 0)             { modem.status = msOK;    modem.ok=true;  modem.error=false;   }
 else if (strcmp(eilute, "ERROR") == 0)     { modem.status = msERROR; modem.ok=false; modem.error=true;   }
 else if (strcmp(eilute, "RDY") == 0)       { modem.status = msREADY; modem.initialized = false;    }
 else if (strcmp(eilute, "NO CARRIER") == 0){ modem.status = msNOCARIER; modem.dtmftimer = 0; }
 else if (strcmp(eilute, "RING") == 0)      { modem.status = msRING;      }
 else if (strcmp(eilute, "CONNECT") == 0)   { modem.status = msCONNECT;     }
 // NURODYTA ZEMIAU
 //else if (strcmp(eilute, "NORMAL POWER DOWN") == 0)   { modem.status = MODEM_POWER_DOWN;     }
 else if (strcmp(eilute, "+CPIN: READY") == 0)   { modem.status = msPIN_READY; modem.pin_ok=true;    }
 else if (strcmp(eilute, "SERVER OK") == 0)   { modem.status = msSERVER_OK; modem.server_ok=true;    }
 else if (strcmp(eilute, "SERVER CLOSE") == 0)   { modem.status = msSERVER_CLOSE; modem.server_ok=false;    }
 else if (strncmp(eilute, "+CIPRXGET:1",11) == 0)   { modem.status = msDATA_AVAILABLE; modem.data_available=true; modem.datatimer=1; debug(F("+DATA_AVAIL"));   }
 else if (strncmp(eilute, "+CIPRXGET: 3",12) == 0)   {
//     int bytesReceived = 0;
     modem.status = msDATA_RECEIVING_HEX;
//     sscanf(&eilute[11], "%*d,%d,%d",&bytesReceived, &modem.hexBytesAvailable);
     sscanf(eilute+11, "%*d,%*d,%d", &modem.hexBytesAvailable);
//     debug("HEX data..R/A"+String(bytesReceived, DEC)+"/"+String(modem.hexBytesAvailable, DEC) + " left");
//    debugA(eilute);
    return(modem.status);
}
 else if (strcmp(eilute, "+PDP: DEACT") == 0)        { modem.status = msPDP_DEACT; resetModemStates(); debug(F("+PDP_DEACT"));    }
 else if (strcmp(eilute, "NORMAL POWER DOWN") == 0)  { modem.status = msPOWER_DOWN; resetModemStates(); debug(F("+MODEM_POWER_DOWN")); }
 else if (strncmp(eilute, "REMOTE IP:",10) == 0)   { modem.status = msCLIENT_CONNECTED; modem.connected = true; debug(F("+CLIENT_CONNECTED"));    }
 else if (strncmp(eilute, "CONNECT OK",10) == 0)   { modem.status = msCLIENT_CONNECTED; modem.connected = true; debug(F("+CLIENT_CONNECTED [CONNECT OK]"));    }
 else if (strncmp(eilute, "STATE: CONNECT OK",17) == 0)   { modem.status = msCLIENT_ONLINE; modem.connected = true; debug(F("+CLIENT_ONLINE"));    }
 else if (strncmp(eilute, "STATE: SERVER LIS",17) == 0)   { modem.status = msCLIENT_OFFLINE; modem.connected = false; debug(F("+CLIENT_OFFLINE"));    }
 else if (strcmp(eilute, "CLOSED") == 0)   { modem.status = msCLIENT_DISCONNECTED; modem.connected = false; modem.datatimer=0; debug(F("+CLIENT_DISCONNECTED"));    }
 else if (strcmp(eilute, ">") == 0)   { modem.status = ms_READY_TO_SEND; debug(F("Ready to send..")); return(modem.status);   }
 else if (strcmp(eilute, "SEND OK") == 0)   { modem.status = msSEND_OK; modem.canSendNewPacket = true; debug(F("+DATA_SENT"));    }
 else if (strncmp(eilute, "DATA ACCEPT", 11) == 0)   { modem.status = msDATA_ACCEPT; modem.canSendNewPacket = true; debug(F("+DATA_ACCEPTED"));    }
 else if (strncmp(eilute, "+DTMF:", 6) == 0) {
   char dtmf_code = eilute[6];
   modem.status = msDTMF_RECEIVED;
   modem.has_dtmf = true;
   modem.dtmftimer=DTMF_TIMEOUT;
    // # arba * isvalo dtmf eilute, kas reiskia, kad pradedama nauja komanda.
    if ((dtmf_code == '#') || (dtmf_code == '*')) {
 //   	modem.dtmf = String(eilute[6]);
    	modem.dtmfCMD[0] = eilute[6];
    	modem.dtmfCMD[1] = 0;
    }
    else {
      //strcat(modem.dtmfCMD, eilute[6]);
    	uint8_t cmdSize = strlen(modem.dtmfCMD);
    	if (cmdSize < sizeof(modem.dtmfCMD)-1){
    		modem.dtmfCMD[cmdSize] = eilute[6];
    		modem.dtmfCMD[cmdSize+1] = 0;
    	}
//      modem.dtmf = modem.dtmf + String(eilute[6]);
    }
    return(modem.status);
  }
 else if (strncmp(eilute, MODEM_AT_COMMAND_PREFIX, MODEM_AT_CMD_PREFIX_LENGTH) == 0) {
   modem.status = msVALDIKLIS_PREFIX;
   valdiklioKomanda(eilute+3, true);   // komanda valdikiui, siust atsakymus tiesiogiai i terminala
 }
 else if (strncmp(eilute, "+CSQ:", 5) == 0) {
    int rssi;
    modem.status = msOTHER;
    sscanf(&eilute[5], "%i,%i", &modem.rssi, &modem.ber);
    rssi = -113 + (2 * modem.rssi);
    debug("+RSSI:" + String(rssi, DEC) + " BER:" + String(modem.ber, DEC) + " ");
 }
 else if (strncmp(eilute, "+CMTE:", 6) == 0) {
    int detect;
    modem.status = msOTHER;
    sscanf(&eilute[6], "%i,%i", &detect, &modem.temperature);
//    debug("+TEMP: " + String(modem.temperature, DEC) + " 'C");

 }

 else {
   modem.status = msUNKNOWN;
 }

if (modem.status != msUNKNOWN) {
    modem.lastReplyTimestamp = getTimeStamp();
}

return modem.status;

}

void VSIM900::modemStatusChanged(){
	if (modem.status == msNOCARIER){
//	  debug(F("+MODEM:NO_CARRIER"));
      modem.voice_connect_time = 0;
      modem.voice_connected = false;
      modem.voice_activated = false;
//	  #ifdef USE_SOUND_RELAY
//      if (!serviceModeActivated) soundRelay(OFF); // isjungt garsiakalbi, jei neaktyvuotas serviso rezimas
//	  #endif
    }
        onStatusChanged(modem.status);
}

bool VSIM900::setAPN(const String& apn){
	if (apn.length() >= sizeof(_modemAPN)){
		debug(F("setAPN:APN>" STR(MODEM_MAX_APN_LENGTH)));
		return false;
	}
	apn.toCharArray(_modemAPN,sizeof(_modemAPN));
	debug(F("setAPN:OK"));
	return true;
}

void VSIM900::modemAnswer(){
    sendATCommand(F("ATA"));
//	  debug(F("+MODEM:VOICE_ANSWER"));
    modem.voice_connect_time = 0;
    modem.voice_connected = true;
    modem.voice_activated = false;
}

void VSIM900::modemHangUp(){
    modem.voice_connect_time = 0;
    modem.voice_connected = false;
    modem.voice_activated = false;
    sendATCommand(F("ATH"));
}

/*
 * call this once in your sketch's setup()
 */
void VSIM900::setup() {
	//	serialEvent= &this->readModemResponse();   // ar reikia ? loop() skaitys modemo atsakymus
	_serialPort->begin(_baudRate);
	_serialPort->setTimeout(1000);

}

/*
 * call this from you loop()
 */
void VSIM900::loop() {
	unsigned long ms = millis();

	//	call only if time from last call is > 100 ms
	if (ms - _loop_call_time > 100){
		if (_serialPort->available()) {
			readModemResponse();
		}
		_loop_call_time = millis();
	}

	// every 10 sec.
	if (ms - _idle_connection_data_check_time > 10000){

		_idle_connection_data_check_time = millis();
	}




	// timer every second
	if (ms - _one_second_time > 1000){

		if (modem.connected) {
			modem.datatimer++;
			if (modem.hexBytesAvailable > 0)
				modem.data_available = true; // jei buferyje liko nenuskaitytu duomenu..
			if ((modem.datatimer % IDLE_CONNECTION_CHECK_FOR_DATA_SECONDS)
					== 0) {
				modem.data_available = true;
				debug(F("asking for data.."));
			}
		} else {
#ifdef TCP_CLIENT
			modem.reconnect_timer++;
			if (modem.reconnect_timer > TCP_CLIENT_RECONNECT_DELAY) {
				modem.reconnect_timer = 0;
				sendATCommand(F(TCP_CONNECT_COMMAND));
				delay(250);
				sendATCommand(F("AT+CIPQSEND=1"));
				delay(500);
			}
#endif
		}






		_one_second_time = millis();
	}


//    if ((modem.datatimer % IDLE_CONNECTION_CHECK_FOR_DATA_SECONDS) == 0) {
//        modem.data_available=true;
//        debug(F("asking for data.."));
//    }

	if (modem.data_available){
		askForTCPData();
	}




}

void VSIM900::resetModemStates(){
  modem.pin_ok  = false;
  modem.error  = false;
  modem.ok     = false;
  modem.status = msUNKNOWN;
  modem.network_ok = false;
  modem.gprs_ok = false;
  modem.server_ok = false;
  modem.initialized = false;
  modem.data_available= false;
  modem.ip_address[0]=0;
  modem.connected = false;
  modem.voice_connected = false;
  modem.receiving_data = false;
  _readModemResponseStarted = false;
  modem.datatimer=1;
  modem.received_new_packet=false;
  modem.rxpacketsize=0;
  modem.has_dtmf=false;
  modem.dtmfCMD[0] = 0;
  modem.last_packet_timestamp = getTimeStamp();
  modem.sendATResponseToClient = false;
  modem.temperature = 25;
  modem.voice_connect_time = 0;
  modem.hexBytesAvailable=0;
  modem.canSendNewPacket = true;
  modem.reconnect_timer = 25;
  modem.lastReplyTimestamp = 0;
  modem.init_failure_counter=0;
  modem.data_bytes_received=0;
  modem.data_bytes_sent=0;

}

int VSIM900::sendModemDataChar(char *dataToSend, int count=0){
//  char s;
//  int i;
  modem.canSendNewPacket = false;
  if (count >0) {
	//sendb
	  char buffer[20];
	  sprintf_P(buffer, PSTR("AT+CIPSEND=%d\r\n"), count);
	  sendATCommandChar(buffer);
//	  serialPort->write(buffer);
//    String atcmd = "AT+CIPSEND=" + String(count, DEC);
//    sendATCommand((atcmd),100,1,false);
	  _serialPort->write(dataToSend, count);
//    for (i=0; i<count; i++) serialPort->write(dataToSend[i]);
	modem.data_bytes_sent += count;
  }
return(0);
}

int VSIM900::sendModemDataString(const String& dataToSend){
    String atcmd = "AT+CIPSEND=" + String(dataToSend.length(), DEC);
    sendATCommand((atcmd),200,1,false);
    _serialPort->print(dataToSend);
	modem.data_bytes_sent += dataToSend.length();
	debug("SENT:"+dataToSend);
return(0);
}

void VSIM900::askForTCPData(){
	modem.data_available=false;
	sendATCommand(F(COMMAND_ASK_FOR_TCP_DATA));
}


int VSIM900::hexToChar(char *src, char *dst, int count){
  int i=0;
  unsigned int u;
  while (i++ < count && sscanf(src, "%2x", &u) == 1){
    *dst++ = u;
    src += 2;
  }
 // grazinam konvertuotu baitu skaiciu - 1
 i--;
 return(i);
}
