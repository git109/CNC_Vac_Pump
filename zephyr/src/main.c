/*
 * CNC Vacuum Pump Controller — Zephyr port (ESP32-S3 / LVGL 9.5)
 *
 * Replaces the Arduino setup()/loop() with:
 *   - a k_timer + workqueue running the 10 ms vacuum control loop, and
 *   - the LVGL handler loop driving the round display.
 *
 * The UI here is a minimal LVGL-9 PLACEHOLDER so the project builds and runs
 * before the SquareLine artwork is re-exported for LVGL 9. Once src/ui/ exists,
 * enable it in CMakeLists.txt and replace ui_placeholder()/ui_update() with
 * ui_init() plus the ui_* widget calls (needle, bugs, LED).
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <stdio.h>

#include "vac_filter.h"
#include "app_state.h"

LOG_MODULE_REGISTER(vac, LOG_LEVEL_INF);

/* ---- Hardware handles from devicetree ---------------------------------- */
static const struct gpio_dt_spec pump  = GPIO_DT_SPEC_GET(DT_ALIAS(vac_pump),  gpios);
static const struct gpio_dt_spec alarm = GPIO_DT_SPEC_GET(DT_ALIAS(vac_alarm), gpios);
static const struct gpio_dt_spec led   = GPIO_DT_SPEC_GET(DT_ALIAS(debug_led), gpios);
static const struct gpio_dt_spec bl    = GPIO_DT_SPEC_GET(DT_ALIAS(lcd_bl),    gpios);
static const struct adc_dt_spec  vac_adc =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(led_strip));

/* Latest computed vacuum value (inHg * 10), shared with the UI loop. */
static volatile int g_vac_val;

/* ---- 10 ms vacuum control loop (was the tm4 block in loop()) ------------ */
static void vac_work_handler(struct k_work *w)
{
	ARG_UNUSED(w);

	int16_t sample = 0;
	struct adc_sequence seq = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
	};

	adc_sequence_init_dt(&vac_adc, &seq);
	if (adc_read_dt(&vac_adc, &seq) != 0) {
		return;
	}

	uint16_t raw = (sample < 0) ? 0 : (uint16_t)sample;
	uint16_t filt = vac_filter_push(raw);
	if (filt > 4090) {
		filt = 4090;
	}

	/* map(filt, 0, 4090, 300, 0) -> inHg*10 */
	int vac_val = (300 * (4090 - (int)filt)) / 4090;
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

/* ---- Placeholder LVGL 9 UI --------------------------------------------- */
static lv_obj_t *lbl_value;
static lv_obj_t *lbl_setpoints;

static void ui_placeholder(void)
{
	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

	lbl_value = lv_label_create(scr);
	lv_obj_set_style_text_color(lbl_value, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_text_font(lbl_value, &lv_font_montserrat_28, LV_PART_MAIN);
	lv_obj_align(lbl_value, LV_ALIGN_CENTER, 0, -12);
	lv_label_set_text(lbl_value, "--.-");

	lbl_setpoints = lv_label_create(scr);
	lv_obj_set_style_text_color(lbl_setpoints, lv_color_hex(0x30D040), LV_PART_MAIN);
	lv_obj_align(lbl_setpoints, LV_ALIGN_CENTER, 0, 28);
	lv_label_set_text(lbl_setpoints, "R --.-  G --.-");
}

static void ui_update(void)
{
	char b[24];
	int v = g_vac_val;
	int av = (v < 0) ? -v : v;

	snprintf(b, sizeof(b), "%d.%d", v / 10, av % 10);
	lv_label_set_text(lbl_value, b);

	snprintf(b, sizeof(b), "R %d.%d  G %d.%d",
		 app_red / 10, app_red % 10, app_grn / 10, app_grn % 10);
	lv_label_set_text(lbl_setpoints, b);
}

/* ------------------------------------------------------------------------ */
int main(void)
{
	const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(disp)) {
		LOG_ERR("display device not ready");
		return 0;
	}
	if (!adc_is_ready_dt(&vac_adc)) {
		LOG_ERR("ADC not ready");
		return 0;
	}
	adc_channel_setup_dt(&vac_adc);

	gpio_pin_configure_dt(&pump,  GPIO_OUTPUT_INACTIVE);   /* pump Off */
	gpio_pin_configure_dt(&alarm, GPIO_OUTPUT_INACTIVE);   /* alarm Off */
	gpio_pin_configure_dt(&led,   GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&bl,    GPIO_OUTPUT_ACTIVE);      /* backlight on */

	ui_placeholder();            /* TODO: ui_init() after LVGL-9 re-export */
	lv_timer_handler();
	display_blanking_off(disp);

	encoder_start();
	k_timer_start(&vac_timer, K_MSEC(10), K_MSEC(10));

	LOG_INF("CNC Vac Pump Controller up (LVGL %d.%d.%d)",
		lv_version_major(), lv_version_minor(), lv_version_patch());

	uint32_t last_ui = 0;
	bool hb = false;

	while (1) {
		uint32_t now = k_uptime_get_32();

		if (now - last_ui >= 100) {      /* refresh readout ~10 Hz */
			last_ui = now;
			ui_update();

			/* Heartbeat: blink the onboard RGB LED green (GPIO2 has no
			 * physical LED on this board, but keep it toggling too). */
			hb = !hb;
			gpio_pin_toggle_dt(&led);
			if (device_is_ready(strip)) {
				struct led_rgb px = { .r = 0, .g = hb ? 16 : 0, .b = 0 };
				led_strip_update_rgb(strip, &px, 1);
			}
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
