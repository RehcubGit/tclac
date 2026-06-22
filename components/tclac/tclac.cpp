/**
* Erstellt von Miguel Ángel López am 20.07.19
* und geändert von xaxexa
* Refactoring und Komponentenerstellung:
* Соловей с паяльником 15.03.2024
**/
#include "esphome.h"
#include "esphome/core/defines.h"
#include "tclac.h"

namespace esphome{
namespace tclac{


ClimateTraits tclacClimate::traits() {
	auto traits = climate::ClimateTraits();
	traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
	
	// Hiermit erkläre ich verantwortungsvoll, dass ich das alles von christoph5180 übernommen habe
	if (this->supported_modes_.empty()) {
		traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
		traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);
	} else {
		for (auto mode : this->supported_modes_)
			traits.add_supported_mode(mode);
	}
	if (this->supported_presets_.empty()) {
		traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);
	} else {
		for (auto preset : this->supported_presets_)
			traits.add_supported_preset(preset);
	}
	if (this->supported_fan_modes_.empty()) {
		traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
	} else {
		for (auto fan_mode : this->supported_fan_modes_)
			traits.add_supported_fan_mode(fan_mode);
	}
	if (this->supported_swing_modes_.empty()) {
		traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
	} else {
		for (auto swing_mode : this->supported_swing_modes_)
			traits.add_supported_swing_mode(swing_mode);
	}

	return traits;
}


void tclacClimate::setup() {

#ifdef CONF_RX_LED
	this->rx_led_pin_->setup();
	this->rx_led_pin_->digital_write(false);
#endif
#ifdef CONF_TX_LED
	this->tx_led_pin_->setup();
	this->tx_led_pin_->digital_write(false);
#endif
}

void tclacClimate::loop()  {
	// Wenn sich etwas im UART-Puffer befindet, lesen wir es aus
	if (esphome::uart::UARTDevice::available() <= 0) 
		return;

	dataShow(0, true);

	dataRX[0] = esphome::uart::UARTDevice::read();
	
	// if (dataRX[0] == 0xF9 || dataRX[0] == 0xC4)
	// 	return;
	
	ESP_LOGD("TCL", "Message received: %x", dataRX[0]);

	// Wenn das empfangene Byte kein Header ist (0x2A), verlassen wir die Schleife
	if (dataRX[0] != HEADER_BYTE) {
		// ESP_LOGD("TCL", "Wrong byte");
		dataShow(0,0);
		return;
	}
	// Wenn der Header (0x2A) passt, lesen wir anschließend weitere 4 Bytes
	// Bei manchen Klimaanlagen muss zwischen Paketen delay(5) gesetzt werden. Warum unklar, aber gelegentlich notwendig.
	delay(5);
	dataRX[1] = esphome::uart::UARTDevice::read();
	delay(5);
	dataRX[2] = esphome::uart::UARTDevice::read();
	delay(5);
	dataRX[3] = esphome::uart::UARTDevice::read();
	delay(5);
	dataRX[4] = esphome::uart::UARTDevice::read();

	// Von den ersten 5 Bytes benötigen wir das fünfte: Es enthält die Nachrichtenlänge
	esphome::uart::UARTDevice::read_array(dataRX+5, dataRX[4]+1);

	uint8_t check = getChecksum(dataRX, sizeof(dataRX));

	// auto raw = getHex(dataRX, sizeof(dataRX));	
	// ESP_LOGD("TCL", "RX full : %s ", raw.c_str());
	
	// Prüfen der Prüfsumme
	if (check != dataRX[60]) {
		// ESP_LOGD("TCL", "Invalid checksum %x", check);
		this->dataShow(0,0);
		return;
	}

	this->dataShow(0,0);
	// Nach dem vollständigen Lesen des Puffers beginnen wir mit dem Parsen
	this->readData();
}

