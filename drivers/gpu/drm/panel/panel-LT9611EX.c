// SPDX-License-Identifier: GPL-2.0
/*
 * debix LT9611EX MIPI-DSI panel driver
 *
 * Copyright 2019 NXP
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

/* Panel specific color-format bits */
#define COL_FMT_16BPP 0x55
#define COL_FMT_18BPP 0x66
#define COL_FMT_24BPP 0x77

/* Write Manufacture Command Set Control */
#define WRMAUCCTR 0xFE

/* Manufacturer Command Set pages (CMD2) */
struct cmd_set_entry {
	u8 len; //param len
	u8 cmd;
	u8 param1;
	u8 param2;
	u8 param3;
};

#define MEDIA_BUS_FMT_RGB888_1X24		0x100a
#define MEDIA_BUS_FMT_RGB666_1X18		0x1009
#define MEDIA_BUS_FMT_RGB565_1X16		0x1017

static const u32 rad_bus_formats[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_RGB666_1X18,
	MEDIA_BUS_FMT_RGB565_1X16,
};

static const u32 rad_bus_flags = DRM_BUS_FLAG_DE_LOW |
				 DRM_BUS_FLAG_PIXDATA_SAMPLE_POSEDGE;

struct rad_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *reset;
	struct gpio_desc *enable;
	struct gpio_desc *irq; 
	struct regulator_bulk_data *supplies;
	unsigned int num_supplies;

	bool prepared;
	bool enabled;

	const struct rad_platform_data *pdata;
};

struct rad_platform_data {
	int (*enable)(struct rad_panel *panel);
};

static const struct drm_display_mode default_mode = {
#if 1
	//.clock = 74250,
	.clock = 75000,
	.hdisplay = 1280,
	.hsync_start = 1280 + 110,
	.hsync_end = 1280 + 110 + 40,
	.htotal = 1280 + 110 + 40 + 220,
	.vdisplay = 720,
	.vsync_start = 720 + 5,
	.vsync_end = 720 + 5 + 5,
	.vtotal = 720 + 5 + 5 + 20,
#else
	.clock = 50000,
	.hdisplay = 800,
	.hsync_start = 800 + 30,
	.hsync_end = 800 + 30 + 8,
	.htotal = 800 + 30 + 8 + 4,
	.vdisplay = 1280,
	.vsync_start = 1280 + 20,
	.vsync_end = 1280 + 20 + 20,
	.vtotal = 1280 + 20 + 20 + 20,
#endif
	.width_mm = 107,
	.height_mm = 172,

	.flags = DRM_MODE_FLAG_NHSYNC |
		 DRM_MODE_FLAG_NVSYNC,
};

static inline struct rad_panel *to_rad_panel(struct drm_panel *panel)
{
	return container_of(panel, struct rad_panel, panel);
}

static int color_format_from_dsi_format(enum mipi_dsi_pixel_format format)
{
	switch (format) {
	case MIPI_DSI_FMT_RGB565:
		return COL_FMT_16BPP;
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB666_PACKED:
		return COL_FMT_18BPP;
	case MIPI_DSI_FMT_RGB888:
		return COL_FMT_24BPP;
	default:
		return COL_FMT_24BPP; /* for backward compatibility */
	}
};

static int rad_panel_prepare(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);
	int ret;

printk("GLS_MIPI %s\n",__func__);
	if (rad->prepared)
		return 0;

	ret = regulator_bulk_enable(rad->num_supplies, rad->supplies);
	if (ret)
		return ret;

	/* At lest 10ms needed between power-on and reset-out as RM specifies */
	usleep_range(10000, 12000);

	rad->prepared = true;

	return 0;
}

static int rad_panel_unprepare(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);
	int ret;

printk("GLS_MIPI %s\n",__func__);
	if (!rad->prepared)
		return 0;

	ret = regulator_bulk_disable(rad->num_supplies, rad->supplies);
	if (ret)
		return ret;

	rad->prepared = false;

	return 0;
}

