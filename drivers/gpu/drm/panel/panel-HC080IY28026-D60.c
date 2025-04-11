// SPDX-License-Identifier: GPL-2.0
/*
 * debix HC080IY28026-D60 MIPI-DSI panel driver
 *
 * Copyright 2019 NXP
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
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
	u8 cmd;
	u8 param;
};

/*
 * There is no description in the Reference Manual about these commands.
 * We received them from vendor, so just use them as is.
 */
static const struct cmd_set_entry init_reg[] = {
	{0xE0, 0x00},

	{0xE1, 0x93},
	{0xE2, 0x65},
	{0xE3, 0xF8},
	{0x80, 0x03},

	{0xE0, 0x01},

	{0x00, 0x00},
	{0x01, 0x3C},
	{0x03, 0x00},
	{0x04, 0x3C},

	{0x17, 0x00},
	{0x18, 0xF7},
	{0x19, 0x01},
	{0x1A, 0x00},
	{0x1B, 0xF7},
	{0x1C, 0x01},

	{0x24, 0xF1},

	{0x35, 0x23},

	{0x37, 0x09},

	{0x38, 0x04},
	{0x39, 0x00},
	{0x3A, 0x01},
	{0x3C, 0x70},
	{0x3D, 0xFF},
	{0x3E, 0xFF},
	{0x3F, 0x7F},

	{0x40, 0x06},
	{0x41, 0xA0},
	{0x43, 0x1E},
	{0x44, 0x0B},
	{0x45, 0x28},
	{0x0C, 0x74},

	{0x55, 0x02}, //John_gao 01 -> 02 reverse color & dark
	{0x57, 0x69},
	{0x59, 0x0A},
	{0x5A, 0x2D},
	{0x5B, 0x1A},
	{0x5C, 0x15},

	{0x5D, 0x7F},
	{0x5E, 0x69},
	{0x5F, 0x59},
	{0x60, 0x4C},
	{0x61, 0x47},
	{0x62, 0x38},
	{0x63, 0x3D},
	{0x64, 0x27},
	{0x65, 0x41},
	{0x66, 0x40},
	{0x67, 0x40},
	{0x68, 0x5B},
	{0x69, 0x46},
	{0x6A, 0x49},
	{0x6B, 0x3A},
	{0x6C, 0x34},
	{0x6D, 0x25},
	{0x6E, 0x15},
	{0x6F, 0x02},
	{0x70, 0x7F},
	{0x71, 0x69},
	{0x72, 0x59},
	{0x73, 0x4C},
	{0x74, 0x47},
	{0x75, 0x38},
	{0x76, 0x3D},
	{0x77, 0x27},
	{0x78, 0x41},
	{0x79, 0x40},
	{0x7A, 0x40},
	{0x7B, 0x5B},
	{0x7C, 0x46},
	{0x7D, 0x49},
	{0x7E, 0x3A},
	{0x7F, 0x34},
	{0x80, 0x25},
	{0x81, 0x15},
	{0x82, 0x02},

	{0xE0, 0x02},

	{0x00, 0x50},
	{0x01, 0x55},
	{0x02, 0x55},
	{0x03, 0x52},
	{0x04, 0x77},
	{0x05, 0x57},
	{0x06, 0x55},
	{0x07, 0x4E},
	{0x08, 0x4C},
	{0x09, 0x55},
	{0x0A, 0x4A},
	{0x0B, 0x48},
	{0x0C, 0x55},
	{0x0D, 0x46},
	{0x0E, 0x44},
	{0x0F, 0x40},
	{0x10, 0x55},
	{0x11, 0x55},
	{0x12, 0x55},
	{0x13, 0x55},
	{0x14, 0x55},
	{0x15, 0x55},

	{0x16, 0x51},
	{0x17, 0x55},
	{0x18, 0x55},
	{0x19, 0x53},
	{0x1A, 0x77},
	{0x1B, 0x57},
	{0x1C, 0x55},
	{0x1D, 0x4F},
	{0x1E, 0x4D},
	{0x1F, 0x55},
	{0x20, 0x4B},
	{0x21, 0x49},
	{0x22, 0x55},
	{0x23, 0x47},
	{0x24, 0x45},
	{0x25, 0x41},
	{0x26, 0x55},
	{0x27, 0x55},
	{0x28, 0x55},
	{0x29, 0x55},
	{0x2A, 0x55},
	{0x2B, 0x55},

	{0x2C, 0x01},
	{0x2D, 0x15},
	{0x2E, 0x15},
	{0x2F, 0x13},
	{0x30, 0x17},
	{0x31, 0x17},
	{0x32, 0x15},
	{0x33, 0x0D},
	{0x34, 0x0F},
	{0x35, 0x15},
	{0x36, 0x05},
	{0x37, 0x07},
	{0x38, 0x15},
	{0x39, 0x09},
	{0x3A, 0x0B},
	{0x3B, 0x11},
	{0x3C, 0x15},
	{0x3D, 0x15},
	{0x3E, 0x15},
	{0x3F, 0x15},
	{0x40, 0x15},
	{0x41, 0x15},

	{0x42, 0x00},
	{0x43, 0x15},
	{0x44, 0x15},
	{0x45, 0x12},
	{0x46, 0x17},
	{0x47, 0x17},
	{0x48, 0x15},
	{0x49, 0x0C},
	{0x4A, 0x0E},
	{0x4B, 0x15},
	{0x4C, 0x04},
	{0x4D, 0x06},
	{0x4E, 0x15},
	{0x4F, 0x08},
	{0x50, 0x0A},
	{0x51, 0x10},
	{0x52, 0x15},
	{0x53, 0x15},
	{0x54, 0x15},
	{0x55, 0x15},
	{0x56, 0x15},
	{0x57, 0x15},

	{0x58, 0x40},
	{0x5B, 0x10},
	{0x5C, 0x06},
	{0x5D, 0x40},
	{0x5E, 0x00},
	{0x5F, 0x00},
	{0x60, 0x40},
	{0x61, 0x03},
	{0x62, 0x04},
	{0x63, 0x6C},
	{0x64, 0x6C},
	{0x65, 0x75},
	{0x66, 0x08},
	{0x67, 0xB4},
	{0x68, 0x08},
	{0x69, 0x6C},
	{0x6A, 0x6C},
	{0x6B, 0x0C},
	{0x6D, 0x00},
	{0x6E, 0x00},
	{0x6F, 0x88},
	{0x75, 0xBB},
	{0x76, 0x00},
	{0x77, 0x05},
	{0x78, 0x2A},

	{0xE0, 0x04},
	{0x09, 0x11},
	{0x0E, 0x48},
	{0x2B, 0x08},
	{0X2E, 0x03},


	{0xE0, 0x00},
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
	struct backlight_device *backlight;
	struct backlight_device *backlight2;

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
	.clock = 50000,
	.hdisplay = 800,
	.hsync_start = 800 + 30,
	.hsync_end = 800 + 30 + 8,
	.htotal = 800 + 30 + 8 + 4,
	.vdisplay = 1280,
	.vsync_start = 1280 + 20,
	.vsync_end = 1280 + 20 + 20,
	.vtotal = 1280 + 20 + 20 + 20,

	.width_mm = 107,
	.height_mm = 172,

	.flags = DRM_MODE_FLAG_NHSYNC |
		 DRM_MODE_FLAG_NVSYNC,
};

