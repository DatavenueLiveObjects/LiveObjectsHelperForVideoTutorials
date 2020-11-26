/*
 * Software Name : Live Objects Helper / CellId Geolocation
 * Version: 1.0
 * SPDX-FileCopyrightText: Copyright (c) 2020 Orange Business Services
 * SPDX-License-Identifier: BSD-3-Clause
 * This software is distributed under the BSD-3-Clause,
 * the text of which is available at https://opensource.org/licenses/BSD-3-Clause
 * or see the "LICENCE" file for more details.
 * Software description: Resources to help using Orange Live Objects CellID-Geolocation
 */

/******************************************************************************
   INCLUDES
 ******************************************************************************/

/******************************************************************************
   USER VARIABLES
 ******************************************************************************/
String cellCurrent, cellEnvironment = "";

/******************************************************************************
   USER PROGRAM
 ******************************************************************************/
void checkSIM() {
  GSMPIN PINManager;
  // MODEM.debug(); // optional debug mode
  PINManager.begin();

  switch(PINManager.isPIN()) {
  case 1:
    Serial.println("SIM detected");
    break;
  case -1:
    Serial.println("PIN locked. PUK needed");
    while (true);
  case -2:
    Serial.println("SIM not ready, or PIN and PUK locked");
    while (true);
  default:
    Serial.println("SIM ready");
  }
}

/*
 * Getting all possible cell information, to be called before attaching any network.
 * Takes 2 minutes to scan the network.
 * 
 * Can and should be replaced by a passive and continuous scan by the modem, so that
 * it takes no active time deep scanning.
 */
void getCellEnvironment() {
  if (cellEnvironment.length() > 0) {
    // AT+COPS=6 is an active deep scan, too slow to run multiple times. Here, we consider that
    // the device is static since its startup time
    return;
  }
  
  do {
    delay(500);
    MODEM.send("AT+COPS=6");
  } while (MODEM.waitForResponse(180000, &cellEnvironment) == 2);
  Serial.print("[DEBUG] Deep cell scan done, lines=");
  Serial.println(countLines(cellEnvironment));
}

/*
 * Gets current serving cell information
 */
void getCellCurrent() {
  MODEM.send("AT+UCELLINFO?");
  MODEM.waitForResponse(3000, &cellCurrent);
  Serial.print("[DEBUG] Serving cell info, lines=");
  Serial.println(countLines(cellCurrent));
}

/*
 * Basic utility to get a line from the input, starting at least at "start" index.
 * Trims lines. Skips empty lines. Returns "" at the end of input.
 * Updates start parameter to allow for direct subsequent calls.
 */
String getLine(String input, int& start) {
  int end = -1;
  for(;start < input.length();) {
    end = input.indexOf('\n', start);
    if (end == -1)
      end = input.length();
    String line = input.substring(start, end);
    line.trim();
    start = end + 1;
    if (line.length() > 0)
      return line;
  }
  return "";
}

int countLines(String input) {
  int nbLines=1;
  for (int i=0 ; i<input.length() -1 ; i++) { // -1 : skip trailing newline
    if (input.charAt(i) == '\n')
      nbLines++;
  }
  return nbLines;
}


/*
 * Fills in the LO JSON parameter the information from the serving cell, if available.
 * Returns success status
 */
bool parseCellServingJson(JsonObject& serving) {
  int locMNC, locMCC, locRXLEV;
  long unsigned int locLAC, locCI;
  String locRAN;

  if (parseCellServingInternal(locMCC, locMNC, locLAC, locCI, locRXLEV, locRAN)) {
    serving["mcc"] = locMCC;
    serving["mnc"] = locMNC;
    serving["lac"] = locLAC;
    serving["cid"] = locCI;
    serving["rxLev"] = locRXLEV;
    serving["ran"] = locRAN;
    return true;
  }
  return false;
}

/*
 * Fills in a flat LO structure the information from the serving cell, if available.
 * Returns success status
 */
bool parseCellServingFlat() {
  int locMNC, locMCC, locRXLEV;
  long unsigned int locLAC, locCI;
  String locRAN;

  if (parseCellServingInternal(locMCC, locMNC, locLAC, locCI, locRXLEV, locRAN)) {
    lo.addToPayload(locMCC);
    lo.addToPayload(locMNC);
    lo.addToPayload(locLAC);
    lo.addToPayload(locCI);
    lo.addToPayload(locRXLEV);
    lo.addToPayload(locRAN);
    return true;
  }
  return false;
}

/*
 * parsing AT+UCELLINFO? answer into split variables
 * 2G: +UCELLINFO: <mode>,<type>,<MCC>,<MNC>,<LAC>,<CI>,<RxLev>[,<t_adv>[,<ch_type>,<ch_mode>]]
 * 3G: +UCELLINFO: <mode>,<type>,<MCC>,<MNC>,<LAC>,<CI>,<scrambling_code>,<dl_frequency>,<rscp_lev>,<ecn0_lev>[,<rrc_state>]
 */
