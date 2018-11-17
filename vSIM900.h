/*
 * VSIM900.h
 *
 *  Created on: 2017-08-05
 *      Author: vilisas
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

// jei modemas neinicializuotas, arba atsijungem nuo gprs, ar nepavyko inicializacija, tai cia nurodomas
// uzlaikymo laikas sekundemis, po kurios bus bandoma inicializuot modema is naujo
#define MODEM_RESET_DELAY 20


#ifndef GPRS_APN
# define GPRS_APN "\"internet\""
#endif

#define GET_TIMESTAMP (millis() / 1000)
#define DTMF_TIMEOUT 5
#define MODEM_AT_COMMAND_PREFIX "#!#"
#define MODEM_AT_CMD_PREFIX_LENGTH 3

#define COMMAND_ASK_FOR_TCP_DATA "AT+CIPRXGET=3,30"

#define USE_WATCHDOG 1


enum modemError{
	meINIT, mePIN, meNETWORK, meGPRS, meIP, meCIICR, meDTMF
};

enum modemState {
	msUNKNOWN, msOK, msERROR, msNOCARIER, msRING, msCONNECT, msREADY, msPOWER_DOWN, msPIN_READY, msSERVER_OK, msSERVER_CLOSE,
	msDATA_AVAILABLE, msDATA_RECEIVING_HEX, msPDP_DEACT, msCLIENT_CONNECTED, msCLIENT_DISCONNECTED, msCLIENT_ONLINE, msCLIENT_OFFLINE,
	ms_READY_TO_SEND, msSEND_OK, msDATA_ACCEPT, msDTMF_RECEIVED, msVALDIKLIS_PREFIX, msINIT_COMPLETED,
	msOTHER
};

typedef struct GModem GModem;

struct GModem{
    bool error;
    bool ok;
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
    char dtmftimer;
    char datatimer;
    char init_failure_counter;
//    char status;
    modemState status;
    char rxpacket[MODEM_BUFFER_SIZE];
    int rssi;
    int ber;
    int rxpacketsize;
    int temperature;
	int reconnect_timer;
	int soundRelayLaikotarpis;
	unsigned int gprsFailures;
	unsigned long last_dataReceivedTimestamp;		// GPRS watchdog. Resetui, jei po nustatyto laiko negaunam jokio paketo
													// neatsizvelgiant i TCP ir PDP busenas.
    unsigned long last_packet_timestamp;
    unsigned long lastReplyTimestamp;
	unsigned long data_bytes_sent;
	unsigned long data_bytes_received;
    unsigned int voice_connect_time;
    uint16_t hexBytesAvailable;
	float voltage;
	char response[MODEM_BUFFER_SIZE+2];
//    String response;
//    String ip_address;
    char ip_address[16];
//    String remote;
//    String dtmf;
    char dtmfCMD[10];
};


class VSIM900  {
public:
	VSIM900(HardwareSerial& hwPort, uint32_t baud);
	virtual ~VSIM900();
	void setup();
	void loop();
	void switchModem();
	void resetModem();
	int sendATCommand(const String& cmd, unsigned int dWait=200, char retries=2, bool ignoreErrors=false);
	int sendATCommandChar(char *cmd);
	int sendModemDataChar(char *dataToSend, int count=0);
	bool setAPN(const String& apn);
	void (*debug)(const String& tekstas) = nullptr;
	void (*valdiklioKomanda)(char *commandLine, bool toSerial = false) = nullptr;
	void (*onStatusChanged)(modemState status) = nullptr;
	void (*blink)(int delay, int count) = nullptr;

	int hexToChar(char *src, char *dst, int count);
	int sendModemDataString(const String& dataToSend);
	int initModem();
	void readModemResponse();
	void modemAnswer();
	void modemHangUp();
	void askForTCPData();
	void serialEvent();
	uint16_t getTimeStamp() 			 { return GET_TIMESTAMP; 	}
	void setResetPin(uint8_t resetPin) 	 { _reset_pin 	= resetPin;	}
	void setSwitchPin(uint8_t switchPin) { _switch_pin 	= switchPin;}
	void setDTMFRequired(bool state)	 { _dtmf_required = state;	}
	GModem   modem;

private:
	HardwareSerial* _serialPort;
	int _baudRate;
	bool _readModemResponseStarted;
	unsigned long _loop_call_time;
	unsigned long _idle_connection_data_check_time;
	unsigned long _one_second_time;
	uint8_t _switch_pin;
	uint8_t _reset_pin;
	bool	_dtmf_required = false;

	static VSIM900* _inst;
	char _modemAPN[MODEM_MAX_APN_LENGTH+1];
	char _readBuffer[MODEM_BUFFER_SIZE];
	int setModemStatus(char *eilute);
	void modemStatusChanged();
	void resetModemStates();
	void blinkModemStatus();
	void doModemHealtCheck();
	void _blink(int time_ms=50, int count=1);
	void _debug(const String& tekstas);
	void _wdr();

};

#endif /* VSIM900_H_ */
