/*
 * Rotary-encoder task (Option B: faithful port of the original polling loop).
 *
 * Reads A/B/SW as raw levels (pull-ups enabled, idle = HIGH) exactly like the
 * Arduino digitalRead() version, counts on falling edges, and keeps the two
 * setpoint "bugs" ordered (green always >= red + 5). The button cycles the
 * active bug.
 *
 * Later you can swap this for the idiomatic gpio-qdec + gpio-keys input drivers
 * feeding an LVGL encoder indev (CONFIG_LV_Z_ENCODER_INPUT) — see README.
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "app_state.h"

#define RED_BUG 1
#define GRN_BUG 2
#define MAX_CNT 295
#define MIN_CNT 5

/* Shared setpoints (inHg * 10). Defaults match the original firmware. */
volatile int16_t app_red = 200;
volatile int16_t app_grn = 250;

#define ENC_NODE DT_PATH(zephyr_user)
static const struct gpio_dt_spec enc_a  = GPIO_DT_SPEC_GET(ENC_NODE, enc_a_gpios);
static const struct gpio_dt_spec enc_b  = GPIO_DT_SPEC_GET(ENC_NODE, enc_b_gpios);
static const struct gpio_dt_spec enc_sw = GPIO_DT_SPEC_GET(ENC_NODE, enc_sw_gpios);

#define ENC_STACK_SIZE 2048
#define ENC_PRIORITY   5
static K_THREAD_STACK_DEFINE(enc_stack, ENC_STACK_SIZE);
static struct k_thread enc_thread_data;

static void enc_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	gpio_pin_configure_dt(&enc_a,  GPIO_INPUT);
	gpio_pin_configure_dt(&enc_b,  GPIO_INPUT);
	gpio_pin_configure_dt(&enc_sw, GPIO_INPUT);

	int16_t cnt = app_grn, cnt_old = app_grn;
	uint8_t active_bug = GRN_BUG;
	bool cnt_lck = false;
	int sw_old = 1;

	while (true) {
		int a  = gpio_pin_get_dt(&enc_a);   /* raw level: 1 = idle/high */
		int b  = gpio_pin_get_dt(&enc_b);
		int sw = gpio_pin_get_dt(&enc_sw);

		/* Count up on falling edge of A with B still high. */
		if (!a && b) {
			if (!cnt_lck) {
				if (cnt < MAX_CNT) {
					cnt += 5;
				}
				if (cnt > MAX_CNT) {
					cnt = MAX_CNT;
				}
				cnt_lck = true;
			}
		}

		/* Count down on falling edge of B with A still high. */
		if (a && !b) {
			if (!cnt_lck) {
				if (cnt > MIN_CNT) {
					cnt -= 5;
				}
				if (cnt < MIN_CNT) {
					cnt = MIN_CNT;
				}
				cnt_lck = true;
			}
		}

		/* Release the lockout only when both channels return high. */
		if (a && b) {
			cnt_lck = false;
		}

		/* Button press (active low) cycles the active bug. */
		if (sw == 0 && sw_old == 1) {
			if (++active_bug > GRN_BUG) {
				active_bug = RED_BUG;
				cnt = app_red;
			} else {
				cnt = app_grn;
			}
		}
		sw_old = sw;

		/* Sync counter into the active bug. */
		if (active_bug == RED_BUG) {
			app_red = cnt;
		} else {
			app_grn = cnt;
		}

		/* Push the inactive bug if the two would cross. */
		if (active_bug == RED_BUG && app_red >= app_grn) {
			app_grn = app_red + 5;
		}
		if (active_bug == GRN_BUG && app_grn <= app_red) {
			app_red = app_grn - 5;
		}

		cnt_old = cnt;
		ARG_UNUSED(cnt_old);
		k_msleep(10);
	}
}

void encoder_start(void)
{
	k_thread_create(&enc_thread_data, enc_stack, ENC_STACK_SIZE,
			enc_thread, NULL, NULL, NULL,
			ENC_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&enc_thread_data, "encoder");
}