bool parseCellServingInternal(int &locMCC, int &locMNC, long unsigned int &locLAC, long unsigned int &locCI, int &locRXLEV, String &locRAN) {
  int currentIndex = 0;
  String line = getLine(cellCurrent, currentIndex);

  //Serial.print("[DEBUG] Parsing ");
  //Serial.println(line);

  int indexCommaMode = line.indexOf(',');
  int indexCommaType = line.indexOf(',', indexCommaMode+1);
  int type = line.substring(indexCommaMode+1, indexCommaType).toInt();
  if (type <= 1)
    locRAN = "2g";
  else if (type <= 4)
    locRAN = "3g";
  else
    locRAN = "4g";

  int indexCommaMcc = line.indexOf(',', indexCommaType+1);
  locMCC = line.substring(indexCommaType+1, indexCommaMcc).toInt();

  int indexCommaMnc = line.indexOf(',', indexCommaMcc+1);
  locMNC = line.substring(indexCommaMcc+1, indexCommaMnc).toInt();

  int indexCommaLac = line.indexOf(',', indexCommaMnc+1);
  locLAC = strtoul(line.substring(indexCommaMnc+1, indexCommaLac).c_str(), 0, 16);

  if (locLAC == 0xffff)
    return false;

  int indexCommaCi = line.indexOf(',', indexCommaLac+1);
  locCI = strtoul(line.substring(indexCommaLac+1, indexCommaCi).c_str(), 0, 16);

  if (locRAN == "2g") { // GSM case
    int indexCommaRx = line.indexOf(',', indexCommaCi+1);
    if (indexCommaRx == -1)
      indexCommaRx = line.length();
    int rx = line.substring(indexCommaCi+1, indexCommaRx).toInt();
    if (rx == 0)
      locRXLEV = -130;
    else if (rx < 63)
      locRXLEV = -111 + rx;
    else
      locRXLEV = -48;
  }
  else { // UMTS case
    int indexCommaScr = line.indexOf(',', indexCommaCi+1);
    int indexCommaDl = line.indexOf(',', indexCommaScr+1);
    int indexCommaRscp = line.indexOf(',', indexCommaDl+1);
    int rscp = line.substring(indexCommaDl+1, indexCommaRscp).toInt();
    if (rscp == 0)
      locRXLEV = -130;
    else if (rscp < 91)
      locRXLEV = -116 + rscp;
    else
      locRXLEV = -25;
  }
  
  Serial.print("[DEBUG] Got serving cell ");
  Serial.print(locMCC);
  Serial.print("-");
  Serial.print(locMNC);
  Serial.print(" LAC=");
  Serial.print(locLAC);
  Serial.print(" CI=");
  Serial.print(locCI);
  Serial.print(" RAN=");
  Serial.print(locRAN);
  Serial.print(" RXLEV=");
  Serial.print(locRXLEV);
  Serial.println(".");

  return true;    
}

/*
 * parsing AT+COPS=6 answer into Live Objects format
 * GSM  : [<AcT>,]<MCC>,<MNC>,<LAC>,      <CI>,<BSIC>,<arfcn>,<RxLev>
 * UMTS :         <MCC>,<MNC>,<LAC>,<RAC>,<CI>,<dl_frequency>,<ul_frequency>,<scrambling_code>,<RSCP LEV>,<ecn0_lev>
 * RxLev : 0: less than -110 dBm, 1..62: from -110 to less than -48 dBm with 1 dBm steps, 63: -48 dBm or greater
 * RscpLev : 0: less than -115 dBm, 1..90: from -115 dBm to less than -25 dBm with 1 dBm steps, 91: -25 dBm
 */
