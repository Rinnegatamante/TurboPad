#include <vitasdk.h>
#include <taihen.h>
#include <libk/string.h>
#include <libk/stdio.h>
#include "renderer.h"

#define HOOKS_NUM    13 // Hooked functions num
#define BUTTONS_NUM  16 // Supported buttons num
#define TYPES_NUM    5  // Turbo modes num
#define MENU_ENTRIES 17 // Config menu entries num

// Currently available turbo modes
#define TURBO_DISABLED   0
#define TURBO_PER_TICK   1
#define TURBO_PER_MSEC   2 // 200 msecs
#define TURBO_PER_MSEC_2 3 // 500 msecs
#define TURBO_PER_SEC    4 // 1000 msecs

static uint8_t show_menu = 0;
static uint8_t current_hook = 0;
static SceUID hooks[HOOKS_NUM];
static tai_hook_ref_t refs[HOOKS_NUM];
static uint64_t last_tick1, last_tick2, last_tick3;
static int cfg_i = 0;
static uint32_t old_buttons;
static uint8_t update_tick1, update_tick2, update_tick3;
static char titleid[16];
static char fname[128];
static char btn_mask[BUTTONS_NUM];

typedef struct ctrlSetting{
	uint32_t code;
	uint8_t states[HOOKS_NUM - 1];
	uint8_t type;
}ctrlSetting;

ctrlSetting turboTable[BUTTONS_NUM];

static uint32_t btns[BUTTONS_NUM] = {
	SCE_CTRL_CROSS, SCE_CTRL_CIRCLE, SCE_CTRL_TRIANGLE, SCE_CTRL_SQUARE,
	SCE_CTRL_START, SCE_CTRL_SELECT, SCE_CTRL_LTRIGGER, SCE_CTRL_RTRIGGER,
	SCE_CTRL_UP, SCE_CTRL_RIGHT, SCE_CTRL_LEFT, SCE_CTRL_DOWN, SCE_CTRL_L1,
	SCE_CTRL_R1, SCE_CTRL_L3, SCE_CTRL_R3
};

static char* str_btns[BUTTONS_NUM] = {
	"Cross", "Circle", "Triangle", "Square",
	"Start", "Select", "L Trigger (L2)", "R Trigger (R2)",
	"Up", "Right", "Left", "Down", "L1", "R1", "L3", "R3"
};

static char* str_types[TYPES_NUM] = {
	"Disabled", "Each input tick", "Every 200 msecs", "Every 500 msecs", "Every 1000 msecs"
};

// Config Menu Renderer
void drawConfigMenu(){
	drawString(5, 30, "Thanks to XandridFire and RaveHeart for their support on Patreon");
	drawString(5, 50, "TurboPad v.0.3 - CONFIG MENU");
	int i;
	for (i = 0; i < BUTTONS_NUM; i++){
		(i == cfg_i) ? setTextColor(0x0000FF00) : setTextColor(0x00FFFFFF);
		drawStringF(5, 70 + i*20, "%s : %s", str_btns[i], str_types[turboTable[i].type]);
	}
	(i == cfg_i) ? setTextColor(0x0000FF00) : setTextColor(0x00FFFFFF);
	drawString(5, 70 + BUTTONS_NUM*20, "Close Config Menu");
	setTextColor(0x00FF00FF);
}

void applyTurbo(SceCtrlData *ctrl, uint8_t j){
	
	// Checking for menu triggering
	if ((ctrl->buttons & SCE_CTRL_START) && (ctrl->buttons & SCE_CTRL_TRIANGLE)){
		show_menu = 1;
		cfg_i = 0;
		return;
	}
	
	// Applying rapidfire rules
	int i;
	uint64_t tick = sceKernelGetProcessTimeWide();
	if (tick - last_tick1 > 100000){
		update_tick1 = 1;
		last_tick1 = tick;
	}
	if (tick - last_tick2 > 250000){
		update_tick2 = 1;
		last_tick2 = tick;
	}
	if (tick - last_tick3 > 500000){
		update_tick3 = 1;
		last_tick3 = tick;
	}
	for (i=0;i<BUTTONS_NUM;i++){
		switch (turboTable[i].type){
			case TURBO_DISABLED:
				continue;
				break;
			case TURBO_PER_MSEC:
				if (!update_tick1) continue;
				break;
			case TURBO_PER_MSEC_2:
				if (!update_tick2) continue;
				break;
			case TURBO_PER_SEC:
				if (!update_tick3) continue;
				break;
			default:
				break;
		}
		if (ctrl->buttons & turboTable[i].code){
			turboTable[i].states[j] = (turboTable[i].states[j] + 1) % 2;
			ctrl->buttons = ctrl->buttons - (turboTable[i].code * turboTable[i].states[j]);
		}else turboTable[i].states[j] = 0;
	}
	
	// Resetting tick flags
	update_tick1 = 0;
	update_tick2 = 0;
	update_tick3 = 0;
	
}