static int mipi_enable(struct rad_panel *panel)
{
	struct mipi_dsi_device *dsi = panel->dsi;
	struct device *dev = &dsi->dev;
	int color_format = color_format_from_dsi_format(dsi->format);
	int ret;

printk("GLS_MIPI %s\n",__func__);
	if (panel->enabled)
		return 0;
#if 0
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	/* Set pixel format */
	ret = mipi_dsi_dcs_set_pixel_format(dsi, color_format);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format (%d)\n", ret);
		goto fail;
	}
	/* Exit sleep mode */
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode (%d)\n", ret);
		goto fail;
	}

	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display ON (%d)\n", ret);
		goto fail;
	}
	msleep(100);
	ret = mipi_dsi_dcs_set_tear_on(dsi, 1);
	if (ret < 0) {
		dev_err(dev, "Failed to set display ON (%d)\n", ret);
		goto fail;
	}
 
#endif
	panel->enabled = true;

	return 0;

fail:
	//gpiod_set_value_cansleep(panel->reset, 1);

	return ret;
}

static int rad_panel_enable(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);

printk("GLS_MIPI %s\n",__func__);
	return rad->pdata->enable(rad);
}

static int rad_panel_disable(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);
	struct mipi_dsi_device *dsi = rad->dsi;
	struct device *dev = &dsi->dev;
	int ret;

printk("GLS_MIPI %s\n",__func__);
	if (!rad->enabled)
		return 0;

#if 0
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
 

	usleep_range(10000, 12000);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display OFF (%d)\n", ret);
		return ret;
	}

	usleep_range(5000, 10000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode (%d)\n", ret);
		return ret;
	}
#endif
	rad->enabled = false;

	return 0;
}

static int rad_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct drm_display_mode *mode;
printk("GLS_MIPI %s\n",__func__);
	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	connector->display_info.bus_flags = rad_bus_flags;

	drm_display_info_set_bus_formats(&connector->display_info,
					 rad_bus_formats,
					 ARRAY_SIZE(rad_bus_formats));
	return 1;
}

 


static const struct drm_panel_funcs rad_panel_funcs = {
	.prepare = rad_panel_prepare,
	.unprepare = rad_panel_unprepare,
	.enable = rad_panel_enable,
	.disable = rad_panel_disable,
	.get_modes = rad_panel_get_modes,
};

static const char * const rad_supply_names[] = {
	"v3p3",
	"v1p8",
};

static int rad_init_regulators(struct rad_panel *rad)
{
	struct device *dev = &rad->dsi->dev;
	int i;

printk("GLS_MIPI %s\n",__func__);
	rad->num_supplies = ARRAY_SIZE(rad_supply_names);
	rad->supplies = devm_kcalloc(dev, rad->num_supplies,
				     sizeof(*rad->supplies), GFP_KERNEL);
	if (!rad->supplies)
		return -ENOMEM;

	for (i = 0; i < rad->num_supplies; i++)
		rad->supplies[i].supply = rad_supply_names[i];

	return devm_regulator_bulk_get(dev, rad->num_supplies, rad->supplies);
};

static const struct rad_platform_data rad_mipi = {
	.enable = &mipi_enable,
};

static const struct of_device_id rad_of_match[] = {
	{ .compatible = "debix,LT9611EX", .data = &rad_mipi },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rad_of_match);

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
    printk(KERN_INFO "GPIO interrupt triggered\n");
    return IRQ_HANDLED;
}