static inline struct rad_panel *to_rad_panel(struct drm_panel *panel)
{
	return container_of(panel, struct rad_panel, panel);
}

static int rad_panel_push_cmd_list(struct mipi_dsi_device *dsi,
				   struct cmd_set_entry const *cmd_set,
				   size_t count)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < count; i++) {
		const struct cmd_set_entry *entry = cmd_set++;
		u8 buffer[2] = { entry->cmd, entry->param };

		ret = mipi_dsi_generic_write(dsi, &buffer, sizeof(buffer));
		if (ret < 0)
			return ret;
	}

	return ret;
};

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

	if (panel->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	gpiod_set_value_cansleep(panel->reset, 1);
	msleep(5);
	gpiod_set_value_cansleep(panel->reset, 0);
	msleep(10);
	gpiod_set_value_cansleep(panel->reset, 1);
	msleep(120);

	ret = rad_panel_push_cmd_list(dsi,
				      &init_reg[0],
				      ARRAY_SIZE(init_reg));
	if (ret < 0) {
		dev_err(dev, "Failed to send MCS (%d)\n", ret);
		goto fail;
	}

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

	backlight_enable(panel->backlight);
	backlight_enable(panel->backlight2);

	panel->enabled = true;

	return 0;

fail:

	return ret;
}

