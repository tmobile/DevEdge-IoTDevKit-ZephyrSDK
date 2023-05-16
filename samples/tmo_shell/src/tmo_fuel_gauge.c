#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tmo_fuel_gauge, CONFIG_LOG_DEFAULT_LEVEL);

#if DT_NODE_EXISTS(DT_NODELABEL(pmic))
#define PMIC_EXIT_MODE_VALUE				0b000
#define PMIC_RESET_MODE_VALUE				0b001
#define PMIC_SHORT_MODE_VALUE				0b010
#define PMIC_PRECOND_MODE_VALUE				0b011
#define PMIC_FASTCHARGE_CC_MODE_VALUE		0b100
#define PMIC_FASTCHARGE_CV_MODE_VALUE		0b101
#define PMIC_END_OF_CHARGE_MODE_VALUE		0b101
#define PMIC_FAULT_MODE_VALUE				0b111
#include <zephyr/drivers/fuel_gauge/act81461.h>

static const struct device *alpc = DEVICE_DT_GET_ANY(qorvo_act81461_alpc);

int get_pmic_status(uint8_t *charging, uint8_t *vbus, uint8_t *attached, uint8_t *fault, uint8_t *charge_status)
{
	int ret;
	struct fuel_gauge_get_property props[4] = {0};

	props[0].property_type = FUEL_GAUGE_STATUS;
	props[1].property_type = FUEL_GAUGE_CONNECT_STATE;
	props[2].property_type = FUEL_GAUGE_PRESENT_STATE;
	props[3].property_type = FUEL_GAUGE_MODE;

	ret = fuel_gauge_get_prop(alpc, props, ARRAY_SIZE(props));

	if (ret) {
		return ret;
	}

	if (charging) {
		*charging = (props[0].value.flags & FUEL_GAUGE_STATUS_FLAGS_DISCHARGING)? 0 : 1;
	}
	if (vbus) {
		*vbus = props[1].value.flags;
	}
	if (attached) {
		*attached = props[2].value.flags;
	}
	if (fault) {
		*fault = props[3].value.mode == PMIC_FAULT_MODE_VALUE;
	}
	if (charge_status) {
		*charge_status  = props[3].value.mode;
	}

	return 0;
}

#if CONFIG_TMO_FUEL_GAUGE_STATE_CHANGE_PRINT
static void print_pmic_status(const uint8_t charging, const uint8_t vbus, const uint8_t attached,
		const uint8_t fault, const uint8_t charge_status)
{
	if (vbus == 1) {
		LOG_INF("PMIC VBUS is detected");
	}
	else {
		LOG_INF("No PMIC VBUS is detected");
	}

	/* Battery removed/inserted */
	if (attached == 1) {
		LOG_INF("Battery has been detected");
	}
	else {
		LOG_INF("No battery is detected");
	}

	if ((attached) && (vbus))
	{
		/* Charge condition / state change – Precondition, fast charge, top off and end of charge reached. */
		switch(charge_status)
		{
			case PMIC_EXIT_MODE_VALUE:
				LOG_INF("Charger has exited charge mode");
				break;
			case PMIC_RESET_MODE_VALUE:
				LOG_INF("Charger is in reset mode");
				break;
			case PMIC_SHORT_MODE_VALUE:
				LOG_INF("Charger is in VBAT SHORT mode");
				break;
			case PMIC_PRECOND_MODE_VALUE:
				LOG_INF("Charger is in VBAT PRECOND mode");
				break;
			case PMIC_FASTCHARGE_CC_MODE_VALUE:
			case PMIC_FASTCHARGE_CV_MODE_VALUE:
				LOG_INF("Battery is in FAST charge mode - charging");
				break;
			case PMIC_END_OF_CHARGE_MODE_VALUE:
				LOG_INF("Battery end of charge has been detected");
				break;
			case PMIC_FAULT_MODE_VALUE:
				LOG_INF("Battery fault detected");
				break;
			default:
				break;
		}
	}
}

void pmic_state_print(const struct device *dev)
{
	ARG_UNUSED(dev);

	uint8_t charging, vbus, attached, fault, status;

	get_pmic_status(&charging, &vbus, &attached, &fault, &status);
	print_pmic_status(charging, vbus, attached, fault, status);
}

struct act81461_int_cb pmic_charge_cb = {
	cb = pmic_state_print
};

static int fuel_guage_init_state_print(const struct device *unused)
{
	ARG_UNUSED(unused);

	act81461_charger_int_cb_register(alpc, &pmic_charge_cb);

	return 0;
}

SYS_INIT(fuel_guage_init_state_print, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif
#endif

