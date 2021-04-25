/**
 * @file       TinyGsmClientA9.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMCLIENTA9_H_
#define SRC_TINYGSMCLIENTA9_H_
// #pragma message("TinyGSM:  TinyGsmClientA9")

// #define TINY_GSM_DEBUG Serial

#define TINY_GSM_MUX_COUNT 8
#define TINY_GSM_NO_MODEM_BUFFER

#include "TinyGsmBattery.tpp"
#include "TinyGsmCalling.tpp"
#include "TinyGsmGPRS.tpp"
#include "TinyGsmGPS.tpp"
#include "TinyGsmModem.tpp"
#include "TinyGsmSMS.tpp"
#include "TinyGsmTCP.tpp"
#include "TinyGsmTime.tpp"

#define GSM_NL "\r\n"
static const char GSM_OK[] TINY_GSM_PROGMEM    = "OK" GSM_NL;
static const char GSM_ERROR[] TINY_GSM_PROGMEM = "ERROR" GSM_NL;
#if defined       TINY_GSM_DEBUG
static const char GSM_CME_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CME ERROR:";
static const char GSM_CMS_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CMS ERROR:";
#endif

enum RegStatus {
  REG_NO_RESULT    = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK_HOME      = 1,
  REG_OK_ROAMING   = 5,
  REG_UNKNOWN      = 4,
};

class TinyGsmA9 : public TinyGsmModem<TinyGsmA9>,
                  public TinyGsmGPRS<TinyGsmA9>,
                  public TinyGsmTCP<TinyGsmA9, TINY_GSM_MUX_COUNT>,
                  public TinyGsmCalling<TinyGsmA9>,
                  public TinyGsmSMS<TinyGsmA9>,
                  public TinyGsmTime<TinyGsmA9>,
				  public TinyGsmGPS<TinyGsmA9>,
                  public TinyGsmBattery<TinyGsmA9> {
  friend class TinyGsmModem<TinyGsmA9>;
  friend class TinyGsmGPRS<TinyGsmA9>;
  friend class TinyGsmTCP<TinyGsmA9, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmCalling<TinyGsmA9>;
  friend class TinyGsmSMS<TinyGsmA9>;
  friend class TinyGsmTime<TinyGsmA9>;
  friend class TinyGsmGPS<TinyGsmA9>;
  friend class TinyGsmBattery<TinyGsmA9>;

  /*
   * Inner Client
   */
 public:
  class GsmClientA9 : public GsmClient {
    friend class TinyGsmA9;

   public:
    GsmClientA9() {}

    explicit GsmClientA9(TinyGsmA9& modem, uint8_t = 0) {
      init(&modem, -1);
    }

    bool init(TinyGsmA9* modem, uint8_t = 0) {
      this->at       = modem;
      this->mux      = -1;
      sock_connected = false;

      return true;
    }

   public:
    int connect(const char* host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      uint8_t newMux = -1;
      sock_connected = at->modemConnect(host, port, &newMux, timeout_s);
	  DBG("><<<<<<<<<<,,sock_connected= ");DBG(sock_connected);
	  
      if (sock_connected) {
        mux              = newMux;
        at->sockets[mux] = this;
      }
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    void stop(uint32_t maxWaitMs) {
      TINY_GSM_YIELD();
      at->sendAT(GF("+CIPCLOSE="), mux);
      sock_connected = false;
      at->waitResponse(maxWaitMs);
      rx.clear();
    }
    void stop() override {
      stop(1000L);
    }

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
  };

  /*
   * Inner Secure Client
   */

  // Doesn't support SSL

  /*
   * Constructor
   */
 public:
  explicit TinyGsmA9(Stream& stream) : stream(stream) {
    memset(sockets, 0, sizeof(sockets));
  }

  /*
   * Basic functions
   */
 protected:
  bool initImpl(const char* pin = NULL) {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);
    DBG(GF("### TinyGSM Compiled Module:  TinyGsmClientA9"));

    if (!testAT()) { return false; }

    // sendAT(GF("&FZ"));  // Factory + Reset
    // waitResponse();

    sendAT(GF("E0"));  // Echo Off
    if (waitResponse() != 1) { return false; }

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2"));  // turn on verbose error codes
#else
    sendAT(GF("+CMEE=0"));  // turn off error codes
#endif
    waitResponse();
    sendAT(
        GF("+CMER=3,0,0,2"));  // Set unsolicited result code output destination
    waitResponse(20L);

    DBG(GF("### Modem:"), getModemName());

    SimStatus ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != NULL && strlen(pin) > 0) {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    } else {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
  }

  bool factoryDefaultImpl() {
    sendAT(GF("&FZE0&W"));  // Factory + Reset + Echo Off + Write
    waitResponse();
    sendAT(GF("&W"));  // Write configuration
    return waitResponse() == 1;
  }

  /*
   * Power functions
   */
 protected:
  bool restartImpl() {
    if (!testAT()) { return false; }
    sendAT(GF("+RST=1"));
	waitResponse();
    delay(40000);
	String res;
	//if (waitResponse(120000L, res) != 1) { DBG("### restartImpl:res=> error");delay(40000); return init(); }
	DBG("### restartImpl:res=>1 ",res);
	/*if (waitResponse(3000L, res) != 1) { return ""; }
	DBG("### restartImpl:res=>2 ",res);
	if (waitResponse(3000L, res) != 1) { return ""; }
	DBG("### restartImpl:res=>3 ",res);*/
	DBG("### restart ok!!! ");
    return init();
  }

  bool powerOffImpl() {
    sendAT(GF("+CPOF"));
    // +CPOF: MS OFF OK
    return waitResponse() == 1;
  }

  bool sleepEnableImpl(bool enable = true) TINY_GSM_ATTR_NOT_AVAILABLE;

  bool setPhoneFunctionalityImpl(uint8_t fun, bool reset = false)
      TINY_GSM_ATTR_NOT_IMPLEMENTED;

  /*
   * Generic network functions
   */
 public:
  RegStatus getRegistrationStatus() {
    return (RegStatus)getRegistrationStatusXREG("CREG");
  }

 protected:
  bool isNetworkConnectedImpl() {
    RegStatus s = getRegistrationStatus();
    return (s == REG_OK_HOME || s == REG_OK_ROAMING);
  }

  String getLocalIPImpl() {
    sendAT(GF("+CIFSR"));
    String res;
    if (waitResponse(10000L, res) != 1) { return ""; }
    res.replace(GSM_NL "OK" GSM_NL, "");
    res.replace(GSM_NL, "");
    res.trim();
    return res;
  }
  
 public: 
 int8_t getSignalStrengthImpl() {
    
	int8_t int_res;
    sendAT(GF("+CSQ"));
    if (waitResponse(GF(GSM_NL "+CSQ: ")) != 1) { return ""; }
    //streamSkipUntil('"');  // Skip mode and format
    String res = stream.readStringUntil(',');
	int_res = -113+2*res.toInt();
    waitResponse();
    return int_res;
  } /**/

  /*
   * GPRS functions
   */
 protected:
  bool gprsConnectImpl(const char* apn, const char* user = NULL,
                       const char* pwd = NULL) {
    gprsDisconnect();

    sendAT(GF("+CGATT=1"));
    if (waitResponse(60000L) != 1) { return false; }

    // TODO(?): wait AT+CGATT?

    sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, '"');
    waitResponse();
    DBG("### +CGDCONT=1:",apn);

    if (!user) user = "";
    if (!pwd) pwd = "";
    //sendAT(GF("+CSTT=\""), apn, GF("\",\""), user, GF("\",\""), pwd, GF("\""));
    //if (waitResponse(60000L) != 1) { return false; }

