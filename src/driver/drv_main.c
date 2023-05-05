#include "../i2c/drv_i2c_public.h"
#include "../logging/logging.h"
#include "drv_bl0937.h"
#include "drv_bl0942.h"
#include "drv_bl_shared.h"
#include "drv_cse7766.h"
#include "drv_ir.h"
#include "drv_local.h"
#include "drv_ntp.h"
#include "drv_public.h"
#include "drv_ssdp.h"
#include "drv_test_drivers.h"
#include "drv_tuyaMCU.h"
#include "drv_uart.h"

const char* sensor_mqttNames[OBK_NUM_MEASUREMENTS] = {
	"voltage",
	"current",
	"power"
};

//sensor_device_classes just so happens to be the same as sensor_mqttNames.
const char* sensor_mqtt_device_classes[OBK_NUM_MEASUREMENTS] = {
	"voltage",
	"current",
	"power"
};

const char* sensor_mqtt_device_units[OBK_NUM_MEASUREMENTS] = {
	"V",
	"A",
	"W"
};

const char* counter_mqttNames[OBK_NUM_COUNTERS] = {
	"energycounter",
	"energycounter_last_hour",
	"consumption_stats",
	"energycounter_yesterday",
	"energycounter_today",
	"energycounter_clear_date",
};

const char* counter_devClasses[OBK_NUM_COUNTERS] = {
	"energy",
	"energy",
	"",
	"energy",
	"energy",
	"timestamp"
};

typedef struct driver_s {
	const char* name;
	void (*initFunc)();
	void (*onEverySecond)();
	void (*appendInformationToHTTPIndexPage)(http_request_t* request);
	void (*runQuickTick)();
	void (*stopFunc)();
	void (*onChannelChanged)(int ch, int val);
	bool bLoaded;
} driver_t;