static int rad_panel_enable(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);

	return rad->pdata->enable(rad);
}

static int rad_panel_disable(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);
	struct mipi_dsi_device *dsi = rad->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	if (!rad->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	backlight_disable(rad->backlight);
	backlight_disable(rad->backlight2);

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

	rad->enabled = false;

	return 0;
}

static int rad_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct drm_display_mode *mode;

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

static int rad_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);
	u16 brightness;
	int ret;

	if (!rad->prepared)
		return 0;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	bl->props.brightness = brightness;

	return brightness & 0xff;
}
static int delayBright = 1;
static int rad_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);
	int ret = 0;


	if (!rad->prepared)
		return 0;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);
	if (ret < 0)
		return ret;

	if (rad->backlight2) {
		if(delayBright==1){
			delayBright = 0;
			msleep(1000);
		}
                rad->backlight2->props.power = FB_BLANK_UNBLANK;
		
		rad->backlight2->props.brightness = bl->props.brightness;
                backlight_update_status(rad->backlight2);
        }


	return 0;
}

static const struct backlight_ops rad_bl_ops = {
	.update_status = rad_bl_update_status,
	.get_brightness = rad_bl_get_brightness,
};

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
	{ .compatible = "debix,HC080IY28026-D60", .data = &rad_mipi },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rad_of_match);

static int rad_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct of_device_id *of_id = of_match_device(rad_of_match, dev);
	struct device_node *np = dev->of_node;
	struct rad_panel *panel;
	struct backlight_properties bl_props;
	int ret;
	struct device_node *backlight2;
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
	dsi->mode_flags =  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO;
	//dsi->mode_flags =  MIPI_DSI_MODE_VIDEO;
	backlight2 = of_parse_phandle(dev->of_node, "backlight", 0);
        if (backlight2) {
                panel->backlight2 = of_find_backlight_by_node(backlight2);
                of_node_put(backlight2);
        }

	ret = of_property_read_u32(np, "video-mode", &video_mode);
	if (!ret) {
		switch (video_mode) {
		case 0:
			/* burst mode */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
			break;
		case 1:
			/* non-burst mode with sync event */
			break;
		case 2:
			/* non-burst mode with sync pulse */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
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
	gpiod_set_value_cansleep(panel->reset, 1);

	memset(&bl_props, 0, sizeof(bl_props));
	bl_props.type = BACKLIGHT_RAW;
	bl_props.brightness = 255;
	bl_props.max_brightness = 255;

	panel->backlight = devm_backlight_device_register(dev, dev_name(dev),
							  dev, dsi, &rad_bl_ops,
							  &bl_props);
	if (IS_ERR(panel->backlight)) {
		ret = PTR_ERR(panel->backlight);
		dev_err(dev, "Failed to register backlight (%d)\n", ret);
		return ret;
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

	return ;
}

static void rad_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);

	rad_panel_disable(&rad->panel);
	rad_panel_unprepare(&rad->panel);
}

static struct mipi_dsi_driver rad_panel_driver = {
	.driver = {
		.name = "panel-HC080IY28026-D60",
		.of_match_table = rad_of_match,
	},
	.probe = rad_panel_probe,
	.remove = rad_panel_remove,
	.shutdown = rad_panel_shutdown,
};
module_mipi_dsi_driver(rad_panel_driver);

MODULE_AUTHOR("John gao  <john@polyhex.com>");
MODULE_DESCRIPTION("DRM Driver for debix HC080IY28026-D60  MIPI DSI panel");
MODULE_LICENSE("GPL v2");