void tclacClimate::update() {
	tclacClimate::dataShow(1,1);
	this->esphome::uart::UARTDevice::write_array(poll, sizeof(poll));
	//auto raw = tclacClimate::getHex(poll, sizeof(poll));
	ESP_LOGD("TCL", "chek status sended");
	tclacClimate::dataShow(1,0);
}

void tclacClimate::readData() {
	
	current_temperature = float((( (dataRX[17] << 8) | dataRX[18] ) / 374 - 32)/1.8);
	target_temperature = (dataRX[FAN_SPEED_POS] & SET_TEMP_MASK) + 16;

	//ESP_LOGD("TCL", "TEMP: %f ", current_temperature);

	if (dataRX[MODE_POS] & ( 1 << 4)) {
		// Wenn die Klimaanlage eingeschaltet ist, bereiten wir die Daten zur Anzeige auf
		ESP_LOGD("TCL", "AC is on");
		uint8_t modeswitch = MODE_MASK & dataRX[MODE_POS];
		uint8_t fanspeedswitch = FAN_SPEED_MASK & dataRX[FAN_SPEED_POS];
		uint8_t swingmodeswitch = SWING_MODE_MASK & dataRX[SWING_POS];

		switch (modeswitch) {
			case MODE_AUTO:
				this->mode = climate::CLIMATE_MODE_AUTO;
				break;
			case MODE_COOL:
				this->mode = climate::CLIMATE_MODE_COOL;
				break;
			case MODE_DRY:
				this->mode = climate::CLIMATE_MODE_DRY;
				break;
			case MODE_FAN_ONLY:
				this->mode = climate::CLIMATE_MODE_FAN_ONLY;
				break;
			case MODE_HEAT:
				this->mode = climate::CLIMATE_MODE_HEAT;
				break;
			default:
				this->mode = climate::CLIMATE_MODE_AUTO;
		}

		if ( dataRX[FAN_QUIET_POS] & FAN_QUIET) {
			fan_mode = climate::CLIMATE_FAN_QUIET;
		} else if (dataRX[MODE_POS] & FAN_DIFFUSE){
			fan_mode = climate::CLIMATE_FAN_DIFFUSE;
		} else {
			switch (fanspeedswitch) {
				case FAN_AUTO:
					fan_mode = climate::CLIMATE_FAN_AUTO;
					break;
				case FAN_LOW:
					fan_mode = climate::CLIMATE_FAN_LOW;
					break;
				case FAN_MIDDLE:
					fan_mode = climate::CLIMATE_FAN_MIDDLE;
					break;
				case FAN_MEDIUM:
					fan_mode = climate::CLIMATE_FAN_MEDIUM;
					break;
				case FAN_HIGH:
					fan_mode = climate::CLIMATE_FAN_HIGH;
					break;
				case FAN_FOCUS:
					fan_mode = climate::CLIMATE_FAN_FOCUS;
					break;
				default:
					fan_mode = climate::CLIMATE_FAN_AUTO;
			}
		}

		switch (swingmodeswitch) {
			case SWING_OFF: 
				swing_mode = climate::CLIMATE_SWING_OFF;
				break;
			case SWING_HORIZONTAL:
				swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
				break;
			case SWING_VERTICAL:
				swing_mode = climate::CLIMATE_SWING_VERTICAL;
				break;
			case SWING_BOTH:
				swing_mode = climate::CLIMATE_SWING_BOTH;
				break;
		}
		
		// Verarbeiten der Preset-Daten
		preset = ClimatePreset::CLIMATE_PRESET_NONE;
		if (dataRX[7] & (1 << 6)){
			preset = ClimatePreset::CLIMATE_PRESET_ECO;
		} else if (dataRX[9] & (1 << 2)){
			preset = ClimatePreset::CLIMATE_PRESET_COMFORT;
		} else if (dataRX[19] & (1 << 0)){
			preset = ClimatePreset::CLIMATE_PRESET_SLEEP;
		}
		
	} else {
		ESP_LOGD("TCL", "AC is OFF");
		// Wenn die Klimaanlage aus ist, werden alle Modi als aus dargestellt
		this->mode = climate::CLIMATE_MODE_OFF;
		//fan_mode = climate::CLIMATE_FAN_OFF;
		this->swing_mode = climate::CLIMATE_SWING_OFF;
		this->preset = ClimatePreset::CLIMATE_PRESET_NONE;
	}
	// Zustand veröffentlichen
	this->publish_state();
	allow_take_control = true;
   }

