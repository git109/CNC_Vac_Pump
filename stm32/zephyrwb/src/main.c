/*
 * CNC Vacuum Pump Controller — Zephyr port (STM32WB55 / P-NUCLEO-WB55 / LVGL 9.5)
 *
 * Replaces the Arduino setup()/loop() with:
 *   - a k_timer + workqueue running the 10 ms vacuum control loop, and
 *   - the LVGL handler loop driving the round display.
 *
 * The UI here is a minimal LVGL-9 PLACEHOLDER so the project builds and runs
 * before the SquareLine artwork is re-exported for LVGL 9. Once src/ui/ exists,
 * enable it in CMakeLists.txt and replace ui_placeholder()/ui_update() with
 * ui_init() plus the ui_* widget calls (needle, bugs, LED).
 *
 * Heartbeat uses the onboard green LED (LD2 / board alias led1).
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <stdio.h>

#include "vac_filter.h"
#include "app_state.h"
#include "ui/ui.h"

LOG_MODULE_REGISTER(vac, LOG_LEVEL_INF);

/* ---- Hardware handles from devicetree ---------------------------------- */
static const struct gpio_dt_spec pump  = GPIO_DT_SPEC_GET(DT_ALIAS(vac_pump),  gpios);
static const struct gpio_dt_spec alarm = GPIO_DT_SPEC_GET(DT_ALIAS(vac_alarm), gpios);
static const struct gpio_dt_spec grn   = GPIO_DT_SPEC_GET(DT_ALIAS(led1),      gpios); /* onboard green LED */
static const struct gpio_dt_spec bl    = GPIO_DT_SPEC_GET(DT_ALIAS(lcd_bl),    gpios);
#ifndef CONFIG_VAC_SIM
static const struct adc_dt_spec  vac_adc =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
#endif

/* Latest values shared with the UI loop: computed vacuum (inHg * 10) and the
 * filtered raw ADC reading (0..4090, used for the needle angle). */
static volatile int g_vac_val;
static volatile int g_vac_raw;

/* ---- 10 ms vacuum control loop (was the tm4 block in loop()) ------------ */
static void vac_work_handler(struct k_work *w)
{
	ARG_UNUSED(w);

	uint16_t raw;

#ifdef CONFIG_VAC_SIM
	/* Synthetic triangle sweep 0..4090..0 to exercise the UI + relay logic
	 * without a real sensor. ~2 s per direction at the 10 ms tick. */
	static int sim_val;
	static int sim_dir = 1;

	sim_val += sim_dir * 20;
	if (sim_val >= 4090) {
		sim_val = 4090;
		sim_dir = -1;
	} else if (sim_val <= 0) {
		sim_val = 0;
		sim_dir = 1;
	}
	raw = (uint16_t)sim_val;
#else
	int16_t sample = 0;
	struct adc_sequence seq = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
	};

	adc_sequence_init_dt(&vac_adc, &seq);
	if (adc_read_dt(&vac_adc, &seq) != 0) {
		return;
	}
	raw = (sample < 0) ? 0 : (uint16_t)sample;
#endif

	uint16_t filt = vac_filter_push(raw);
	if (filt > 4090) {
		filt = 4090;
	}

	g_vac_raw = filt;

	/* Two-point calibration (measured 2026-08-02) against the shop vacuum gauge on
	 * the VB2200 manifold. filt (ADC counts) -> vacuum in inHg*10, linear:
	 *   atmosphere   filt=4044 -> 0.0 inHg
	 *   full vacuum  filt=709  -> 29.0 inHg   (sealed line, reference gauge = 29")
	 * vac(inHg*10) = 290 * (4044 - filt) / (4044 - 709). Both endpoints are real
	 * measurements, so the divider ratio and VREF drop out. Clamp 0..300. */
	const int cnt_atm = 4044;   /* counts at atmosphere  -> 0.0 inHg  */
	const int cnt_ref = 709;    /* counts at full vacuum -> 29.0 inHg */
	const int ref_x10 = 290;    /* 29.0 inHg (inHg * 10)              */
	int vac_val = (ref_x10 * (cnt_atm - (int)filt)) / (cnt_atm - cnt_ref);
	if (vac_val < 0) {
		vac_val = 0;
	} else if (vac_val > 300) {
		vac_val = 300;
	}
	g_vac_val = vac_val;

	if (vac_val <= app_red) {
		gpio_pin_set_dt(&pump, 1);   /* On  (active-low handled by DT) */
	}
	if (vac_val >= app_grn) {
		gpio_pin_set_dt(&pump, 0);   /* Off */
	}

	/* TODO: drive the alarm relay (never implemented in the original). */
}
K_WORK_DEFINE(vac_work, vac_work_handler);

