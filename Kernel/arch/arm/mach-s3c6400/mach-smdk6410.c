/* linux/arch/arm/mach-s3c6410/mach-smdk6410.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/smsc911x.h>

#ifdef CONFIG_SMDK6410_WM1190_EV1
#include <linux/mfd/wm8350/core.h>
#include <linux/mfd/wm8350/pmic.h>
#endif

#include <video/platform_lcd.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/regs-fb.h>
#include <mach/map.h>

#include <asm/irq.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/regs-modem.h>
#include <plat/regs-gpio.h>
#include <plat/regs-sys.h>
#include <plat/iic.h>
#include <plat/fb.h>
#include <plat/gpio-cfg.h>

#include <plat/s3c6410.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/cpu.h>

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg smdk6410_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	[3] = {
		.hwport	     = 3,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
};

/* framebuffer and LCD setup. */

/* GPF15 = LCD backlight control
 * GPF13 => Panel power
 * GPN5 = LCD nRESET signal
 * PWM_TOUT1 => backlight brightness
 */

static void smdk6410_lcd_power_set(struct plat_lcd_data *pd,
				   unsigned int power)
{
	if (power) {
		gpio_direction_output(S3C64XX_GPF(13), 1);
		gpio_direction_output(S3C64XX_GPF(15), 1);

		/* fire nRESET on power up */
		gpio_direction_output(S3C64XX_GPN(5), 0);
		msleep(10);
		gpio_direction_output(S3C64XX_GPN(5), 1);
		msleep(1);
	} else {
		gpio_direction_output(S3C64XX_GPF(15), 0);
		gpio_direction_output(S3C64XX_GPF(13), 0);
	}
}

static struct plat_lcd_data smdk6410_lcd_power_data = {
	.set_power	= smdk6410_lcd_power_set,
};

static struct platform_device smdk6410_lcd_powerdev = {
	.name			= "platform-lcd",
	.dev.parent		= &s3c_device_fb.dev,
	.dev.platform_data	= &smdk6410_lcd_power_data,
};

static struct s3c_fb_pd_win smdk6410_fb_win0 = {
	/* this is to ensure we use win0 */
	.win_mode	= {
		.pixclock	= 41094,
		.left_margin	= 8,
		.right_margin	= 13,
		.upper_margin	= 7,
		.lower_margin	= 5,
		.hsync_len	= 3,
		.vsync_len	= 1,
		.xres		= 800,
		.yres		= 480,
	},
	.max_bpp	= 32,
	.default_bpp	= 16,
};

/* 405566 clocks per frame => 60Hz refresh requires 24333960Hz clock */
static struct s3c_fb_platdata smdk6410_lcd_pdata __initdata = {
	.setup_gpio	= s3c64xx_fb_gpio_setup_24bpp,
	.win[0]		= &smdk6410_fb_win0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_HSYNC | VIDCON1_INV_VSYNC,
};

static struct resource smdk6410_smsc911x_resources[] = {
	[0] = {
		.start = 0x18000000,
		.end   = 0x18000000 + SZ_64K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = S3C_EINT(10),
		.end   = S3C_EINT(10),
		.flags = IORESOURCE_IRQ | IRQ_TYPE_LEVEL_LOW,
	},
};

static struct smsc911x_platform_config smdk6410_smsc911x_pdata = {
	.irq_polarity  = SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type      = SMSC911X_IRQ_TYPE_OPEN_DRAIN,
	.flags         = SMSC911X_USE_32BIT | SMSC911X_FORCE_INTERNAL_PHY,
	.phy_interface = PHY_INTERFACE_MODE_MII,
};


static struct platform_device smdk6410_smsc911x = {
	.name          = "smsc911x",
	.id            = -1,
	.num_resources = ARRAY_SIZE(smdk6410_smsc911x_resources),
	.resource      = &smdk6410_smsc911x_resources[0],
	.dev = {
		.platform_data = &smdk6410_smsc911x_pdata,
	},
};

static struct map_desc smdk6410_iodesc[] = {};

static struct platform_device *smdk6410_devices[] __initdata = {
#ifdef CONFIG_SMDK6410_SD_CH0
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_SMDK6410_SD_CH1
	&s3c_device_hsmmc1,
#endif
	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_fb,
	&s3c_device_usb,
	&s3c_device_usb_hsotg,
	&smdk6410_lcd_powerdev,

	&smdk6410_smsc911x,
};

#ifdef CONFIG_SMDK6410_WM1190_EV1
/* S3C64xx internal logic & PLL */
static struct regulator_init_data wm8350_dcdc1_data = {
	.constraints = {
		.name = "PVDD_INT/PVDD_PLL",
		.min_uV = 1200000,
		.max_uV = 1200000,
		.always_on = 1,
		.apply_uV = 1,
	},
};

/* Memory */
static struct regulator_init_data wm8350_dcdc3_data = {
	.constraints = {
		.name = "PVDD_MEM",
		.min_uV = 1700000,
		.max_uV = 1700000,
		.always_on = 1,
		.state_mem = {
			 .uV = 1700000,
			 .mode = REGULATOR_MODE_NORMAL,
			 .enabled = 1,
		 },
		.initial_state = PM_SUSPEND_MEM,
	},
};

/* USB, EXT, PCM, ADC/DAC, USB, MMC */
static struct regulator_init_data wm8350_dcdc4_data = {
	.constraints = {
		.name = "PVDD_HI/PVDD_EXT/PVDD_SYS/PVCCM2MTV",
		.min_uV = 3000000,
		.max_uV = 3000000,
		.always_on = 1,
	},
};