// Klimasteuerung
void tclacClimate::control(const climate::ClimateCall &call) {
	
	ESP_LOGD("TCL", "Call from UI");
	
	// Dies und den folgenden Teil habe ich von Vi3jo übernommen.
	
	if (call.get_mode().has_value()) this->mode = *call.get_mode();
    if (call.get_target_temperature().has_value()) this->target_temperature = *call.get_target_temperature();
    if (call.get_fan_mode().has_value()) this->fan_mode = *call.get_fan_mode();
	if (call.get_swing_mode().has_value()) this->swing_mode = *call.get_swing_mode();
	if (call.get_preset().has_value()) this->preset = *call.get_preset();
	
	this->publish_state();
	this->takeControl();
	this->allow_take_control = true;
}
	
	
void tclacClimate::takeControl() {
	
	dataTX[7]  = 0b00000000;
	dataTX[8]  = 0b00000000;
	dataTX[9]  = 0b00000000;
	dataTX[10] = 0b00000000;
	dataTX[11] = 0b00000000;
	dataTX[19] = 0b00000000;
	dataTX[32] = 0b00000000;
	dataTX[33] = 0b00000000;
	
	uint8_t target_temperature_set = 31-(int)target_temperature;
	
	// Summer je nach Schalter in den Einstellungen ein- oder ausschalten
	if (beeper_status_){
		ESP_LOGD("TCL", "Beep mode ON");
		dataTX[7] += 0b00100000;
	} else {
		ESP_LOGD("TCL", "Beep mode OFF");
		dataTX[7] += 0b00000000;
	}
	
	// Display der Klimaanlage je nach Schalter in den Einstellungen ein- oder ausschalten
	// Das Display wird nur eingeschaltet, wenn die Anlage in einem Betriebsmodus ist
	
	// ACHTUNG! Beim Ausschalten des Displays wechselt die Klimaanlage erzwungen in den Automatikmodus!
	
	if ((display_status_) && (mode != climate::CLIMATE_MODE_OFF)){
		ESP_LOGD("TCL", "Dispaly turn ON");
		dataTX[7] += 0b01000000;
	} else {
		ESP_LOGD("TCL", "Dispaly turn OFF");
		dataTX[7] += 0b00000000;
	}
		
	// Betriebsmodus der Klimaanlage einstellen
	switch (this->mode) {
		case climate::CLIMATE_MODE_OFF:
			dataTX[7] += 0b00000000;
			dataTX[8] += 0b00000000;
			break;
		case climate::CLIMATE_MODE_AUTO:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00001000;
			break;
		case climate::CLIMATE_MODE_COOL:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00000011;	
			break;
		case climate::CLIMATE_MODE_DRY:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00000010;	
			break;
		case climate::CLIMATE_MODE_FAN_ONLY:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00000111;	
			break;
		case climate::CLIMATE_MODE_HEAT:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00000001;	
			break;
	}

	// Lüftermodus einstellen
	if (this->fan_mode.has_value()) {
		switch(*this->fan_mode) {
			case climate::CLIMATE_FAN_AUTO:
				dataTX[8]	+= 0b00000000;
				dataTX[10]	+= 0b00000000;
				break;
			case climate::CLIMATE_FAN_QUIET:
				dataTX[8]	+= 0b10000000;
				dataTX[10]	+= 0b00000000;
				break;
			case climate::CLIMATE_FAN_LOW:
				dataTX[8]	+= 0b00000000;
				dataTX[10]	+= 0b00000001;
				break;
			case climate::CLIMATE_FAN_MIDDLE:
				dataTX[8]	+= 0b00000000;
				dataTX[10]	+= 0b00000110;
				break;
			case climate::CLIMATE_FAN_MEDIUM:
				dataTX[8]	+= 0b00000000;
				dataTX[10]	+= 0b00000011;
				break;
			case climate::CLIMATE_FAN_HIGH:
				dataTX[8]	+= 0b00000000;
				dataTX[10]	+= 0b00000111;
				break;
			case climate::CLIMATE_FAN_FOCUS:
				dataTX[8]	+= 0b00000000;
				dataTX[10]	+= 0b00000101;
				break;
			case climate::CLIMATE_FAN_DIFFUSE:
				dataTX[8]	+= 0b01000000;
				dataTX[10]	+= 0b00000000;
				break;
		}
	}
	
	// Swing-Modus der Lamellen einstellen
	switch(this->swing_mode) {
		case climate::CLIMATE_SWING_OFF:
			dataTX[10]	+= 0b00000000;
			dataTX[11]	+= 0b00000000;
			break;
		case climate::CLIMATE_SWING_VERTICAL:
			dataTX[10]	+= 0b00111000;
			dataTX[11]	+= 0b00000000;
			break;
		case climate::CLIMATE_SWING_HORIZONTAL:
			dataTX[10]	+= 0b00000000;
			dataTX[11]	+= 0b00001000;
			break;
		case climate::CLIMATE_SWING_BOTH:
			dataTX[10]	+= 0b00111000;
			dataTX[11]	+= 0b00001000;  
			break;
	}
	
	// Presets der Klimaanlage einstellen
	if (this->preset.has_value()) {
		switch(*this->preset) {
			case ClimatePreset::CLIMATE_PRESET_NONE:
				break;
			case ClimatePreset::CLIMATE_PRESET_ECO:
				dataTX[7]	+= 0b10000000;
				break;
			case ClimatePreset::CLIMATE_PRESET_SLEEP:
				dataTX[19]	+= 0b00000001;
				break;
			case ClimatePreset::CLIMATE_PRESET_COMFORT:
				dataTX[8]	+= 0b00010000;
				break;
		}
	}

        // Lamellenmodi
		//	Vertikale Lamelle
		//		Swing der vertikalen Lamelle [Byte 10, Maske 00111000]:
		//			000 - Swing aus, Lamelle in letzter Position oder fixiert
		//			111 - Swing im gewählten Modus aktiv
		//		Swing-Modus der vertikalen Lamelle (Fixiermodus ist ohne Bedeutung, wenn Swing aktiv ist) [Byte 32, Maske 00011000]:
		//			01 - Swing von oben nach unten, STANDARD
		//			10 - Swing in der oberen Hälfte
		//			11 - Swing in der unteren Hälfte
		//		Fixiermodus der Lamelle (Swing-Modus ist ohne Bedeutung, wenn Swing aus ist) [Byte 32, Maske 00000111]:
		//			000 - keine Fixierung, STANDARD
		//			001 - oben fixiert
		//			010 - zwischen oben und Mitte fixiert
		//			011 - in der Mitte fixiert
		//			100 - zwischen Mitte und unten fixiert
		//			101 - unten fixiert
		//	Horizontale Lamellen
		//		Swing der horizontalen Lamellen [Byte 11, Maske 00001000]:
		//			0 - Swing aus, Lamellen in letzter Position oder fixiert
		//			1 - Swing im gewählten Modus aktiv
		//		Swing-Modus der horizontalen Lamellen (Fixiermodus ist ohne Bedeutung, wenn Swing aktiv ist) [Byte 33, Maske 00111000]:
		//			001 - Swing von links nach rechts, STANDARD
		//			010 - Swing links
		//			011 - Swing mittig
		//			100 - Swing rechts
		//		Fixiermodus der horizontalen Lamellen (Swing-Modus ist ohne Bedeutung, wenn Swing aus ist) [Byte 33, Maske 00000111]:
		//			000 - keine Fixierung, STANDARD
		//			001 - links fixiert
		//			010 - zwischen linker Seite und Mitte fixiert
		//			011 - in der Mitte fixiert
		//			100 - zwischen Mitte und rechter Seite fixiert
		//			101 - rechts fixiert
		
		
	// Modus für den Swing der vertikalen Lamelle einstellen
	switch(vertical_swing_direction_) {
		case VerticalSwingDirection::UP_DOWN:
			dataTX[32]	+= 0b00001000;
			ESP_LOGD("TCL", "Vertical swing: up-down");
			break;
		case VerticalSwingDirection::UPSIDE:
			dataTX[32]	+= 0b00010000;
			ESP_LOGD("TCL", "Vertical swing: upper");
			break;
		case VerticalSwingDirection::DOWNSIDE:
			dataTX[32]	+= 0b00011000;
			ESP_LOGD("TCL", "Vertical swing: downer");
			break;
	}
	// Modus für den Swing der horizontalen Lamellen einstellen
	switch(horizontal_swing_direction_) {
		case HorizontalSwingDirection::LEFT_RIGHT:
			dataTX[33]	+= 0b00001000;
			ESP_LOGD("TCL", "Horizontal swing: left-right");
			break;
		case HorizontalSwingDirection::LEFTSIDE:
			dataTX[33]	+= 0b00010000;
			ESP_LOGD("TCL", "Horizontal swing: lefter");
			break;
		case HorizontalSwingDirection::CENTER:
			dataTX[33]	+= 0b00011000;
			ESP_LOGD("TCL", "Horizontal swing: center");
			break;
		case HorizontalSwingDirection::RIGHTSIDE:
			dataTX[33]	+= 0b00100000;
			ESP_LOGD("TCL", "Horizontal swing: righter");
			break;
	}
	// Fixierposition der vertikalen Lamelle einstellen
	switch(vertical_direction_) {
		case AirflowVerticalDirection::LAST:
			dataTX[32]	+= 0b00000000;
			ESP_LOGD("TCL", "Vertical fix: last position");
			break;
		case AirflowVerticalDirection::MAX_UP:
			dataTX[32]	+= 0b00000001;
			ESP_LOGD("TCL", "Vertical fix: up");
			break;
		case AirflowVerticalDirection::UP:
			dataTX[32]	+= 0b00000010;
			ESP_LOGD("TCL", "Vertical fix: upper");
			break;
		case AirflowVerticalDirection::CENTER:
			dataTX[32]	+= 0b00000011;
			ESP_LOGD("TCL", "Vertical fix: center");
			break;
		case AirflowVerticalDirection::DOWN:
			dataTX[32]	+= 0b00000100;
			ESP_LOGD("TCL", "Vertical fix: downer");
			break;
		case AirflowVerticalDirection::MAX_DOWN:
			dataTX[32]	+= 0b00000101;
			ESP_LOGD("TCL", "Vertical fix: down");
			break;
	}
	// Fixierposition der horizontalen Lamellen einstellen
	switch(horizontal_direction_) {
		case AirflowHorizontalDirection::LAST:
			dataTX[33]	+= 0b00000000;
			ESP_LOGD("TCL", "Horizontal fix: last position");
			break;
		case AirflowHorizontalDirection::MAX_LEFT:
			dataTX[33]	+= 0b00000001;
			ESP_LOGD("TCL", "Horizontal fix: left");
			break;
		case AirflowHorizontalDirection::LEFT:
			dataTX[33]	+= 0b00000010;
			ESP_LOGD("TCL", "Horizontal fix: lefter");
			break;
		case AirflowHorizontalDirection::CENTER:
			dataTX[33]	+= 0b00000011;
			ESP_LOGD("TCL", "Horizontal fix: center");
			break;
		case AirflowHorizontalDirection::RIGHT:
			dataTX[33]	+= 0b00000100;
			ESP_LOGD("TCL", "Horizontal fix: righter");
			break;
		case AirflowHorizontalDirection::MAX_RIGHT:
			dataTX[33]	+= 0b00000101;
			ESP_LOGD("TCL", "Horizontal fix: right");
			break;
	}

	// Temperatur einstellen
	dataTX[9] = target_temperature_set;
		
	// Byte-Array für das Senden an die Klimaanlage zusammenbauen
	dataTX[0] = HEADER_BYTE;	// Start-Header-Byte
	dataTX[1] = 0x00;	// Start-Header-Byte
	dataTX[2] = 0x01;	// Start-Header-Byte
	dataTX[3] = 0x03;	// 0x03 - Steuerung, 0x04 - Abfrage
	dataTX[4] = 0x20;	// 0x20 - Steuerung, 0x19 - Abfrage
	dataTX[5] = 0x03;	//??
	dataTX[6] = 0x01;	//??
	//dataTX[7] = 0x64;	// eco, display, beep, ontimerenable, offtimerenable, power,0,0
	//dataTX[8] = 0x08;	// mute,0,turbo,health, mode(4): 01 heat, 02 dry, 03 cool, 07 fan, 08 auto, health(+16), 41=turbo-heat, 43=turbo-cool (turbo = 0x40 + 0x01..0x08)
	//dataTX[9] = 0x0f;	// 0-31; 15 entspricht 16 Grad; 0,0,0,0,temp(4), settemp 31 - x
	//dataTX[10] = 0x00;	// 0,timerindicator,swingv(3),fan(3) fan+swing modes //0=auto 1=low 2=med 3=high
	//dataTX[11] = 0x00;	//0,offtimer(6),0
	dataTX[12] = 0x00;	// fahrenheit,ontimer(6),0; cf 80=f 0=c
	dataTX[13] = 0x01;	//??
	dataTX[14] = 0x00;	//0,0,halfdegree,0,0,0,0,0
	dataTX[15] = 0x00;	//??
	dataTX[16] = 0x00;	//??
	dataTX[17] = 0x00;	//??
	dataTX[18] = 0x00;	//??
	//dataTX[19] = 0x00;	// sleep ein = 1, aus = 0
	dataTX[20] = 0x00;	//??
	dataTX[21] = 0x00;	//??
	dataTX[22] = 0x00;	//??
	dataTX[23] = 0x00;	//??
	dataTX[24] = 0x00;	//??
	dataTX[25] = 0x00;	//??
	dataTX[26] = 0x00;	//??
	dataTX[27] = 0x00;	//??
	dataTX[28] = 0x00;	//??
	dataTX[30] = 0x00;	//??
	dataTX[31] = 0x00;	//??
	//dataTX[32] = 0x00;	// 0,0,0,vertikaler Swing-Modus(2),vertikaler Fixiermodus(3)
	//dataTX[33] = 0x00;	// 0,0,horizontaler Swing-Modus(3),horizontaler Fixiermodus(3)
	dataTX[34] = 0x00;	//??
	dataTX[35] = 0x00;	//??
	dataTX[36] = 0x00;	//??
	dataTX[37] = 0xFF;	// Prüfsumme
	dataTX[37] = tclacClimate::getChecksum(dataTX, sizeof(dataTX));

	tclacClimate::sendData(dataTX, sizeof(dataTX));
	allow_take_control = false;
	is_call_control = false;
}

