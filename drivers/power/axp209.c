/*
 * (C) Copyright 2012
 * Henrik Nordstrom <henrik@henriknordstrom.net>
 * 
 * Copyright (C) 2016 Next Thing Co.
 * Jose Angel Torres <software@nextthing.co>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/pmic_bus.h>
#include <axp_pmic.h>

static u8 axp209_mvolt_to_cfg(int mvolt, int min, int max, int div)
{
	if (mvolt < min)
		mvolt = min;
	else if (mvolt > max)
		mvolt = max;

	return (mvolt - min) / div;
}

int axp_set_dcdc2(unsigned int mvolt)
{
	int rc;
	u8 cfg, current;

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP209_OUTPUT_CTRL,
					AXP209_OUTPUT_CTRL_DCDC2);

	rc = pmic_bus_setbits(AXP209_OUTPUT_CTRL, AXP209_OUTPUT_CTRL_DCDC2);
	if (rc)
		return rc;

	cfg = axp209_mvolt_to_cfg(mvolt, 700, 2275, 25);

	/* Do we really need to be this gentle? It has built-in voltage slope */
	while ((rc = pmic_bus_read(AXP209_DCDC2_VOLTAGE, &current)) == 0 &&
	       current != cfg) {
		if (current < cfg)
			current++;
		else
			current--;

		rc = pmic_bus_write(AXP209_DCDC2_VOLTAGE, current);
		if (rc)
			break;
	}

	return rc;
}

int axp_set_dcdc3(unsigned int mvolt)
{
	u8 cfg = axp209_mvolt_to_cfg(mvolt, 700, 3500, 25);
	int rc;

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP209_OUTPUT_CTRL,
					AXP209_OUTPUT_CTRL_DCDC3);

	rc = pmic_bus_write(AXP209_DCDC3_VOLTAGE, cfg);
	if (rc)
		return rc;

	return pmic_bus_setbits(AXP209_OUTPUT_CTRL, AXP209_OUTPUT_CTRL_DCDC3);
}

int axp_set_aldo2(unsigned int mvolt)
{
	int rc;
	u8 cfg, reg;

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP209_OUTPUT_CTRL,
					AXP209_OUTPUT_CTRL_LDO2);

	cfg = axp209_mvolt_to_cfg(mvolt, 1800, 3300, 100);

	rc = pmic_bus_read(AXP209_LDO24_VOLTAGE, &reg);
	if (rc)
		return rc;

	/* LDO2 configuration is in upper 4 bits */
	reg = (reg & 0x0f) | (cfg << 4);
	rc = pmic_bus_write(AXP209_LDO24_VOLTAGE, reg);
	if (rc)
		return rc;

	return pmic_bus_setbits(AXP209_OUTPUT_CTRL, AXP209_OUTPUT_CTRL_LDO2);
}

int axp_set_aldo3(unsigned int mvolt)
{
	u8 cfg;
	int rc;

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP209_OUTPUT_CTRL,
					AXP209_OUTPUT_CTRL_LDO3);

	if (mvolt == -1)
		cfg = 0x80;	/* determined by LDO3IN pin */
	else
		cfg = axp209_mvolt_to_cfg(mvolt, 700, 3500, 25);

	rc = pmic_bus_write(AXP209_LDO3_VOLTAGE, cfg);
	if (rc)
		return rc;

	return pmic_bus_setbits(AXP209_OUTPUT_CTRL, AXP209_OUTPUT_CTRL_LDO3);
}

int axp_set_aldo4(unsigned int mvolt)
{
	int rc;
	static const unsigned int vindex[] = {
		1250, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2500,
		2700, 2800, 3000, 3100, 3200, 3300
	};
	u8 cfg, reg;

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP209_OUTPUT_CTRL,
					AXP209_OUTPUT_CTRL_LDO4);

	/* Translate mvolt to register cfg value, requested <= selected */
	for (cfg = 15; vindex[cfg] > mvolt && cfg > 0; cfg--);

	rc = pmic_bus_read(AXP209_LDO24_VOLTAGE, &reg);
	if (rc)
		return rc;

	/* LDO4 configuration is in lower 4 bits */
	reg = (reg & 0xf0) | (cfg << 0);
	rc = pmic_bus_write(AXP209_LDO24_VOLTAGE, reg);
	if (rc)
		return rc;

	return pmic_bus_setbits(AXP209_OUTPUT_CTRL, AXP209_OUTPUT_CTRL_LDO4);
}