static void vac_timer_fn(struct k_timer *t)
{
	ARG_UNUSED(t);
	k_work_submit(&vac_work);
}
K_TIMER_DEFINE(vac_timer, vac_timer_fn, NULL);

/* ---- Drive the SquareLine (LVGL 9) widgets ----------------------------- */
/* Angle units are 0.1 deg (LVGL: 0..3600), matching the original firmware. */
static void ui_update(void)
{
	char buf[12];
	int v = g_vac_val;
	int av = (v < 0) ? -v : v;

	/* Vacuum value label, e.g. "24.5". */
	snprintf(buf, sizeof(buf), "%d.%d", v / 10, av % 10);
	lv_label_set_text(ui_vacLabel, buf);

	/* Needle: calibrated vacuum (inHg*10) mapped to the gauge face. The needle
	 * image sits 135 deg off the setpoint-bug frame, so 0 inHg -> 270.0 deg (dial
	 * start), 15 -> 135.0 deg (center), 30 inHg -> 0 deg. Verified against the
	 * original raw*2700/4096 mapping (atmosphere ~270 deg) + on-screen check. */
	lv_image_set_rotation(ui_Needle, (int32_t)(2700 - 9 * v));

	/* Setpoint "bugs": map inHg*10 (300..0) -> -135.0..135.0 deg. */
	lv_image_set_rotation(ui_redBugImg, (int32_t)(1350 - 9 * app_red));
	lv_image_set_rotation(ui_grnBugImg, (int32_t)(1350 - 9 * app_grn));

	/* Red LED: shown at/below the pump-on setpoint, hidden at/above pump-off. */
	if (v <= app_red) {
		lv_obj_remove_flag(ui_ledRedImage, LV_OBJ_FLAG_HIDDEN);
	}
	if (v >= app_grn) {
		lv_obj_add_flag(ui_ledRedImage, LV_OBJ_FLAG_HIDDEN);
	}
}

/* ------------------------------------------------------------------------ */
int main(void)
{
	const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(disp)) {
		LOG_ERR("display device not ready");
		return 0;
	}
#ifndef CONFIG_VAC_SIM
	if (!adc_is_ready_dt(&vac_adc)) {
		LOG_ERR("ADC not ready");
		return 0;
	}
	adc_channel_setup_dt(&vac_adc);
#endif

	gpio_pin_configure_dt(&pump,  GPIO_OUTPUT_INACTIVE);   /* pump Off */
	gpio_pin_configure_dt(&alarm, GPIO_OUTPUT_INACTIVE);   /* alarm Off */
	gpio_pin_configure_dt(&grn,   GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&bl,    GPIO_OUTPUT_ACTIVE);      /* backlight on */

	ui_init();                   /* SquareLine LVGL-9 UI */
	lv_timer_handler();
	display_blanking_off(disp);

	encoder_start();
	k_timer_start(&vac_timer, K_MSEC(10), K_MSEC(10));

	LOG_INF("CNC Vac Pump Controller up (LVGL %d.%d.%d)",
		lv_version_major(), lv_version_minor(), lv_version_patch());
#ifdef CONFIG_VAC_SIM
	LOG_WRN("VAC_SIM enabled: synthetic sensor sweep (no ADC)");
#endif

	uint32_t last_ui = 0;

	while (1) {
		uint32_t now = k_uptime_get_32();

		if (now - last_ui >= 100) {      /* refresh readout ~10 Hz */
			last_ui = now;
			ui_update();
			gpio_pin_toggle_dt(&grn);    /* green-LED heartbeat */
		}

		uint32_t idle = lv_timer_handler();
		if (idle < 5) {
			idle = 5;
		} else if (idle > 20) {
			idle = 20;
		}
		k_msleep(idle);
	}

	return 0;
}