void applyTurboNegative(SceCtrlData *ctrl, uint8_t j){
	
	// Checking for menu triggering
	if ((!(ctrl->buttons & SCE_CTRL_START)) && (!(ctrl->buttons & SCE_CTRL_TRIANGLE))){
		show_menu = 1;
		cfg_i = 0;
		return;
	}
	
	// Applying rapidfire rules
	int i;
	uint64_t tick = sceKernelGetProcessTimeWide();
	if (tick - last_tick1 > 100000){
		update_tick1 = 1;
		last_tick1 = tick;
	}
	if (tick - last_tick2 > 250000){
		update_tick2 = 1;
		last_tick2 = tick;
	}
	if (tick - last_tick3 > 500000){
		update_tick3 = 1;
		last_tick3 = tick;
	}
	for (i=0;i<BUTTONS_NUM;i++){
		switch (turboTable[i].type){
			case TURBO_DISABLED:
				continue;
				break;
			case TURBO_PER_MSEC:
				if (!update_tick1) continue;
				break;
			case TURBO_PER_MSEC_2:
				if (!update_tick2) continue;
				break;
			case TURBO_PER_SEC:
				if (!update_tick3) continue;
				break;
			default:
				break;
		}
		if (!(ctrl->buttons & turboTable[i].code)){
			turboTable[i].states[j] = (turboTable[i].states[j] + 1) % 2;
			ctrl->buttons = ctrl->buttons + (turboTable[i].code * turboTable[i].states[j]);
		}else turboTable[i].states[j] = 0;
	}
	
	// Resetting tick flags
	update_tick1 = 0;
	update_tick2 = 0;
	update_tick3 = 0;
	
}

void saveConfig(void){
	
	// Opening config file for the running app
	sprintf(fname, "ux0:/data/TurboPad/%s.bin", titleid);
	SceUID fd = sceIoOpen(fname, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	
	// Populating buttons mask
	int i = 0;
	while (i < BUTTONS_NUM){
		btn_mask[i] = turboTable[i].type;
		i++;
	}
	
	// Saving buttons mask
	sceIoWrite(fd, btn_mask, BUTTONS_NUM);
	sceIoClose(fd);
	
}

void loadConfig(void){
	
	sceIoMkdir("ux0:/data/TurboPad", 0777); // Just in case the folder doesn't exist
	
	// Getting game Title ID
	sceAppMgrAppParamGetString(0, 12, titleid , 256);
	
	// Loading config file for the selected app if exists
	sprintf(fname, "ux0:/data/TurboPad/%s.bin", titleid);
	SceUID fd = sceIoOpen(fname, SCE_O_RDONLY, 0777);
	memset(btn_mask, 0, BUTTONS_NUM);
	if (fd >= 0){
		sceIoRead(fd, btn_mask, BUTTONS_NUM);
		sceIoClose(fd);
	}
	
	// Populating turboTable
	int j, i = 0;
	while (i < BUTTONS_NUM){
		turboTable[i].code = btns[i];
		turboTable[i].type = btn_mask[i];
		j = 0;
		while (j < 4){
			turboTable[i].states[j++] = 0;
		}
		i++;
	}
	
	// Setting starting tick
	last_tick1 = sceKernelGetProcessTimeWide();
	last_tick2 = last_tick1;
	last_tick3 = last_tick1;
	
}

// Input Handler for the Config Menu
void configInputHandler(SceCtrlData *ctrl){
	if ((ctrl->buttons & SCE_CTRL_DOWN) && (!(old_buttons & SCE_CTRL_DOWN))){
		cfg_i++;
		if (cfg_i >= MENU_ENTRIES) cfg_i = 0;
	}else if ((ctrl->buttons & SCE_CTRL_UP) && (!(old_buttons & SCE_CTRL_UP))){
		cfg_i--;
		if (cfg_i < 0) cfg_i = MENU_ENTRIES-1;
	}else if ((ctrl->buttons & SCE_CTRL_CROSS) && (!(old_buttons & SCE_CTRL_CROSS))){
		if (cfg_i == MENU_ENTRIES-1){ 
			show_menu = 0;
			saveConfig();
		}else turboTable[cfg_i].type = (turboTable[cfg_i].type + 1) % TYPES_NUM;
	}
	old_buttons = ctrl->buttons;
	ctrl->buttons = 0; // Nulling returned buttons
}

// Input Handler for the Config Menu (negative logic)
void configInputHandlerNegative(SceCtrlData *ctrl){
	if ((old_buttons & SCE_CTRL_DOWN) && (!(ctrl->buttons & SCE_CTRL_DOWN))){
		cfg_i++;
		if (cfg_i >= MENU_ENTRIES) cfg_i = 0;
	}else if ((old_buttons & SCE_CTRL_UP) && (!(ctrl->buttons & SCE_CTRL_UP))){
		cfg_i--;
		if (cfg_i < 0) cfg_i = MENU_ENTRIES-1;
	}else if ((old_buttons & SCE_CTRL_CROSS) && (!(ctrl->buttons & SCE_CTRL_CROSS))){
		if (cfg_i == MENU_ENTRIES-1){ 
			show_menu = 0;
			saveConfig();
		}else turboTable[cfg_i].type = (turboTable[cfg_i].type + 1) % TYPES_NUM;
	}
	old_buttons = ctrl->buttons;
	ctrl->buttons = 0xFFFFFFFF; // Nulling returned buttons
}

// Simplified generic hooking function
void hookFunction(uint32_t nid, const void *func){
	hooks[current_hook] = taiHookFunctionImport(&refs[current_hook],TAI_MAIN_MODULE,TAI_ANY_LIBRARY,nid,func);
	current_hook++;
}

int sceCtrlPeekBufferPositive_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[0], port, ctrl, count);
	if (!show_menu) applyTurbo(ctrl, 0);
	else configInputHandler(ctrl);
	return ret;
}