// Daten an die Klimaanlage senden
void tclacClimate::sendData(uint8_t * message, uint8_t size) {
	tclacClimate::dataShow(1,1);
	//Serial.write(message, size);
	this->esphome::uart::UARTDevice::write_array(message, size);
	//auto raw = getHex(message, size);
	ESP_LOGD("TCL", "Message to TCL sended...");
	tclacClimate::dataShow(1,0);
}

// Byte in ein lesbares Format umwandeln
String tclacClimate::getHex(uint8_t *message, uint8_t size) {
	String raw;
	for (int i = 0; i < size; i++) {
		raw += "\n" + String(message[i]);
	}
	raw.toUpperCase();
	return raw;
}

// Prüfsumme berechnen
uint8_t tclacClimate::getChecksum(const uint8_t * message, size_t size) {
	uint8_t position = size - 1;
	uint8_t crc = 0;
	for (int i = 0; i < position; i++)
		crc ^= message[i];
	return crc;
}

// LEDs blinken lassen
void tclacClimate::dataShow(bool flow, bool shine) {
	if (module_display_status_){
		if (flow == 0){
			if (shine == 1){
#ifdef CONF_RX_LED
				this->rx_led_pin_->digital_write(true);
#endif
			} else {
#ifdef CONF_RX_LED
				this->rx_led_pin_->digital_write(false);
#endif
			}
		}
		if (flow == 1) {
			if (shine == 1){
#ifdef CONF_TX_LED
				this->tx_led_pin_->digital_write(true);
#endif
			} else {
#ifdef CONF_TX_LED
				this->tx_led_pin_->digital_write(false);
#endif
			}
		}
	}
}