int axp_set_no_limit(void)
{
        int rc;
        u8 reg;

    	rc = pmic_bus_read(AXP209_VBUS_POWER_PATH, &reg);
	if (rc)
		return rc;

        reg = reg | AXP209_VBUS_NO_LIMIT;
        rc = pmic_bus_write(AXP209_VBUS_POWER_PATH, reg);
        if (rc)
		return rc;

	return 0;

}

int axp_get_fuel_gauge(int * fuel_gauge)
{
        int rc, percent = 0;
        bool battery_connected = false;
        u8 reg;

        rc = axp_is_battery_connected(&battery_connected);
        if (rc)
                return rc;

        if (!battery_connected)
        {
                *fuel_gauge = 0;
                return 0;
        }

        rc = pmic_bus_read(AXP209_FUEL_GAUGE, &reg);
        if (rc)
                return rc;

        if (reg & 0x80)
        {
                printf("Fuel Gauge: Suspended\n");
                return 1;
        }

        if ((reg >= 0x00) & (reg <= 0x7F))
        {
                percent = (((float)reg / 127) * 100);
        }

        *fuel_gauge = percent;

        return 0;
}

int axp_get_battery_voltage(u16 * voltage)
{
        int rc;
	u8 MSB_REGISTER, LSB_REGISTER, LBIT_MASK;
	u16 VALUE = 0;

	rc = pmic_bus_read(AXP209_BATTERY_VOLTAGE_HIGH, &MSB_REGISTER);
        if (rc)
                return rc;

	rc = pmic_bus_read(AXP209_BATTERY_VOLTAGE_LOW, &LSB_REGISTER);
        if (rc)
                return rc;

	LBIT_MASK=16;
	VALUE=(MSB_REGISTER * LBIT_MASK) | ((LSB_REGISTER & 240) / LBIT_MASK);

	*voltage = VALUE;

	return 0;
}

int axp_is_battery_connected(bool *isconnected)
{
        int rc;
        u16 val;
        rc = axp_get_battery_voltage(&val);
        if (rc) {
                printf("ERROR cannot read from AXP209!\n");
                return 1;
        }
        if (val >= 2500) {
                * isconnected = true;
        }
        else {
                * isconnected = false;
        }
        
        return 0;
}

int axp_is_powered(bool * ispowered)
{
        int rc;
        u8 val;
        rc=pmic_bus_read(AXP209_POWER_STATUS, &val);
        if (rc) {
                printf("ERROR cannot read from AXP209!\n");
                return 1;
        }
        if ( val & 0x01 ) {
                *ispowered = true;
        }
        else {
                * ispowered = false;
        }
        return 0;
}

int axp_shutdown(void)
{
        int rc;
        u8 val;
        rc = pmic_bus_read(AXP209_SHUTDOWN, &val);

        if(rc) {
                printf("ERROR cannot read from AXP209!\n");
        }

        val |= 128;
        rc = pmic_bus_write(AXP209_SHUTDOWN, val);
        if(rc) {
                printf("ERROR cannot write to AXP209!\n");
        }
        return rc;
}

int axp_init(void)
{
	u8 ver;
	int i, rc;

	rc = pmic_bus_init();
	if (rc)
		return rc;

	rc = pmic_bus_read(AXP209_CHIP_VERSION, &ver);
	if (rc)
		return rc;

	/* Low 4 bits is chip version */
	ver &= 0x0f;

	if (ver != 0x1)
		return -1;

	/* Mask all interrupts */
	for (i = AXP209_IRQ_ENABLE1; i <= AXP209_IRQ_ENABLE5; i++) {
		rc = pmic_bus_write(i, 0);
		if (rc)
			return rc;
	}

	return 0;
}