int sceCtrlPeekBufferPositive2_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[1], port, ctrl, count);
	if (!show_menu) applyTurbo(ctrl, 1);
	else configInputHandler(ctrl);
	return ret;
}

int sceCtrlReadBufferPositive_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[2], port, ctrl, count);
	if (!show_menu) applyTurbo(ctrl, 2);
	else configInputHandler(ctrl);
	return ret;
}

int sceCtrlReadBufferPositive2_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[3], port, ctrl, count);
	if (!show_menu) applyTurbo(ctrl, 3);
	else configInputHandler(ctrl);
	return ret;
}

int sceCtrlPeekBufferPositiveExt_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[4], port, ctrl, count);
	if (!show_menu) applyTurbo(ctrl, 4);
	else configInputHandler(ctrl);
	return ret;
}

int sceCtrlPeekBufferPositiveExt2_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[5], port, ctrl, count);
	if (!show_menu) applyTurbo(ctrl, 5);
	else configInputHandler(ctrl);
	return ret;
}

int sceCtrlReadBufferPositiveExt_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[6], port, ctrl, count);
	if (!show_menu) applyTurbo(ctrl, 6);
	else configInputHandler(ctrl);
	return ret;
}

int sceCtrlReadBufferPositiveExt2_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[7], port, ctrl, count);
	if (!show_menu) applyTurbo(ctrl, 7);
	else configInputHandler(ctrl);
	return ret;
}

int sceCtrlPeekBufferNegative_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[8], port, ctrl, count);
	if (!show_menu) applyTurboNegative(ctrl, 8);
	else configInputHandlerNegative(ctrl);
	return ret;
}

int sceCtrlPeekBufferNegative2_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[9], port, ctrl, count);
	if (!show_menu) applyTurboNegative(ctrl, 9);
	else configInputHandlerNegative(ctrl);
	return ret;
}

int sceCtrlReadBufferNegative_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[10], port, ctrl, count);
	if (!show_menu) applyTurboNegative(ctrl, 10);
	else configInputHandlerNegative(ctrl);
	return ret;
}

int sceCtrlReadBufferNegative2_patched(int port, SceCtrlData *ctrl, int count) {
	int ret = TAI_CONTINUE(int, refs[11], port, ctrl, count);
	if (!show_menu) applyTurboNegative(ctrl, 11);
	else configInputHandlerNegative(ctrl);
	return ret;
}

int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf *pParam, int sync) {
	if (show_menu){
		updateFramebuf(pParam);
		drawConfigMenu();
	}
	return TAI_CONTINUE(int, refs[12], pParam, sync);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	
	// Setup stuffs
	loadConfig();
	
	// Hooking functions
	hookFunction(0xA9C3CED6, sceCtrlPeekBufferPositive_patched);
	hookFunction(0x15F81E8C, sceCtrlPeekBufferPositive2_patched);
	hookFunction(0x67E7AB83, sceCtrlReadBufferPositive_patched);
	hookFunction(0xC4226A3E, sceCtrlReadBufferPositive2_patched);
	hookFunction(0xA59454D3, sceCtrlPeekBufferPositiveExt_patched);
	hookFunction(0x860BF292, sceCtrlPeekBufferPositiveExt2_patched);
	hookFunction(0xE2D99296, sceCtrlReadBufferPositiveExt_patched);
	hookFunction(0xA7178860, sceCtrlReadBufferPositiveExt2_patched);
	hookFunction(0x104ED1A7, sceCtrlPeekBufferNegative_patched);
	hookFunction(0x81A89660, sceCtrlPeekBufferNegative2_patched);
	hookFunction(0x15F96FB0, sceCtrlReadBufferNegative_patched);
	hookFunction(0x27A0C5FB, sceCtrlReadBufferNegative2_patched);
	hookFunction(0x7A410B64, sceDisplaySetFrameBuf_patched);
	
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {

	// Freeing hooks
	while (current_hook-- > 0){
		taiHookRelease(hooks[current_hook], refs[current_hook]);
	}
		
	return SCE_KERNEL_STOP_SUCCESS;
	
}