// Aktionen mit Daten aus der Konfiguration

// Status des Summers übernehmen
void tclacClimate::set_beeper_state(bool state) {
	this->beeper_status_ = state;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Status des Klimaanlagen-Displays übernehmen
void tclacClimate::set_display_state(bool disp_state) {
	this->display_status_ = disp_state;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Status des erzwungenen Anwendens von Einstellungen übernehmen
void tclacClimate::set_force_mode_state(bool f_state) {
	this->force_mode_status_ = f_state;
}
// Pin der Empfangs-LED übernehmen
#ifdef CONF_RX_LED
void tclacClimate::set_rx_led_pin(GPIOPin *rx_led_pin) {
	this->rx_led_pin_ = rx_led_pin;
}
#endif
// Pin der Sende-LED übernehmen
#ifdef CONF_TX_LED
void tclacClimate::set_tx_led_pin(GPIOPin *tx_led_pin) {
	this->tx_led_pin_ = tx_led_pin;
}
#endif
// Status der Modul-Kommunikations-LEDs übernehmen
void tclacClimate::set_module_display_state(bool d_state) {
	this->module_display_status_ = d_state;
}
// Fixiermodus der vertikalen Lamelle übernehmen
void tclacClimate::set_vertical_airflow(AirflowVerticalDirection v_airflow) {
	this->vertical_direction_ = v_airflow;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Fixiermodus der horizontalen Lamellen übernehmen
void tclacClimate::set_horizontal_airflow(AirflowHorizontalDirection h_airflow) {
	this->horizontal_direction_ = h_airflow;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Swing-Modus der vertikalen Lamelle übernehmen
void tclacClimate::set_vertical_swing_direction(VerticalSwingDirection vs_direction) {
	this->vertical_swing_direction_ = vs_direction;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Verfügbare Betriebsmodi der Klimaanlage übernehmen
void tclacClimate::set_supported_modes(climate::ClimateModeMask modes) {
	this->supported_modes_ = modes;
	ESP_LOGD("TCL", "Set up Modes");
}
// Swing-Modus der horizontalen Lamellen übernehmen
void tclacClimate::set_horizontal_swing_direction(HorizontalSwingDirection hs_direction) {
	horizontal_swing_direction_ = hs_direction;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Verfügbare Lüftergeschwindigkeiten übernehmen
void tclacClimate::set_supported_fan_modes(climate::ClimateFanModeMask fan_modes){
	this->supported_fan_modes_ = fan_modes;
}
// Verfügbare Swing-Modi der Lamellen übernehmen
void tclacClimate::set_supported_swing_modes(climate::ClimateSwingModeMask swing_modes) {
	this->supported_swing_modes_ = swing_modes;
}
// Verfügbare Presets übernehmen
void tclacClimate::set_supported_presets(climate::ClimatePresetMask presets) {
  this->supported_presets_ = presets;
}


}
}