#include <string.h>
#include "xwa_runtime/input/controller_mapping.h"
enum {CONTROLLER_AXIS_RANGE=65535, CONTROLLER_AXIS_CENTER=32768};
static int Aeron_ControllerAxisDigitalDown(int16_t value, const AeronControllerDigitalSource* source,
										 int was_down) {
	float threshold;
	float normalized;

	if (!(source->threshold > 0.0f && source->threshold <= 1.0f)) {
		return 0;
	}
	threshold = was_down ? source->threshold - 0.1f : source->threshold;
	if (threshold < 0.0f) {
		threshold = 0.0f;
	}
	if (source->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE) {
		normalized = value > 0 ? (float)value / 32767.0f : 0.0f;
	} else {
		normalized = value < 0 ? (float)(-(int32_t)value) / 32768.0f : 0.0f;
	}
	return normalized >= threshold;
}
int Aeron_ControllerDigitalSourceDown(const AeronControllerSnapshot* controller,
									  const AeronControllerDigitalSource* source, int was_down) {
	if (!controller || !controller->connected || !source) {
		return 0;
	}
	switch (source->kind) {
		case AERON_CONTROLLER_DIGITAL_BUTTON:
			if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
				uint32_t bit;
				if (source->index >= AERON_GAMEPAD_BUTTON_COUNT) {
					return 0;
				}
				bit = 1u << source->index;
				return (controller->gamepad_available_buttons & bit) != 0 &&
					   (controller->gamepad_buttons & bit) != 0;
			}
			if (controller->kind == AERON_CONTROLLER_KIND_JOYSTICK &&
				source->index < controller->button_count && source->index < AERON_CONTROLLER_BUTTON_MAX) {
				return (controller->raw_buttons & (UINT64_C(1) << source->index)) != 0;
			}
			return 0;
		case AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE:
		case AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE:
			if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
				if (source->index >= AERON_GAMEPAD_AXIS_COUNT ||
					!(controller->gamepad_available_axes & (1u << source->index))) {
					return 0;
				}
				return Aeron_ControllerAxisDigitalDown(controller->gamepad_axes[source->index], source,
											   was_down);
			}
			if (controller->kind == AERON_CONTROLLER_KIND_JOYSTICK &&
				source->index < controller->axis_count && source->index < AERON_CONTROLLER_AXIS_MAX) {
				return Aeron_ControllerAxisDigitalDown(controller->raw_axes[source->index], source, was_down);
			}
			return 0;
		case AERON_CONTROLLER_DIGITAL_HAT:
			if (controller->kind != AERON_CONTROLLER_KIND_JOYSTICK ||
				source->index >= controller->hat_count || source->index >= AERON_CONTROLLER_HAT_MAX ||
				(source->hat_direction != AERON_CONTROLLER_HAT_UP &&
				 source->hat_direction != AERON_CONTROLLER_HAT_RIGHT &&
				 source->hat_direction != AERON_CONTROLLER_HAT_DOWN &&
				 source->hat_direction != AERON_CONTROLLER_HAT_LEFT)) {
				return 0;
			}
			return (controller->raw_hats[source->index] & source->hat_direction) != 0;
		case AERON_CONTROLLER_DIGITAL_NONE:
		default:
			return 0;
	}
}
static const XwaControllerProfile* ControllerMapping_Profile(const XwaControllerOptions* options,
															 const AeronControllerSnapshot* controller) {
	return controller->kind == AERON_CONTROLLER_KIND_GAMEPAD ? &options->gamepad : &options->joystick;
}
static int16_t ControllerMapping_AxisValue(const AeronControllerSnapshot* controller, int source) {
	if (source < 0) {
		return 0;
	}
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		return source < AERON_GAMEPAD_AXIS_COUNT ? controller->gamepad_axes[source] : 0;
	}
	return source < controller->axis_count && source < AERON_CONTROLLER_AXIS_MAX
			   ? controller->raw_axes[source]
			   : 0;
}
static int ControllerMapping_IsTrigger(const AeronControllerSnapshot* controller, int source) {
	return controller->kind == AERON_CONTROLLER_KIND_GAMEPAD &&
		   (source == AERON_GAMEPAD_AXIS_LEFT_TRIGGER || source == AERON_GAMEPAD_AXIS_RIGHT_TRIGGER);
}
static uint32_t ControllerMapping_CenteredAxis(int16_t value, int invert, float deadzone) {
	double normalized = value < 0 ? (double)value / 32768.0 : (double)value / 32767.0;
	double magnitude;
	uint32_t mapped;

	magnitude = normalized < 0.0 ? -normalized : normalized;
	if (magnitude <= deadzone) {
		return CONTROLLER_AXIS_CENTER;
	}
	/* Shift SDL's complete signed range onto the complete WinMM range without
	 * losing the negative endpoint. */
	mapped = (uint32_t)((int32_t)value + CONTROLLER_AXIS_CENTER);
	return invert ? CONTROLLER_AXIS_RANGE - mapped : mapped;
}
static uint32_t ControllerMapping_TriggerAxis(int16_t value, int invert, float deadzone) {
	double normalized = (double)value / 32767.0;

	if (normalized < 0.0) {
		normalized = 0.0;
	} else if (normalized > 1.0) {
		normalized = 1.0;
	}
	if (normalized <= deadzone) {
		normalized = 0.0;
	}
	if (invert) {
		normalized = 1.0 - normalized;
	}
	return (uint32_t)(normalized * CONTROLLER_AXIS_RANGE);
}
static uint8_t ControllerMapping_Hat(const AeronControllerSnapshot* controller,
									 const XwaControllerProfile* profile) {
	uint8_t hat = AERON_CONTROLLER_HAT_CENTERED;

	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		if (!profile->pov_source) {
			return hat;
		}
		if (controller->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_UP)) {
			hat |= AERON_CONTROLLER_HAT_UP;
		}
		if (controller->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_RIGHT)) {
			hat |= AERON_CONTROLLER_HAT_RIGHT;
		}
		if (controller->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_DOWN)) {
			hat |= AERON_CONTROLLER_HAT_DOWN;
		}
		if (controller->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_LEFT)) {
			hat |= AERON_CONTROLLER_HAT_LEFT;
		}
		return hat;
	}
	if (profile->pov_source >= 0 && profile->pov_source < controller->hat_count &&
		profile->pov_source < AERON_CONTROLLER_HAT_MAX) {
		return controller->raw_hats[profile->pov_source];
	}
	return hat;
}
static int ControllerMapping_PovDirection(uint8_t hat) {
	/* XWA has four POV actions. Vertical wins for diagonal hats. */
	if (hat & AERON_CONTROLLER_HAT_UP) {
		return 0;
	}
	if (hat & AERON_CONTROLLER_HAT_DOWN) {
		return 2;
	}
	if (hat & AERON_CONTROLLER_HAT_RIGHT) {
		return 1;
	}
	if (hat & AERON_CONTROLLER_HAT_LEFT) {
		return 3;
	}
	return -1;
}
static int ControllerMapping_HasPov(const AeronControllerSnapshot* controller,
									const XwaControllerProfile* profile) {
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		return profile->pov_source != 0;
	}
	return profile->pov_source >= 0 && profile->pov_source < controller->hat_count;
}
static void ControllerMapping_MapSnapshot(const XwaControllerOptions* options,
										  const AeronControllerSnapshot* controller, int has_focus,
										  uint32_t previous_axis_buttons, uint32_t* axis_buttons,
										  XwaControllerLogicalState* state) {
	const XwaControllerProfile* profile;
	int logical;

	if (axis_buttons) {
		*axis_buttons = 0;
	}
	if (!state) {
		return;
	}
	memset(state, 0, sizeof(*state));
	state->pov_direction = -1;
	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++logical) {
		state->axes[logical] = CONTROLLER_AXIS_CENTER;
		state->source_axes[logical] = -1;
	}
	if (!options || !controller || !controller->connected) {
		return;
	}
	profile = ControllerMapping_Profile(options, controller);
	state->has_pov = ControllerMapping_HasPov(controller, profile);
	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_AXIS_COUNT; ++logical) {
		const XwaControllerAxisBinding* binding = &profile->axes[logical];
		const int16_t value = ControllerMapping_AxisValue(controller, binding->source);
		state->source_axes[logical] = (int8_t)binding->source;
		state->source_axis_values[logical] = value;
		if (!has_focus) {
			continue;
		}
		state->axes[logical] =
			ControllerMapping_IsTrigger(controller, binding->source)
				? ControllerMapping_TriggerAxis(value, binding->invert, binding->deadzone)
				: ControllerMapping_CenteredAxis(value, binding->invert, binding->deadzone);
	}
	if (!has_focus) {
		return;
	}
	for (logical = 0; logical < XWA_CONTROLLER_LOGICAL_BUTTON_COUNT; ++logical) {
		const AeronControllerDigitalSource* binding = &profile->buttons[logical];
		const uint32_t bit = 1u << logical;
		if (Aeron_ControllerDigitalSourceDown(controller, binding, (previous_axis_buttons & bit) != 0)) {
			state->buttons |= 1u << logical;
			if (axis_buttons && binding->kind != AERON_CONTROLLER_DIGITAL_BUTTON) {
				*axis_buttons |= bit;
			}
		}
	}
	state->pov_direction = ControllerMapping_PovDirection(ControllerMapping_Hat(controller, profile));
}
void XwaControllerMapping_MapSnapshot(const XwaControllerOptions* options,
									  const AeronControllerSnapshot* controller, int has_focus,
									  XwaControllerLogicalState* state) {
	ControllerMapping_MapSnapshot(options, controller, has_focus, 0, NULL, state);
}