/* ARM core */
static struct regulator_consumer_supply dcdc6_consumers[] = {
	{
		.supply = "vddarm",
	}
};

static struct regulator_init_data wm8350_dcdc6_data = {
	.constraints = {
		.name = "PVDD_ARM",
		.min_uV = 1000000,
		.max_uV = 1300000,
		.always_on = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies = ARRAY_SIZE(dcdc6_consumers),
	.consumer_supplies = dcdc6_consumers,
};

/* Alive */
static struct regulator_init_data wm8350_ldo1_data = {
	.constraints = {
		.name = "PVDD_ALIVE",
		.min_uV = 1200000,
		.max_uV = 1200000,
		.always_on = 1,
		.apply_uV = 1,
	},
};

/* OTG */
static struct regulator_init_data wm8350_ldo2_data = {
	.constraints = {
		.name = "PVDD_OTG",
		.min_uV = 3300000,
		.max_uV = 3300000,
		.always_on = 1,
	},
};

/* LCD */
static struct regulator_init_data wm8350_ldo3_data = {
	.constraints = {
		.name = "PVDD_LCD",
		.min_uV = 3000000,
		.max_uV = 3000000,
		.always_on = 1,
	},
};

/* OTGi/1190-EV1 HPVDD & AVDD */
static struct regulator_init_data wm8350_ldo4_data = {
	.constraints = {
		.name = "PVDD_OTGI/HPVDD/AVDD",
		.min_uV = 1200000,
		.max_uV = 1200000,
		.apply_uV = 1,
		.always_on = 1,
	},
};

static struct {
	int regulator;
	struct regulator_init_data *initdata;
} wm1190_regulators[] = {
	{ WM8350_DCDC_1, &wm8350_dcdc1_data },
	{ WM8350_DCDC_3, &wm8350_dcdc3_data },
	{ WM8350_DCDC_4, &wm8350_dcdc4_data },
	{ WM8350_DCDC_6, &wm8350_dcdc6_data },
	{ WM8350_LDO_1, &wm8350_ldo1_data },
	{ WM8350_LDO_2, &wm8350_ldo2_data },
	{ WM8350_LDO_3, &wm8350_ldo3_data },
	{ WM8350_LDO_4, &wm8350_ldo4_data },
};

static int __init smdk6410_wm8350_init(struct wm8350 *wm8350)
{
	int i;

	/* Configure the IRQ line */
	s3c_gpio_setpull(S3C64XX_GPN(12), S3C_GPIO_PULL_UP);

	/* Instantiate the regulators */
	for (i = 0; i < ARRAY_SIZE(wm1190_regulators); i++)
		wm8350_register_regulator(wm8350,
					  wm1190_regulators[i].regulator,
					  wm1190_regulators[i].initdata);

	return 0;
}

static struct wm8350_platform_data __initdata smdk6410_wm8350_pdata = {
	.init = smdk6410_wm8350_init,
	.irq_high = 1,
};
#endif

static struct i2c_board_info i2c_devs0[] __initdata = {
	{ I2C_BOARD_INFO("24c08", 0x50), },
	{ I2C_BOARD_INFO("wm8580", 0x1b), },

#ifdef CONFIG_SMDK6410_WM1190_EV1
	{ I2C_BOARD_INFO("wm8350", 0x1a),
	  .platform_data = &smdk6410_wm8350_pdata,
	  .irq = S3C_EINT(12),
	},
#endif
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{ I2C_BOARD_INFO("24c128", 0x57), },	/* Samsung S524AD0XD1 */
};

static void __init smdk6410_map_io(void)
{
	u32 tmp;

	s3c64xx_init_io(smdk6410_iodesc, ARRAY_SIZE(smdk6410_iodesc));
	s3c24xx_init_clocks(12000000);
	s3c24xx_init_uarts(smdk6410_uartcfgs, ARRAY_SIZE(smdk6410_uartcfgs));

	/* set the LCD type */

	tmp = __raw_readl(S3C64XX_SPCON);
	tmp &= ~S3C64XX_SPCON_LCD_SEL_MASK;
	tmp |= S3C64XX_SPCON_LCD_SEL_RGB;
	__raw_writel(tmp, S3C64XX_SPCON);

	/* remove the lcd bypass */
	tmp = __raw_readl(S3C64XX_MODEM_MIFPCON);
	tmp &= ~MIFPCON_LCD_BYPASS;
	__raw_writel(tmp, S3C64XX_MODEM_MIFPCON);
}

static void __init smdk6410_machine_init(void)
{
	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	s3c_fb_set_platdata(&smdk6410_lcd_pdata);

	gpio_request(S3C64XX_GPN(5), "LCD power");
	gpio_request(S3C64XX_GPF(13), "LCD power");
	gpio_request(S3C64XX_GPF(15), "LCD power");

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	platform_add_devices(smdk6410_devices, ARRAY_SIZE(smdk6410_devices));
}

MACHINE_START(SMDK6410, "SMDK6410")
	/* Maintainer: Ben Dooks <ben@fluff.org> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C64XX_PA_SDRAM + 0x100,

	.init_irq	= s3c6410_init_irq,
	.map_io		= smdk6410_map_io,
	.init_machine	= smdk6410_machine_init,
	.timer		= &s3c24xx_timer,
MACHINE_END