static int rad_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct of_device_id *of_id = of_match_device(rad_of_match, dev);
	struct device_node *np = dev->of_node;
	struct rad_panel *panel; 
	int ret;
	u32 video_mode;

	if (!of_id || !of_id->data)
		return -ENODEV;

	panel = devm_kzalloc(&dsi->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, panel);

	panel->dsi = dsi;
	panel->pdata = of_id->data;

	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_NO_EOT_PACKET;

	ret = of_property_read_u32(np, "video-mode", &video_mode);
	if (!ret) {
		switch (video_mode) {
		case 0:
			/* burst mode */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST |
					   MIPI_DSI_MODE_VIDEO;
			break;
		case 1:
			/* non-burst mode with sync event */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;
			break;
		case 2:
			/* non-burst mode with sync pulse */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
					   MIPI_DSI_MODE_VIDEO;
			break;
		case 3:
			/* command mode */
			dsi->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS |
					   MIPI_DSI_MODE_VSYNC_FLUSH;
			break;
		default:
			dev_warn(dev, "invalid video mode %d\n", video_mode);
			break;
		}
	}

	ret = of_property_read_u32(np, "dsi-lanes", &dsi->lanes);
	if (ret) {
		dev_err(dev, "Failed to get dsi-lanes property (%d)\n", ret);
		return ret;
	}


	panel->reset = devm_gpiod_get_optional(dev, "reset",
					       GPIOD_OUT_LOW |
					       GPIOD_FLAGS_BIT_NONEXCLUSIVE);
	if (IS_ERR(panel->reset)) {
		ret = PTR_ERR(panel->reset);
		dev_err(dev, "Failed to get reset gpio (%d)\n", ret);
		return ret;
	}

	panel->enable = devm_gpiod_get_optional(dev, "enable",
					       GPIOD_OUT_LOW |
					       GPIOD_FLAGS_BIT_NONEXCLUSIVE);
	if (IS_ERR(panel->enable)) {
		ret = PTR_ERR(panel->enable);
		dev_err(dev, "Failed to get enable gpio (%d)\n", ret);
		return ret;
	}
	gpiod_set_value_cansleep(panel->enable, 1);

	gpiod_set_value_cansleep(panel->reset, 0);
//		usleep_range(15000, 17000);
	msleep(300);
	gpiod_set_value_cansleep(panel->reset, 1);


	
	panel->irq = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
	if (IS_ERR(panel->irq)) {
		ret = PTR_ERR(panel->irq);
		dev_err(dev, "Failed to get irq gpio (%d)\n", ret);
		return ret;
	}else{
		int irq_num = gpiod_to_irq(panel->irq);	
		if(irq_num < 0){
			dev_err(dev,"Failed to map gpio to irq \n");	
			return irq_num;
		}
		ret = devm_request_irq(dev, irq_num, gpio_irq_handler, IRQF_TRIGGER_RISING,"gpio_irq", dev);
		if(ret){
			dev_err(dev, "Failed to request IRQ\n");	
			return ret;
		}
	}


	ret = rad_init_regulators(panel);
	if (ret)
		return ret;

	drm_panel_init(&panel->panel, dev, &rad_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	dev_set_drvdata(dev, panel);

	drm_panel_add(&panel->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&panel->panel);

printk("GLS LT9611EX probe ok \n");
	return ret;
}

static void rad_panel_remove(struct mipi_dsi_device *dsi)
{
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret)
		dev_err(dev, "Failed to detach from host (%d)\n", ret);

	drm_panel_remove(&rad->panel);
}

static void rad_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);

	rad_panel_disable(&rad->panel);
	rad_panel_unprepare(&rad->panel);
}

static struct mipi_dsi_driver rad_panel_driver = {
	.driver = {
		.name = "panel-LT9611EX",
		.of_match_table = rad_of_match,
	},
	.probe = rad_panel_probe,
	.remove = rad_panel_remove,
	.shutdown = rad_panel_shutdown,
};
module_mipi_dsi_driver(rad_panel_driver);

MODULE_AUTHOR("John gao  <john@polyhex.com>");
MODULE_DESCRIPTION("DRM Driver for debix LT9611EX  MIPI DSI panel");
MODULE_LICENSE("GPL v2");