// startDriver BL0937
static driver_t g_drivers[] = {
#ifdef ENABLE_DRIVER_TUYAMCU
	//drvdetail:{"name":"TuyaMCU",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"TuyaMCU is a protocol used for communication between WiFI module and external MCU. This protocol is using usually RX1/TX1 port of BK chips. See [TuyaMCU dimmer example](https://www.elektroda.com/rtvforum/topic3929151.html), see [TH06 LCD humidity/temperature sensor example](https://www.elektroda.com/rtvforum/topic3942730.html), see [fan controller example](https://www.elektroda.com/rtvforum/topic3908093.html), see [simple switch example](https://www.elektroda.com/rtvforum/topic3906443.html)",
	//drvdetail:"requires":""}
	{ "TuyaMCU",	TuyaMCU_Init,		TuyaMCU_RunFrame,			NULL, NULL, NULL, NULL, false },
	//drvdetail:{"name":"tmSensor",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"tmSensor must be used only when TuyaMCU is already started. tmSensor is a TuyaMcu Sensor, it's used for Low Power TuyaMCU communication on devices like TuyaMCU door sensor, or TuyaMCU humidity sensor. After device reboots, tmSensor uses TuyaMCU to request data update from the sensor and reports it on MQTT. Then MCU turns off WiFi module again and goes back to sleep. See an [example door sensor here](https://www.elektroda.com/rtvforum/topic3914412.html).",
	//drvdetail:"requires":""}
	{ "tmSensor",	TuyaMCU_Sensor_Init, TuyaMCU_Sensor_RunFrame,	NULL, NULL, NULL, NULL, false },
#endif
#ifdef ENABLE_NTP
	//drvdetail:{"name":"NTP",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"NTP driver is required to get current time and date from web. Without it, there is no correct datetime.",
	//drvdetail:"requires":""}
	{ "NTP",		NTP_Init,			NTP_OnEverySecond,			NTP_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
#endif
#ifdef ENABLE_HTTPBUTTONS
	//drvdetail:{"name":"HTTPButtons",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"This driver allows you to create custom, scriptable buttons on main WWW page. You can create those buttons in autoexec.bat and assign commands to them",
	//drvdetail:"requires":""}
	{ "HTTPButtons",	DRV_InitHTTPButtons, NULL, NULL, NULL, NULL, NULL, false },
#endif
#ifdef ENABLE_TEST_DRIVERS
	//drvdetail:{"name":"TESTPOWER",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"This is a fake POWER measuring socket driver, only for testing",
	//drvdetail:"requires":""}
	{ "TESTPOWER",	Test_Power_Init,	 Test_Power_RunFrame,		BL09XX_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
	//drvdetail:{"name":"TESTLED",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"This is a fake I2C LED driver, only for testing",
	//drvdetail:"requires":""}
	{ "TESTLED",	Test_LED_Driver_Init, Test_LED_Driver_RunFrame, NULL, NULL, NULL, Test_LED_Driver_OnChannelChanged, false },
#endif
#if ENABLE_I2C
	//drvdetail:{"name":"I2C",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Generic I2C, not used for LED drivers, but may be useful for displays or port expanders. Supports both hardware and software I2C.",
	//drvdetail:"requires":""}
	{ "I2C",		DRV_I2C_Init,		DRV_I2C_EverySecond,		NULL, NULL, NULL, NULL, false },
#endif
#ifdef ENABLE_DRIVER_BL0942
	//drvdetail:{"name":"BL0942",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"BL0942 is a power-metering chip which uses UART protocol for communication. It's usually connected to TX1/RX1 port of BK. You need to calibrate power metering once, just like in Tasmota. See [LSPA9 teardown example](https://www.elektroda.com/rtvforum/topic3887748.html). ",
	//drvdetail:"requires":""}
	{ "BL0942",		BL0942_UART_Init,	BL0942_UART_RunFrame,		BL09XX_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
#endif
#ifdef ENABLE_DRIVER_BL0942SPI
	//drvdetail:{"name":"BL0942SPI",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"BL0942 is a power-metering chip which uses SPI protocol for communication. It's usually connected to SPI1 port of BK. You need to calibrate power metering once, just like in Tasmota. See [PZIOT-E01 teardown example](https://www.elektroda.com/rtvforum/topic3945667.html). ",
	//drvdetail:"requires":""}
	{ "BL0942SPI",	BL0942_SPI_Init,	BL0942_SPI_RunFrame,		BL09XX_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
#endif
#ifdef ENABLE_DRIVER_BL0937
	//drvdetail:{"name":"BL0937",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"BL0937 is a power-metering chip which uses custom protocol to report data. It requires setting 3 pins in pin config: CF, CF1 and SEL",
	//drvdetail:"requires":""}
	{ "BL0937",		BL0937_Init,		BL0937_RunFrame,			BL09XX_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
#endif
#ifdef ENABLE_DRIVER_CSE7766
	//drvdetail:{"name":"CSE7766",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"BL0942 is a power-metering chip which uses UART protocol for communication. It's usually connected to TX1/RX1 port of BK",
	//drvdetail:"requires":""}
	{ "CSE7766",	CSE7766_Init,		CSE7766_RunFrame,			BL09XX_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
#endif
#if PLATFORM_BEKEN
	//drvdetail:{"name":"SM16703P",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"WIP driver",
	//drvdetail:"requires":""}
	{ "SM16703P",	SM16703P_Init,		NULL,						NULL, NULL, NULL, NULL, false },
	//drvdetail:{"name":"IR",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"IRLibrary wrapper, so you can receive remote signals and send them. See [forum discussion here](https://www.elektroda.com/rtvforum/topic3920360.html), also see [LED strip and IR YT video](https://www.youtube.com/watch?v=KU0tDwtjfjw)",
	//drvdetail:"requires":""}
	{ "IR",			DRV_IR_Init,		 NULL,						NULL, DRV_IR_RunFrame, NULL, NULL, false },
#endif
#if defined(PLATFORM_BEKEN) || defined(WINDOWS)	|| defined(PLATFORM_BL602)
	//drvdetail:{"name":"DDP",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"DDP is a LED control protocol that is using UDP. You can use xLights or any other app to control OBK LEDs that way.",
	//drvdetail:"requires":""}
	{ "DDP",		DRV_DDP_Init,		NULL,						DRV_DDP_AppendInformationToHTTPIndexPage, DRV_DDP_RunFrame, DRV_DDP_Shutdown, NULL, false },
	//drvdetail:{"name":"SSDP",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"SSDP is a discovery protocol, so BK devices can show up in, for example, Windows network section",
	//drvdetail:"requires":""}
	{ "SSDP",		DRV_SSDP_Init,		DRV_SSDP_RunEverySecond,	NULL, DRV_SSDP_RunQuickTick, DRV_SSDP_Shutdown, NULL, false },
	//drvdetail:{"name":"Wemo",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Wemo emulation for Alexa. You must also start SSDP so it can run, because it depends on SSDP discovery.",
	//drvdetail:"requires":""}
	{ "Wemo",		WEMO_Init,		NULL,		WEMO_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
	//drvdetail:{"name":"DGR",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Tasmota Device groups driver. See [forum example](https://www.elektroda.com/rtvforum/topic3925472.html) and TODO-video tutorial (will post on YT soon)",
	//drvdetail:"requires":""}
	{ "DGR",		DRV_DGR_Init,		DRV_DGR_RunEverySecond,		DRV_DGR_AppendInformationToHTTPIndexPage, DRV_DGR_RunQuickTick, DRV_DGR_Shutdown, DRV_DGR_OnChannelChanged, false },
#endif
#if defined(PLATFORM_BEKEN) || defined(WINDOWS)
	//drvdetail:{"name":"PWMToggler",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"PWMToggler is a custom abstraction layer that can run on top of raw PWM channels. It provides ability to turn off/on the PWM while keeping it's value, which is not possible by direct channel operations. It can be used for some custom devices with extra lights/lasers. See example [here](https://www.elektroda.com/rtvforum/topic3939064.html).",
	//drvdetail:"requires":""}
	{ "PWMToggler",	DRV_InitPWMToggler, NULL, DRV_Toggler_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
	//drvdetail:{"name":"DoorSensor",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"DoorSensor is using deep sleep to preserve battery. This is used for devices without TuyaMCU, where BK deep sleep and wakeup on GPIO is used. This drives requires you to set a DoorSensor pin. Change on door sensor pin wakes up the device. If there are no changes for some time, device goes to sleep. See example [here](https://www.elektroda.com/rtvforum/topic3960149.html). If your door sensor does not wake up in certain pos, please use DSEdge command (try all 3 options, default is 2). ",
	//drvdetail:"requires":""}
	{ "DoorSensor",		DoorDeepSleep_Init,		DoorDeepSleep_OnEverySecond,	DoorDeepSleep_AppendInformationToHTTPIndexPage, NULL, NULL, DoorDeepSleep_OnChannelChanged, false },
	//drvdetail:{"name":"MAX72XX_Clock",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Simple hardcoded driver for MAX72XX clock. Requirex manual start of MAX72XX driver with MAX72XX setup and NTP start.",
	//drvdetail:"requires":""}
	{ "MAX72XX_Clock",		DRV_MAX72XX_Clock_Init,		DRV_MAX72XX_Clock_OnEverySecond,	NULL, DRV_MAX72XX_Clock_RunFrame, NULL, NULL, false },
	//drvdetail:{"name":"ADCButton",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"This allows you to connect multiple buttons on single ADC pin. Each button must have a different resistor value, this works by probing the voltage on ADC from a resistor divider. You need to select AB_Map first. See forum post for [details](https://www.elektroda.com/rtvforum/viewtopic.php?p=20541973#20541973).",
	//drvdetail:"requires":""}
	{ "ADCButton",		DRV_ADCButton_Init,		NULL,	NULL, DRV_ADCButton_RunFrame, NULL, NULL, false },
#endif
#ifdef ENABLE_DRIVER_LED
	//drvdetail:{"name":"SM2135",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"SM2135 custom-'I2C' LED driver for RGBCW lights. This will start automatically if you set both SM2135 pin roles. This may need you to remap the RGBCW indexes with SM2135_Map command",
	//drvdetail:"requires":""}
	{ "SM2135",		SM2135_Init,		NULL,			NULL, NULL, NULL, NULL, false },
	//drvdetail:{"name":"BP5758D",
	//drvdetail:"title":"TODO",	
	//drvdetail:"descr":"BP5758D custom-'I2C' LED driver for RGBCW lights. This will start automatically if you set both BP5758D pin roles. This may need you to remap the RGBCW indexes with BP5758D_Map command. This driver is used in some of BL602/Sonoff bulbs, see [video flashing tutorial here](https://www.youtube.com/watch?v=L6d42IMGhHw)",
	//drvdetail:"requires":""}
	{ "BP5758D",	BP5758D_Init,		NULL,			NULL, NULL, NULL, NULL, false },
	//drvdetail:{"name":"BP1658CJ",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"BP1658CJ custom-'I2C' LED driver for RGBCW lights. This will start automatically if you set both BP1658CJ pin roles. This may need you to remap the RGBCW indexes with BP1658CJ_Map command",
	//drvdetail:"requires":""}
	{ "BP1658CJ",	BP1658CJ_Init,		NULL,			NULL, NULL, NULL, NULL, false },
	//drvdetail:{"name":"SM2235",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"SM2335 andd SM2235 custom-'I2C' LED driver for RGBCW lights. This will start automatically if you set both SM2235 pin roles. This may need you to remap the RGBCW indexes with SM2235_Map command",
	//drvdetail:"requires":""}
	{ "SM2235",		SM2235_Init,		NULL,			NULL, NULL, NULL, NULL, false },
#endif
#if defined(PLATFORM_BEKEN) || defined(WINDOWS)
	//drvdetail:{"name":"CHT8305",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"CHT8305 is a Temperature and Humidity sensor with I2C interface.",
	//drvdetail:"requires":""}
	{ "CHT8305",	CHT8305_Init,		CHT8305_OnEverySecond,		CHT8305_AppendInformationToHTTPIndexPage, NULL, NULL, NULL, false },
	//drvdetail:{"name":"KP18068",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"KP18068 I2C LED driver",
	//drvdetail:"requires":""}
	{ "KP18068",		KP18068_Init,		NULL,			NULL, NULL, NULL, NULL, false },
	//drvdetail:{"name":"MAX72XX",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"MAX72XX LED matrix display driver with font and simple script interface.",
	//drvdetail:"requires":""}
	{ "MAX72XX",	DRV_MAX72XX_Init,		NULL,		NULL, NULL, NULL, NULL, false },
	//drvdetail:{"name":"TM1637",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Driver for 7-segment LED display with DIO/CLK interface",
	//drvdetail:"requires":""}
	{ "TM1637",	TM1637_Init,		NULL,		NULL,  TMGN_RunQuickTick,NULL, NULL, false },
	//drvdetail:{"name":"GN6932",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Driver for 7-segment LED display with DIO/CLK/STB interface. See [this topic](https://www.elektroda.com/rtvforum/topic3971252.html) for details.",
	//drvdetail:"requires":""}
	{ "GN6932",	GN6932_Init,		NULL,		NULL, TMGN_RunQuickTick, NULL, NULL, false },
	//drvdetail:{"name":"TM1638",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Driver for 7-segment LED display with DIO/CLK/STB interface. TM1638 is very similiar to GN6932 and TM1637. See [this topic](https://www.elektroda.com/rtvforum/viewtopic.php?p=20553628#20553628) for details.",
	//drvdetail:"requires":""}
	{ "TM1638",	TM1638_Init,		NULL,		NULL, TMGN_RunQuickTick,NULL,  NULL, false },
	//drvdetail:{"name":"SHT3X",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Humidity/temperature sensor. See [SHT Sensor tutorial topic here](https://www.elektroda.com/rtvforum/topic3958369.html), also see [this sensor teardown](https://www.elektroda.com/rtvforum/topic3945688.html)",
	//drvdetail:"requires":""}
	{ "SHT3X",	    SHT3X_Init,		SHT3X_OnEverySecond,		SHT3X_AppendInformationToHTTPIndexPage, NULL, SHT3X_StopDriver, NULL, false },
	//drvdetail:{"name":"SGP",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"SGP Air Quality sensor with I2C interface.",
	//drvdetail:"requires":""}
	{ "SGP",	    SGP_Init,		SGP_OnEverySecond,		SGP_AppendInformationToHTTPIndexPage, NULL, SGP_StopDriver, NULL, false },

	//drvdetail:{"name":"ShiftRegister",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"ShiftRegisterShiftRegisterShiftRegisterShiftRegister",
	//drvdetail:"requires":""}
	{ "ShiftRegister",	    Shift_Init,		Shift_OnEverySecond,		NULL, NULL, NULL, Shift_OnChannelChanged, false },
#endif
#if defined(PLATFORM_BEKEN) || defined(WINDOWS)
	//drvdetail:{"name":"Battery",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"Custom mechanism to measure battery level with ADC and an optional relay. See [example here](https://www.elektroda.com/rtvforum/topic3959103.html).",
	//drvdetail:"requires":""}
	{ "Battery",	Batt_Init,		Batt_OnEverySecond,		Batt_AppendInformationToHTTPIndexPage, NULL, Batt_StopDriver, NULL, false },
#endif
#ifdef ENABLE_DRIVER_BRIDGE
	//drvdetail:{"name":"Bridge",
	//drvdetail:"title":"TODO",
	//drvdetail:"descr":"TODO",
	//drvdetail:"requires":""}
	{ "Bridge",     Bridge_driver_Init, NULL,                       NULL, Bridge_driver_QuickFrame, Bridge_driver_DeInit, Bridge_driver_OnChannelChanged, false }
#endif
};


static const int g_numDrivers = sizeof(g_drivers) / sizeof(g_drivers[0]);

bool DRV_IsRunning(const char* name) {
	int i;

	for (i = 0; i < g_numDrivers; i++) {
		if (g_drivers[i].bLoaded) {
			if (!stricmp(name, g_drivers[i].name)) {
				return true;
			}
		}
	}
	return false;
}

static SemaphoreHandle_t g_mutex = 0;

bool DRV_Mutex_Take(int del) {
	int taken;

	if (g_mutex == 0)
	{
		g_mutex = xSemaphoreCreateMutex();
	}
	taken = xSemaphoreTake(g_mutex, del);
	if (taken == pdTRUE) {
		return true;
	}
	return false;
}
void DRV_Mutex_Free() {
	xSemaphoreGive(g_mutex);
}
void DRV_OnEverySecond() {
	int i;

	if (DRV_Mutex_Take(100) == false) {
		return;
	}
	for (i = 0; i < g_numDrivers; i++) {
		if (g_drivers[i].bLoaded) {
			if (g_drivers[i].onEverySecond != 0) {
				g_drivers[i].onEverySecond();
			}
		}
	}
	DRV_Mutex_Free();
}
void DRV_RunQuickTick() {
	int i;

	if (DRV_Mutex_Take(0) == false) {
		return;
	}
	for (i = 0; i < g_numDrivers; i++) {
		if (g_drivers[i].bLoaded) {
			if (g_drivers[i].runQuickTick != 0) {
				g_drivers[i].runQuickTick();
			}
		}
	}
	DRV_Mutex_Free();
}
void DRV_OnChannelChanged(int channel, int iVal) {
	int i;

	//if(DRV_Mutex_Take(100)==false) {
	//	return;
	//}
	for (i = 0; i < g_numDrivers; i++) {
		if (g_drivers[i].bLoaded) {
			if (g_drivers[i].onChannelChanged != 0) {
				g_drivers[i].onChannelChanged(channel, iVal);
			}
		}
	}
	//DRV_Mutex_Free();
}
// right now only used by simulator
void DRV_ShutdownAllDrivers() {
	int i;
	for (i = 0; i < g_numDrivers; i++) {
		if (g_drivers[i].bLoaded) {
			DRV_StopDriver(g_drivers[i].name);
		}
	}
}
void DRV_StopDriver(const char* name) {
	int i;

	if (DRV_Mutex_Take(100) == false) {
		return;
	}
	for (i = 0; i < g_numDrivers; i++) {
		if (*name == '*' || !stricmp(g_drivers[i].name, name)) {
			if (g_drivers[i].bLoaded) {
				if (g_drivers[i].stopFunc != 0) {
					g_drivers[i].stopFunc();
				}
				g_drivers[i].bLoaded = false;
				addLogAdv(LOG_INFO, LOG_FEATURE_MAIN, "Drv %s stopped.", g_drivers[i].name);
			}
			else {
				if (*name != '*') {
					addLogAdv(LOG_INFO, LOG_FEATURE_MAIN, "Drv %s not running.", name);
				}
			}
		}
	}
	DRV_Mutex_Free();
}
void DRV_StartDriver(const char* name) {
	int i;
	int bStarted;

	if (DRV_Mutex_Take(100) == false) {
		return;
	}
	bStarted = 0;
	for (i = 0; i < g_numDrivers; i++) {
		if (!stricmp(g_drivers[i].name, name)) {
			if (g_drivers[i].bLoaded) {
				addLogAdv(LOG_INFO, LOG_FEATURE_MAIN, "Drv %s is already loaded.\n", name);
				bStarted = 1;
				break;

			}
			else {
				g_drivers[i].initFunc();
				g_drivers[i].bLoaded = true;
				addLogAdv(LOG_INFO, LOG_FEATURE_MAIN, "Started %s.\n", name);
				bStarted = 1;
				break;
			}
		}
	}
	if (!bStarted) {
		addLogAdv(LOG_INFO, LOG_FEATURE_MAIN, "Driver %s is not known in this build.\n", name);
		addLogAdv(LOG_INFO, LOG_FEATURE_MAIN, "Available drivers: ");
		for (i = 0; i < g_numDrivers; i++) {
			if (i == 0) {
				addLogAdv(LOG_INFO, LOG_FEATURE_MAIN, "%s", g_drivers[i].name);
			}
			else {
				addLogAdv(LOG_INFO, LOG_FEATURE_MAIN, ", %s", g_drivers[i].name);
			}
		}
	}
	DRV_Mutex_Free();
}
// startDriver DGR
// startDriver BL0942
// startDriver BL0937
static commandResult_t DRV_Start(const void* context, const char* cmd, const char* args, int cmdFlags) {
	Tokenizer_TokenizeString(args, 0);
	// following check must be done after 'Tokenizer_TokenizeString',
	// so we know arguments count in Tokenizer. 'cmd' argument is
	// only for warning display
	if (Tokenizer_CheckArgsCountAndPrintWarning(cmd, 1)) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	DRV_StartDriver(Tokenizer_GetArg(0));
	return CMD_RES_OK;
}
static commandResult_t DRV_Stop(const void* context, const char* cmd, const char* args, int cmdFlags) {
	Tokenizer_TokenizeString(args, 0);

	// following check must be done after 'Tokenizer_TokenizeString',
	// so we know arguments count in Tokenizer. 'cmd' argument is
	// only for warning display
	if (Tokenizer_CheckArgsCountAndPrintWarning(cmd, 1)) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	DRV_StopDriver(Tokenizer_GetArg(0));
	return CMD_RES_OK;
}

void DRV_Generic_Init() {
	//cmddetail:{"name":"startDriver","args":"[DriverName]",
	//cmddetail:"descr":"Starts driver",
	//cmddetail:"fn":"DRV_Start","file":"driver/drv_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("startDriver", DRV_Start, NULL);
	//cmddetail:{"name":"stopDriver","args":"[DriverName]",
	//cmddetail:"descr":"Stops driver",
	//cmddetail:"fn":"DRV_Stop","file":"driver/drv_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("stopDriver", DRV_Stop, NULL);
}
void DRV_AppendInformationToHTTPIndexPage(http_request_t* request) {
	int i, j;
	int c_active = 0;

	if (DRV_Mutex_Take(100) == false) {
		return;
	}
	for (i = 0; i < g_numDrivers; i++) {
		if (g_drivers[i].bLoaded) {
			c_active++;
			if (g_drivers[i].appendInformationToHTTPIndexPage) {
				g_drivers[i].appendInformationToHTTPIndexPage(request);
			}
		}
	}
	DRV_Mutex_Free();

	hprintf255(request, "<h5>%i drivers active", c_active);
	if (c_active > 0) {
		j = 0;// printed 0 names so far
		// generate active drivers list in (  )
		hprintf255(request, " (");
		for (i = 0; i < g_numDrivers; i++) {
			if (g_drivers[i].bLoaded) {
				// if at least one name printed, add separator
				if (j != 0) {
					hprintf255(request, ",");
				}
				hprintf255(request, g_drivers[i].name);
				// one more name printed
				j++;
			}
		}
		hprintf255(request, ")");
	}
	hprintf255(request, ", total %i</h5>", g_numDrivers);
}

bool DRV_IsMeasuringPower() {
#ifndef OBK_DISABLE_ALL_DRIVERS
	return DRV_IsRunning("BL0937") || DRV_IsRunning("BL0942") || DRV_IsRunning("CSE7766") || DRV_IsRunning("TESTPOWER");
#else
	return false;
#endif
}
bool DRV_IsMeasuringBattery() {
#ifndef OBK_DISABLE_ALL_DRIVERS
	return DRV_IsRunning("Battery");
#else
	return false;
#endif
}

bool DRV_IsSensor() {
#ifndef OBK_DISABLE_ALL_DRIVERS
	return DRV_IsRunning("SHT3X") || DRV_IsRunning("CHT8305") || DRV_IsRunning("SGP");
#else
	return false;
#endif
}
