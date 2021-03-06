/*
 * VSIM900.h
 *
 *  Created on: 2017-08-05
 *      Author: Vilius Bilinkevicius
 */

#include <Arduino.h>

#ifndef VSIM900_H_
#define VSIM900_H_

#ifndef MODEM_BUFFER_SIZE
# define MODEM_BUFFER_SIZE 64
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

//setAPN(apn) returns false if apn size > this.
#ifndef MODEM_MAX_APN_LENGTH
# define MODEM_MAX_APN_LENGTH 20
#endif

#define IDLE_CONNECTION_CHECK_FOR_DATA_SECONDS 10


// po tiek nesekmingu bandymu pasikalbeti su modemu (initModem() pradine stadija = ATE0)
// bandome ijungti modema is naujo (low, high, low seka MODEM_SWITCH_PIN)
#define MODEM_SWITCH_POWER_AFTER_FAILURES 10

// drop gprs if TCP connection is inactive for this time (in seconds)
#define MODEM_MAX_GPRS_INACTIVIY_TIME 3600

// jei modemas neinicializuotas, arba atsijungem nuo gprs, ar nepavyko inicializacija, tai cia nurodomas
// uzlaikymo laikas sekundemis, po kurios bus bandoma inicializuot modema is naujo
#define MODEM_RESET_DELAY 20

#define GET_TIMESTAMP (millis() / 1000)

#ifndef COMMAND_PREFIX
# define COMMAND_PREFIX "#!#"
#endif

# define COMMAND_PREFIX_LENGTH sizeof(COMMAND_PREFIX) - 1

#define COMMAND_ASK_FOR_TCP_DATA "AT+CIPRXGET=3,30"

//#define USE_WATCHDOG 1


enum modemError : uint8_t {
	meINIT, mePIN, meNETWORK, meGPRS, meIP, meCIICR, meDTMF, meNO_ERRORS
};

enum modemState : uint8_t {
	msUNKNOWN=0, msOK, msERROR, msNOCARIER, msRING, msCONNECT, msREADY, msPOWER_DOWN, msPIN_READY, msSERVER_OK, msSERVER_CLOSE,
	msDATA_AVAILABLE, msDATA_RECEIVING_HEX, msPDP_DEACT, msCLIENT_CONNECTED, msCLIENT_DISCONNECTED, msCLIENT_ONLINE, msCLIENT_OFFLINE,
	msREADY_TO_SEND, msSEND_OK, msDATA_ACCEPT, msDTMF_RECEIVED, msVALDIKLIS_PREFIX, msINIT_COMPLETED, msCSQ, msCMTE, msSHUT_OK,
	msOTHER
};

//typedef struct GModem GModem;

typedef struct GModem{
	bool pin_ok;
    bool network_ok;
    bool gprs_ok;
    bool server_ok;
    bool initialized;
    bool data_available;
    bool tcp_connected;
    bool voice_connected;
    bool voice_activated;
    bool receiving_data;
    bool received_new_packet;
    bool has_dtmf;
    bool sendATResponseToClient;
    bool canSendNewPacket;
    unsigned int resetTimer;
//    char dtmftimer;
    uint8_t datatimer;
    uint8_t init_failure_counter = 0;
    modemState status;
    char rxpacket[MODEM_BUFFER_SIZE];
    int rssi;
    int ber;
    unsigned int rxpacketsize;
    int temperature;
	int reconnect_timer;
	unsigned int gprsFailures;
	unsigned long last_dataReceivedTimestamp;		// GPRS watchdog. Resetui, jei po nustatyto laiko negaunam jokio paketo
													// neatsizvelgiant i TCP ir PDP busenas.
    unsigned long last_packet_timestamp;
    unsigned long lastReplyTimestamp;
	unsigned long data_bytes_sent;
	unsigned long data_bytes_received;
    unsigned int voice_connect_time;
    unsigned int hexBytesAvailable;
	float voltage;
	char response[MODEM_BUFFER_SIZE+2];
//    String response;
//    String ip_address;
    char ip_address[16];
//    String remote;
//    String dtmf;
//    char dtmfCMD[10];
} GModem;


class VSIM900  {
public:
	VSIM900(HardwareSerial& hwPort, uint32_t baud);
	VSIM900(HardwareSerial &hwPort, uint32_t baud, unsigned int switchPin, unsigned int resetPin);
	virtual ~VSIM900(){}
	void setup();
	void loop();
	void switchModem();
	void resetModem();
	bool setAPN(const String& apn);
	int sendATCommand(const String& cmd, unsigned int dWait=200, char retries=2, bool ignoreErrors=false);
	int sendATCommandChar(char *cmd);
	int sendAtCommandRespondToClient(char *cmd);
	int sendString(const String& dataToSend);
	void sendChar(char *dataToSend, int count=0);
	modemError initModem();
	void dropGPRS();
	void readModemResponse();
	void answerCall();
	void hangUp();
	void askForTCPData();
	void serialEvent();
	uint16_t getTimeStamp() 			 { return GET_TIMESTAMP; 	}
	void setResetPin(uint8_t resetPin) 	 { _reset_pin 	= resetPin;	}
	void setDTMFRequired(bool state)	 { _dtmf_required = state;	}
	void setSwitchPin(uint8_t switchPin) { _switch_pin 	= switchPin;}
	void setDebugCallback(void (* debug)(const String&)){ this->debug__ = debug;}
	void setBlinkCallback(void (* blink)(int, int))		{ this->blink__ = blink;}
	void setDTMFCallback(void (* dtmf)(const char tone)){ this->dtmf__  = dtmf;}
	void setSpecialCommandCallback(void (* specialCommandCallback)(char *commandLine, bool replyToSerial)) { this->specialCommand__ = specialCommandCallback;}
	void setStatusChangeCallback(void (*statusChangeCallback)(modemState status)) {this->onStatusChanged = statusChangeCallback;}
	bool isTCPConnected(){return modem.tcp_connected;};
	bool hasNewData(){return modem.received_new_packet;}
	unsigned int getReceivedPacketSize(){return modem.rxpacketsize;}
	char* getPacket(){return modem.rxpacket;}
	void markPacketReceived();
	GModem   modem;
	HardwareSerial* serialPort;

private:
	unsigned int _baudRate;
	bool _readModemResponseStarted;
	unsigned long _loop_call_time = 0;
	unsigned long _idle_connection_data_check_time = 0;
	unsigned long _one_second_time = 0;
	unsigned int _switch_pin = 0;
	unsigned int _reset_pin  = 0;
	bool	_dtmf_required = false;
	void (*blink__)(int delay, int count) = nullptr;
	void (*debug__)(const String& tekstas) = nullptr;
	void (*dtmf__)(const char tone) = nullptr;
	void (*specialCommand__)(char *commandLine, bool replyToSerial) = nullptr;
	void (*onStatusChanged)(modemState status) = nullptr;

	static VSIM900* _inst;
	char _modemAPN[MODEM_MAX_APN_LENGTH+1];
	char _readBuffer[MODEM_BUFFER_SIZE];
	modemState parseModemStatus(char *eilute);
	modemState setModemStatus(char *eilute);
	void modemStatusChanged();
	void resetModemStates();
	void blinkModemStatus();
	void checkModemHealth();
	void blink(int time_ms=50, int count=1);
	void debug(const String& tekstas);
	void _wdr();
	int hexToChar(char *src, char *dst, int count);

};

#endif /* VSIM900_H_ */