void parseCellEnvironment(JsonArray& neighbours, JsonObject serving, int maxNeighbours, int restrictMcc, int restrictMnc) {
  int currentIndex = 0;
  String line;
  while ((line = getLine(cellEnvironment, currentIndex)).length() > 0 && maxNeighbours > 0) {
    int locMNC, locMCC, locRXLEV;
    long unsigned int locLAC, locCI;
    String locRAN = "3g"; // 3G is the default value when no AcT field available

    //Serial.print("[DEBUG] Parsing ");
    //Serial.println(line);

    int nbCommas = 0;
    for (int i = 0; i<line.length() ; i++) {
      if (line.charAt(i) == ',')
        nbCommas++;
    }
    
    int indexCommaAct = line.indexOf(',');
    if (indexCommaAct == 2) {
      // optional first field : AcT in case of GSM or LTE network
      int act = line.substring(0, indexCommaAct).toInt();
      if (act <= 3)
        locRAN = "2g";
      else {
        Serial.print("Inconsistent AT response: ");
        Serial.println(line);
        continue;
      }
    }
    else {
      indexCommaAct = -1;
      if (nbCommas == 6)
        locRAN = "2g";
    }
    
    int indexCommaMcc = line.indexOf(',', indexCommaAct+1);
    locMCC = line.substring(indexCommaAct+1, indexCommaMcc).toInt();

    int indexCommaMnc = line.indexOf(',', indexCommaMcc+1);
    locMNC = line.substring(indexCommaMcc+1, indexCommaMnc).toInt();

    int indexCommaLac = line.indexOf(',', indexCommaMnc+1);
    locLAC = strtoul(line.substring(indexCommaMnc+1, indexCommaLac).c_str(), 0, 16);

    int indexCommaRac = indexCommaLac;
    if (locRAN == "3g") // UMTS case
      indexCommaRac = line.indexOf(',', indexCommaLac+1);

    int indexCommaCi = line.indexOf(',', indexCommaRac+1);
    locCI = strtoul(line.substring(indexCommaRac+1, indexCommaCi).c_str(), 0, 16);

    if (locRAN == "3g") { // UMTS case
      int indexCommaDl = line.indexOf(',', indexCommaCi+1);
      int indexCommaUl = line.indexOf(',', indexCommaDl+1);
      int indexCommaScr = line.indexOf(',', indexCommaUl+1);
      int indexCommaRscp = line.indexOf(',', indexCommaScr+1);
      int rscp = line.substring(indexCommaScr+1, indexCommaRscp).toInt();
      if (rscp == 0)
        locRXLEV = -130;
      else if (rscp < 91)
        locRXLEV = -116 + rscp;
      else
        locRXLEV = -25;
    }
    else { // GSM case
      int indexCommaBsic = line.indexOf(',', indexCommaCi+1);
      int indexCommaArf = line.indexOf(',', indexCommaBsic+1);
      int rx = line.substring(indexCommaArf+1).toInt();
      if (rx == 0)
        locRXLEV = -130;
      else if (rx < 63)
        locRXLEV = -111 + rx;
      else
        locRXLEV = -48;
    }
    
    Serial.print("[DEBUG] Got neighbour cell ");
    Serial.print(locMCC);
    Serial.print("-");
    Serial.print(locMNC);
    Serial.print(" LAC=");
    Serial.print(locLAC);
    Serial.print(" CI=");
    Serial.print(locCI);
    Serial.print(" RAN=");
    Serial.print(locRAN);
    Serial.print(" RXLEV=");
    Serial.print(locRXLEV);
    Serial.print(" : ");

    if (serving["mnc"] != locMNC || serving["mcc"] != locMCC || serving["lac"] != locLAC || serving["cid"] != locCI) {
      if (restrictMcc <= 0 || restrictMnc <= 0 || (locMNC == restrictMnc && locMCC == restrictMcc)) {
        JsonObject obj = neighbours.createNestedObject();
        obj["ran"] = locRAN;
        obj["cid"] = locCI;
        obj["lac"] = locLAC;
        obj["mcc"] = locMCC;
        obj["mnc"] = locMNC;
        obj["rxLev"] = locRXLEV;
        maxNeighbours--;
        Serial.println("set in location frame");
      }
      else
        Serial.println("MCC/MNC filtered");
    }
    else
      Serial.println("ignored (serving cell)");
  }
}

/*
 * Adds infos from serving and neighbour cell towers into LiveObjects payload.
 */
void addCellInfoToPayload(int nbNeighbours) {
  getCellCurrent();
  if (nbNeighbours > 0)
    getCellEnvironment();
  DynamicJsonDocument cellInfo = DynamicJsonDocument(5*1024);
  JsonObject serving = cellInfo.createNestedObject("serving");
  if (parseCellServingJson(serving)) {
    if (nbNeighbours > 0) {
      JsonArray neighbours = cellInfo.createNestedArray("neighbours");
      parseCellEnvironment(neighbours, serving, nbNeighbours, -1, -1);
    }
    JsonObject json = cellInfo.as<JsonObject>();
    lo.addObjectToPayload("cellularInfo", json);
  }    
}

/*
 * Adds infos from serving cell towers into LiveObjects payload.
 * To be used with a LO CSV decoder :
 * columns: [{"name": "uptime", "jsonType": "NUMERIC"},{"name": "mcc", "jsonType": "NUMERIC"},{"name": "mnc", "jsonType": "NUMERIC"},
 *          {"name": "lac", "jsonType": "NUMERIC"},{"name": "ci", "jsonType": "NUMERIC"},{"name": "rx", "jsonType": "NUMERIC"},{"name": "ran", "jsonType": "STRING"}]
 * options: {"columnSeparator": ";"}
 * template: {"cellularInfo": { "serving": {"mnc": {{mnc}}, "rxLev": {{rx}}, "mcc": {{mcc}}, "ran": {{ran}}, "cid": {{ci}}, "lac": {{lac}} } },
 *           "uptime": {{uptime}} }
 */
void addCellInfoToSMS() {
  getCellCurrent();
  parseCellServingFlat();
}
