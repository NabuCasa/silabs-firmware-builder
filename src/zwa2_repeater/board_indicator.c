// A modification of the SDK's AppsHw board_indicator.c to work with our custom LEDs.
// Blinking is driven by a sleeptimer, matching how the controller firmware controls
// its LEDs.

#include "board_indicator.h"
#include "board_indicator_control.h"
#include "led_adapter.h"
#include "CC_ColorSwitch.h"
#include "em_core.h"
#include "sl_sleeptimer.h"

extern bool m_indicator_active_from_cc;
rgb_t IDLE_COLOR = {4, 0, 0};
rgb_t OFF_COLOR = {0, 0, 0};
rgb_t LEARNMODE_COLOR = {255, 0, 255};
rgb_t DEFAULT_COLOR = {255, 0, 0};

// Active blink state. The timer alternates between the on and off phase until
// the requested number of cycles completes or a new command replaces the blink.
static sl_sleeptimer_timer_handle_t blink_timer;
static rgb_t blink_color;
static rgb_t blink_restore_color;
static uint32_t blink_on_time_ms;
static uint32_t blink_off_time_ms;
static uint32_t blink_cycles_remaining;
static bool blink_stay_on;
static bool blink_phase_on;

static void apply_color(const rgb_t *color)
{
	set_color_buffer(color);
}

static void blink_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
	(void)handle;
	(void)data;

	if (blink_phase_on)
	{
		blink_phase_on = false;
		apply_color(&OFF_COLOR);
		sl_sleeptimer_start_timer_ms(&blink_timer, blink_off_time_ms, blink_timer_callback, NULL, 0, 0);
		return;
	}

	if (blink_cycles_remaining != BLINK_INDEFINITELY)
	{
		blink_cycles_remaining--;
		if (blink_cycles_remaining == 0)
		{
			// Blink finished: restore the color from before the blink
			apply_color(blink_stay_on ? &blink_color : &blink_restore_color);
			m_indicator_active_from_cc = false;
			return;
		}
	}

	blink_phase_on = true;
	apply_color(&blink_color);
	sl_sleeptimer_start_timer_ms(&blink_timer, blink_on_time_ms, blink_timer_callback, NULL, 0, 0);
}

void set_idle_color(rgb_t *color)
{
	IDLE_COLOR.R = color->R;
	IDLE_COLOR.G = color->G;
	IDLE_COLOR.B = color->B;
}

uint8_t cc_color_switch_get_default_value(s_colorComponent* colorComponent) {
	switch (colorComponent->colorId) {
		case ECOLORCOMPONENT_RED:
			return IDLE_COLOR.R;
		case ECOLORCOMPONENT_GREEN:
			return IDLE_COLOR.G;
		case ECOLORCOMPONENT_BLUE:
			return IDLE_COLOR.B;
		default:
			return 0;
	}
}

bool indicator_solid(rgb_t *color)
{
	CORE_DECLARE_IRQ_STATE;
	CORE_ENTER_ATOMIC();
	sl_sleeptimer_stop_timer(&blink_timer);
	apply_color(color);
	m_indicator_active_from_cc = false;
	CORE_EXIT_ATOMIC();
	return true;
}

bool indicator_blink(rgb_t *color, uint32_t on_time_ms, uint32_t off_time_ms, uint32_t num_cycles, bool stay_on)
{
	CORE_DECLARE_IRQ_STATE;
	CORE_ENTER_ATOMIC();
	sl_sleeptimer_stop_timer(&blink_timer);

	// Remember the current color to restore it when a finite blink completes
	get_color_buffer(&blink_restore_color);

	blink_color = *color;
	blink_on_time_ms = on_time_ms;
	blink_off_time_ms = off_time_ms;
	blink_cycles_remaining = num_cycles;
	blink_stay_on = stay_on;
	blink_phase_on = true;

	apply_color(color);
	sl_status_t status = sl_sleeptimer_start_timer_ms(&blink_timer, on_time_ms, blink_timer_callback, NULL, 0, 0);
	CORE_EXIT_ATOMIC();

	return status == SL_STATUS_OK;
}

void Board_IndicateStatus(board_status_t status)
{
	if (status == BOARD_STATUS_LEARNMODE_ACTIVE)
	{
		indicator_blink(
			&LEARNMODE_COLOR,
			500,
			500,
			BLINK_INDEFINITELY,
			false);
	}
	else
	{
		indicator_solid(&IDLE_COLOR);
	}
}

void Board_IndicatorInit(void)
{
	initWs2812();
}

bool Board_IndicatorControl(uint32_t on_time_ms,
							uint32_t off_time_ms,
							uint32_t num_cycles,
							bool called_from_indicator_cc)
{

	// Blink in the current color unless it is off or the Indicator CC asks
	rgb_t color = {0};
	get_color_buffer(&color);

	// Indicator CC indicates indefinite blinking with 0
	if (num_cycles == 0)
	{
		num_cycles = BLINK_INDEFINITELY;
	}

	bool result;
	if (called_from_indicator_cc || (color.R == 0 && color.G == 0 && color.B == 0))
	{
		result = indicator_blink(&DEFAULT_COLOR, on_time_ms, off_time_ms, num_cycles, false);
	}
	else
	{
		result = indicator_blink(&color, on_time_ms, off_time_ms, num_cycles, false);
	}
	if (result)
	{
		m_indicator_active_from_cc = called_from_indicator_cc;
	}

	return result;
}

bool Board_IsIndicatorActive(void)
{
	return m_indicator_active_from_cc;
}

void cc_indicator_handler(uint32_t on_time_ms, uint32_t off_time_ms, uint32_t num_cycles)
{
	// V1 indicator handling
	if (num_cycles == 0 && on_time_ms == 0 && off_time_ms == 0) {
		indicator_solid(&IDLE_COLOR);
		return;
	} else if (num_cycles == 0xff && on_time_ms == 0xff && off_time_ms == 0xff) {
		indicator_solid(&DEFAULT_COLOR);
		return;
	}

	if (num_cycles == 0)
	{
		num_cycles = BLINK_INDEFINITELY;
	}

	m_indicator_active_from_cc = indicator_blink(&DEFAULT_COLOR, on_time_ms, off_time_ms, num_cycles, false);
}