//TINY_GSM_DEBUG.print("><<<<<<<<<<  AT+gjhfgjfgjfjfgjf=");
 DBG("### ++CGACT=1,1:");
 sendAT(GF("+CGACT=1,1"));
    waitResponse(60000L);
	
    sendAT(GF("+CIFSR"));
    DBG("### +CIFSR<<");
    if (waitResponse(GF(GSM_NL "+CIFSR:")) != 1) {
	DBG("### +CIFSR: errr");
	sendAT(GF("+CIPSTART=\"TCP\",\"8.8.8.8\",53"));
	waitResponse(10000L);
	/*sendAT(GF("+CIFSR"));
	waitResponse(60000L);
	streamSkipUntil('"');  // Skip mode and format
    String res = stream.readStringUntil('"');
	DBG("### Unhandled1:", res);*/
	//return ""; 
	
	}
    
	 //if (data.length()) { DBG("### Unhandled1:", data); data = "";}
      
    
	
	
   /* waitResponse(60000L);
	stream.flush();*/
DBG("### ---------------------------------------------------");
   DBG("### +CIPMUX:");
    sendAT(GF("+CIPMUX=1"));
    if (waitResponse() != 1) { return false; }
/**/
    return true;
  }

  bool gprsDisconnectImpl() {
    // Shut the TCP/IP connection
    sendAT(GF("+CIPSHUT"));
    if (waitResponse(60000L) != 1) { return false; }

    for (int i = 0; i < 3; i++) {
      sendAT(GF("+CGATT=0"));
      if (waitResponse(5000L) == 1) { return true; }
    }

    return false;
  }

  String getOperatorImpl() {
    sendAT(GF("+COPS=3,0"));  // Set format
    waitResponse();

    sendAT(GF("+COPS?"));
    if (waitResponse(GF(GSM_NL "+COPS:")) != 1) { return ""; }
    streamSkipUntil('"');  // Skip mode and format
    String res = stream.readStringUntil('"');
    waitResponse();
    return res;
  }
  public:
  bool setOperatorImpl( String number) {
    sendAT(GF("+COPS=4,2,\"25503\""));  // Set format
    waitResponse();

    /*sendAT(GF("+COPS?"));
    if (waitResponse(GF(GSM_NL "+COPS:")) != 1) { return ""; }
    streamSkipUntil('"');  // Skip mode and format
    String res = stream.readStringUntil('"');
    waitResponse();*/
    return true;
  }

  /*
   * SIM card functions
   */
 protected:
  String getSimCCIDImpl() {
    sendAT(GF("+CCID"));
    if (waitResponse(GF(GSM_NL "+SCID: SIM Card ID:")) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

  /*
   * Phone Call functions
   */
 protected:
  // Returns true on pick-up, false on error/busy
  bool callNumberImpl(const String& number) {
    if (number == GF("last")) {
      sendAT(GF("DLST"));
    } else {
      sendAT(GF("D\""), number, "\";");
    }

    if (waitResponse(5000L) != 1) { return false; }

    if (waitResponse(60000L, GF(GSM_NL "+CIEV: \"CALL\",1"),
                     GF(GSM_NL "+CIEV: \"CALL\",0"), GFP(GSM_ERROR)) != 1) {
      return false;
    }

    int8_t rsp = waitResponse(60000L, GF(GSM_NL "+CIEV: \"SOUNDER\",0"),
                              GF(GSM_NL "+CIEV: \"CALL\",0"));

    int8_t rsp2 = waitResponse(300L, GF(GSM_NL "BUSY" GSM_NL),
                               GF(GSM_NL "NO ANSWER" GSM_NL));

    return rsp == 1 && rsp2 == 0;
  }

  // 0-9,*,#,A,B,C,D
  bool dtmfSendImpl(char cmd, uint8_t duration_ms = 100) {
    duration_ms = constrain(duration_ms, 100, 1000);

    // The duration parameter is not working, so we simulate it using delay..
    // TODO(?): Maybe there's another way...

    // sendAT(GF("+VTD="), duration_ms / 100);
    // waitResponse();

    sendAT(GF("+VTS="), cmd);
    if (waitResponse(10000L) == 1) {
      delay(duration_ms);
      return true;
    }
    return false;
  }

  /*
   * Audio functions
   */
 public:
  bool audioSetHeadphones() {
    sendAT(GF("+SNFS=0"));
    return waitResponse() == 1;
  }

  bool audioSetSpeaker() {
    sendAT(GF("+SNFS=1"));
    return waitResponse() == 1;
  }

  bool audioMuteMic(bool mute) {
    sendAT(GF("+CMUT="), mute);
    return waitResponse() == 1;
  }

  /*
   * Messaging functions
   */
 protected:
  String sendUSSDImpl(const String& code) {
    sendAT(GF("+CMGF=1"));
    waitResponse();
    sendAT(GF("+CSCS=\"HEX\""));
    waitResponse();
    sendAT(GF("+CUSD=1,\""), code, GF("\",15"));
    if (waitResponse(10000L) != 1) { return ""; }
    if (waitResponse(GF(GSM_NL "+CUSD:")) != 1) { return ""; }
    streamSkipUntil('"');
    String hex = stream.readStringUntil('"');
    streamSkipUntil(',');
    int8_t dcs = streamGetIntBefore('\n');

    if (dcs == 15) {
      return TinyGsmDecodeHex7bit(hex);
    } else if (dcs == 72) {
      return TinyGsmDecodeHex16bit(hex);
    } else {
      return hex;
    }
  }


 /*
   * GSM Location functions
   */
 protected:
  // NOTE:  As of application firmware version 01.016.01.016 triangulated
  // locations can be obtained via the QuecLocator service and accompanying AT
  // commands.  As this is a separate paid service which I do not have access
  // to, I am not implementing it here.

  /*
   * GPS/GNSS/GLONASS location functions
   */
 protected:
  // enable GPS
  bool enableGPSImpl() {
    sendAT(GF("+GPS=1"));
    if (waitResponse() != 1) { return false; }
	sendAT(GF("+GPSMD=1"));
    if (waitResponse() != 1) { return false; }	
	sendAT(GF("+GPSRD =1"));
    if (waitResponse() != 1) { return false; }
    return true;
  }

  bool disableGPSImpl() {
    sendAT(GF("+GPS=0"));
    if (waitResponse() != 1) { return false; }
    return true;
  }

  // get the RAW GPS output
  String getGPSrawImpl() {
    //sendAT(GF("+LOCATION=2"));
    if (waitResponse(20000L, GF("+GPSRD:")) != 1) { return ""; }
	//if (waitResponse(10000L, GF("$GPGGA,")) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

  // get GPS informations
  bool getGPSImpl(float* lat, float* lon, float* speed = 0, float* alt = 0,
                  int* vsat = 0, int* usat = 0, float* accuracy = 0,
                  int* year = 0, int* month = 0, int* day = 0, int* hour = 0,
                  int* minute = 0, int* second = 0) {
	DBG("###GPSImpl:");				  
    //sendAT(GF("+LOCATION=2"));
    if (waitResponse(20000L, GF("$GPGGA,")) != 1) {
      // NOTE:  Will return an error if the position isn't fixed
      return false;
    }

    // init variables
    float ilat         = 0;
    float ilon         = 0;
    //float ispeed       = 0;
    float ialt         = 0;
    int   iusat        = 0;
    float iaccuracy    = 0;
    //int   iyear        = 0;
    //int   imonth       = 0;
    //int   iday         = 0;
    int   ihour        = 0;
    int   imin         = 0;
    float secondWithSS = 0;
//Serial.println("><<<<<<<<<<  AT+="+String(ilat));
	//$GPGGA,141539.000,5014.6138,N,02337.7610,E,2,5,1.73,245.3,M,37.8,M,,*51
	//		,time,Latitude,direction,Longitude,direction,quality,Number satellites,Horizontal precision,Antenna altitude,M = meters,undulation,M = meters,,Check sum
    // UTC date & Time
    ihour        = streamGetIntLength(2);      // Two digit hour
    imin         = streamGetIntLength(2);      // Two digit minute
    secondWithSS = streamGetFloatBefore(',');  // 6 digit second with subseconds

    ilat      = streamGetFloatBefore(',');  // Latitude
	streamSkipUntil(',');  
    ilon      = streamGetFloatBefore(',');  // Longitude
	streamSkipUntil(',');  //direction
	streamSkipUntil(',');  //quality
	iusat = streamGetIntBefore(',');  // Number of satellites,
    iaccuracy = streamGetFloatBefore(',');  // Horizontal precision
    ialt      = streamGetFloatBefore(',');  // Altitude from sea level
    
	/*streamSkipUntil(',');                   // GNSS positioning mode
    streamSkipUntil(',');  // Course Over Ground based on true north
    streamSkipUntil(',');  // Speed Over Ground in Km/h
    ispeed = streamGetFloatBefore(',');  // Speed Over Ground in knots

    iday   = streamGetIntLength(2);    // Two digit day
    imonth = streamGetIntLength(2);    // Two digit month
    iyear  = streamGetIntBefore(',');  // Two digit year
*/
    
    streamSkipUntil('\n');  // The error code of the operation. If it is not
                            // 0, it is the type of error.
     /*Serial.println("><<<<<<<<<<  lat+="+String(ilat));
	 Serial.println("><<<<<<<<<<  lon+="+String(ilon));*/
    // Set pointers
    if (lat != NULL) *lat = ilat;
    if (lon != NULL) *lon = ilon;
    if (speed != NULL) *speed = 0;
    if (alt != NULL) *alt = ialt;
    if (vsat != NULL) *vsat = 0;
    if (usat != NULL) *usat = iusat;
    if (accuracy != NULL) *accuracy = iaccuracy;
    //if (iyear < 2000) iyear += 2000;
    if (year != NULL) *year = 0;
    if (month != NULL) *month = 0;
    if (day != NULL) *day = 0;
    if (hour != NULL) *hour = ihour;
    if (minute != NULL) *minute = imin;
    if (second != NULL) *second = static_cast<int>(secondWithSS);

    waitResponse();  // Final OK
    return true;
  }

  /*
   * Time functions
   */
 protected:
  String getGSMDateTimeImpl(TinyGSMDateTimeFormat format) {
    sendAT(GF("+QLTS=2"));
    if (waitResponse(2000L, GF("+QLTS: \"")) != 1) { return ""; }

    String res;

    switch (format) {
      case DATE_FULL: res = stream.readStringUntil('"'); break;
      case DATE_TIME:
        streamSkipUntil(',');
        res = stream.readStringUntil('"');
        break;
      case DATE_DATE: res = stream.readStringUntil(','); break;
    }
    waitResponse();  // Ends with OK
    return res;
  }

  // The BG96 returns UTC time instead of local time as other modules do in
  // response to CCLK, so we're using QLTS where we can specifically request
  // local time.
  bool getNetworkTimeImpl(int* year, int* month, int* day, int* hour,
                          int* minute, int* second, float* timezone) {
    sendAT(GF("+QLTS=2"));
    if (waitResponse(2000L, GF("+QLTS: \"")) != 1) { return false; }

    int iyear     = 0;
    int imonth    = 0;
    int iday      = 0;
    int ihour     = 0;
    int imin      = 0;
    int isec      = 0;
    int itimezone = 0;

    // Date & Time
    iyear       = streamGetIntBefore('/');
    imonth      = streamGetIntBefore('/');
    iday        = streamGetIntBefore(',');
    ihour       = streamGetIntBefore(':');
    imin        = streamGetIntBefore(':');
    isec        = streamGetIntLength(2);
    char tzSign = stream.read();
    itimezone   = streamGetIntBefore(',');
    if (tzSign == '-') { itimezone = itimezone * -1; }
    streamSkipUntil('\n');  // DST flag

    // Set pointers
    if (iyear < 2000) iyear += 2000;
    if (year != NULL) *year = iyear;
    if (month != NULL) *month = imonth;
    if (day != NULL) *day = iday;
    if (hour != NULL) *hour = ihour;
    if (minute != NULL) *minute = imin;
    if (second != NULL) *second = isec;
    if (timezone != NULL) *timezone = static_cast<float>(itimezone) / 4.0;

    // Final OK
    waitResponse();  // Ends with OK
    return true;
  }




  /*
   * Time functions
   */
 protected:
  // Can follow the standard CCLK function in the template
  // Note - the clock probably has to be set manaually first

  /*
   * Battery functions
   */
 protected:
  uint16_t getBattVoltageImpl() TINY_GSM_ATTR_NOT_AVAILABLE;

  // Needs a '?' after CBC, unlike most
  int8_t getBattPercentImpl() {
    sendAT(GF("+CBC?"));
    if (waitResponse(GF(GSM_NL "+CBC:")) != 1) { return false; }
    streamSkipUntil(',');  // Skip battery charge status
    // Read battery charge level
    int8_t res = streamGetIntBefore('\n');
    // Wait for final OK
    waitResponse();
    return res;
  }

  // Needs a '?' after CBC, unlike most
  bool getBattStatsImpl(uint8_t& chargeState, int8_t& percent,
                        uint16_t& milliVolts) {
    sendAT(GF("+CBC?"));
    if (waitResponse(GF(GSM_NL "+CBC:")) != 1) { return false; }
    chargeState = streamGetIntBefore(',');
    percent     = streamGetIntBefore('\n');
    milliVolts  = 0;
    // Wait for final OK
    waitResponse();
    return true;
  }

  /*
   * Client related functions
   */
 protected:
  bool modemConnect(const char* host, uint16_t port, uint8_t* mux,
                    int timeout_s = 75) {
    uint32_t startMillis = millis();
    uint32_t timeout_ms  = ((uint32_t)timeout_s) * 1000;
	
	waitResponse(1000);


    sendAT(GF("+CIPSTART="), GF("\"TCP"), GF("\",\""), host, GF("\","), port);
	//DBG("><<<<<<<<<<  AT+CIPSTART= ",host," port:",port);
    if (waitResponse(timeout_ms, GF("+CIPNUM:")) != 1) {
		//DBG("><<<<<<<<<<  CIPNUM= error");
		return false; 
		
		}/**/
    int8_t newMux = streamGetIntBefore('\n');

    int8_t rsp = waitResponse(
        (timeout_ms - (millis() - startMillis)), GF("CONNECT OK" GSM_NL),
        GF("CONNECT FAIL" GSM_NL), GF("ALREAY CONNECT" GSM_NL));
		
	//DBG("><<<<<<<<<<  waitResponse=");	DBG(rsp);
    if (waitResponse() != 1) { return false; }
    *mux = newMux;

    return (1 == rsp);
  }

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
	  //DBG("### modemSend:", len);
    sendAT(GF("+CIPSEND="), mux, ',', (uint16_t)len);
    if (waitResponse(2000L, GF(GSM_NL ">")) != 1) { return 0; }
    stream.write(reinterpret_cast<const uint8_t*>(buff), len);
    stream.flush();
    if (waitResponse(10000L, GFP(GSM_OK), GF(GSM_NL "FAIL")) != 1) { return 0; }
    return len;
  }

  bool modemGetConnected(uint8_t) {
	  //DBG("### modemGetConnected:");
    sendAT(GF("+CIPSTATUS"));  // TODO(?) mux?
    int8_t res = waitResponse(GF(",\"CONNECTED\""), GF(",\"CLOSED\""),
                              GF(",\"CLOSING\""), GF(",\"INITIAL\""));
    waitResponse();
    return 1 == res;
  }

  /*
   * Utilities
   */
 public:
  // TODO(vshymanskyy): Optimize this!
  int8_t waitResponse(uint32_t timeout_ms, String& data,
                      GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    /*String r1s(r1);// r1s.trim();
    String r2s(r2); r2s.trim();
    String r3s(r3); r3s.trim();
    String r4s(r4); r4s.trim();
    String r5s(r5); r5s.trim();
    DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s);
	DBG("### waitResponse A9:",r1s,"<",r1s.length());*/
    
	DBG("##waitResponse:");
	
    data.reserve(64);
    uint8_t  index       = 0;
    uint32_t startMillis = millis();
    do {
      TINY_GSM_YIELD();
      while (stream.available() > 0) {
        TINY_GSM_YIELD();
        int8_t a = stream.read();
        if (a <= 0) continue;  // Skip 0x00 bytes, just in case
        data += static_cast<char>(a);
		//if (data.length()>60) data=data.substring(data.indexOf('\n'));
		//DBG("### data1 ",data);		if (data.equals(r1)) 	DBG("### waitResp");
		//Serial.println(data);	
        if (r1 && data.endsWith(r1)) {
          index = 1;
          goto finish;
        } else if (r2 && data.endsWith(r2)) {
          index = 2;
          goto finish;
        } else if (r3 && data.endsWith(r3)) {
#if defined TINY_GSM_DEBUG
          if (r3 == GFP(GSM_CME_ERROR)) {
            streamSkipUntil('\n');  // Read out the error
          }
#endif
          index = 3;
          goto finish;
        } else if (r4 && data.endsWith(r4)) {
          index = 4;
          goto finish;
        } else if (r5 && data.endsWith(r5)) {
          index = 5;
          goto finish;
		} else if (data.endsWith("+CIPRCV,")) {
			/*
			### waitResponse A9:
[114377] ### index 0
[114377] ### data 
+CIPRCV,0,5:

[114377] ### Unhandled: +CIPRCV,0,5:
[114378] ### waitResponse A9:
[114478] ### index 0
[114478] ### data 
			*/
          int8_t  mux      = streamGetIntBefore(',');
          int16_t len      = streamGetIntBefore(':');
          int16_t len_orig = len;
        if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            if (len > sockets[mux]->rx.free()) {
              DBG("### Buffer overflow: ", len, "->", sockets[mux]->rx.free());
            } else {
              DBG("### Got: ", len, "->", sockets[mux]->rx.free());
            }
            while (len--) { moveCharFromStreamToFifo(mux); }
            // TODO(?) Deal with missing characters
            if (len_orig > sockets[mux]->available()) {
              DBG("### Fewer characters received than expected: ",
                  sockets[mux]->available(), " vs ", len_orig);
            }
          }
          data = "";
        } else if (data.endsWith(GF("+TCPCLOSED:"))) {
          int8_t mux = streamGetIntBefore('\n');
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->sock_connected = false;
          }
          data = "";
          DBG("### Closed: ", mux);
        }
      }
    } while (millis() - startMillis < timeout_ms);
  finish:
	DBG("### in",index);
	//DBG("### data",data);
    if (!index) {
      data.trim();
      if (data.length()) { DBG("### Unhandled:", data); }
      data = "";
    }
    // data.replace(GSM_NL, "/");
    // DBG('<', index, '>', data);
    return index;
  }

  int8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
  }

  int8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    return waitResponse(1000, r1, r2, r3, r4, r5);
  }

 public:
  Stream& stream;

 protected:
  GsmClientA9* sockets[TINY_GSM_MUX_COUNT];
  const char*  gsmNL = GSM_NL;
};

#endif  // SRC_TINYGsmClientA9_H_
