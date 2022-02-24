// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>
#include <linux/soc/mediatek/mtk-mmsys.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk-mmsys.h"
#include "mt8167-mmsys.h"
#include "mt8183-mmsys.h"
#include "mt8192-mmsys.h"
#include "mt8195-mmsys.h"
#include "mt8365-mmsys.h"

#define MMSYS_SW_RESET_PER_REG 32

static const struct mtk_mmsys_driver_data mt2701_mmsys_driver_data = {
	.clk_driver = "clk-mt2701-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
};

static const struct mtk_mmsys_driver_data mt2712_mmsys_driver_data = {
	.clk_driver = "clk-mt2712-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
};

static const struct mtk_mmsys_driver_data mt6779_mmsys_driver_data = {
	.clk_driver = "clk-mt6779-mm",
};

static const struct mtk_mmsys_driver_data mt6797_mmsys_driver_data = {
	.clk_driver = "clk-mt6797-mm",
};

static const struct mtk_mmsys_driver_data mt8167_mmsys_driver_data = {
	.clk_driver = "clk-mt8167-mm",
	.routes = mt8167_mmsys_routing_table,
	.num_routes = ARRAY_SIZE(mt8167_mmsys_routing_table),
};

static const struct mtk_mmsys_driver_data mt8173_mmsys_driver_data = {
	.clk_driver = "clk-mt8173-mm",
	.routes = mmsys_default_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_default_routing_table),
	.sw_reset_start = MMSYS_SW0_RST_B,
	.num_resets = 32,
};

static const struct mtk_mmsys_driver_data mt8183_mmsys_driver_data = {
	.clk_driver = "clk-mt8183-mm",
	.routes = mmsys_mt8183_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8183_routing_table),
	.mdp_routes = mmsys_mt8183_mdp_routing_table,
	.mdp_num_routes = ARRAY_SIZE(mmsys_mt8183_mdp_routing_table),
	.mdp_isp_ctrl = mmsys_mt8183_mdp_isp_ctrl_table,
	.sw_reset_start = MMSYS_SW0_RST_B,
	.num_resets = 32,
};

static const struct mtk_mmsys_driver_data mt8192_mmsys_driver_data = {
	.clk_driver = "clk-mt8192-mm",
	.routes = mmsys_mt8192_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8192_routing_table),
};

static const struct mtk_mmsys_driver_data mt8195_vdosys0_driver_data = {
	.clk_driver = "clk-mt8195-vdo0",
	.routes = mmsys_mt8195_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8195_routing_table),
};

static const struct mtk_mmsys_driver_data mt8195_vdosys1_driver_data = {
	.clk_driver = "clk-mt8195-vdo1",
	.routes = mmsys_mt8195_routing_table,
	.num_routes = ARRAY_SIZE(mmsys_mt8195_routing_table),
	.config = mmsys_mt8195_config_table,
	.num_configs = ARRAY_SIZE(mmsys_mt8195_config_table),
	.sw_reset_start = MT8195_VDO1_SW0_RST_B,
	.num_resets = 64,
};

static const struct mtk_mmsys_driver_data mt8195_vppsys0_driver_data = {
	.clk_driver = "clk-mt8195-vpp0",
	.mdp_routes = mmsys_mt8195_mdp_routing_table,
	.mdp_num_routes = ARRAY_SIZE(mmsys_mt8195_mdp_routing_table),
	.mdp_mmsys_configs = mmsys_mt8195_mdp_vppsys_config_table,
	.mdp_num_mmsys_configs = ARRAY_SIZE(mmsys_mt8195_mdp_vppsys_config_table),
	.vppsys = true,
};

static const struct mtk_mmsys_driver_data mt8195_vppsys1_driver_data = {
	.clk_driver = "clk-mt8195-vpp1",
	.mdp_routes = mmsys_mt8195_mdp_routing_table,
	.mdp_num_routes = ARRAY_SIZE(mmsys_mt8195_mdp_routing_table),
	.mdp_mmsys_configs = mmsys_mt8195_mdp_vppsys_config_table,
	.mdp_num_mmsys_configs = ARRAY_SIZE(mmsys_mt8195_mdp_vppsys_config_table),
	.vppsys = true,
};

static const struct mtk_mmsys_driver_data mt8365_mmsys_driver_data = {
	.clk_driver = "clk-mt8365-mm",
	.routes = mt8365_mmsys_routing_table,
	.num_routes = ARRAY_SIZE(mt8365_mmsys_routing_table),
};

struct mtk_mmsys {
	struct regmap *regmap;
	const struct mtk_mmsys_driver_data *data;
	struct reset_controller_dev rcdev;
	struct cmdq_client_reg cmdq_base;
	phys_addr_t addr;
	u8 subsys_id;
};

void mtk_mmsys_ddp_connect(struct device *dev,
			   enum mtk_ddp_comp_id cur,
			   enum mtk_ddp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->routes;
	int i;

	for (i = 0; i < mmsys->data->num_routes; i++) {
		if (cur != routes[i].from_comp || next != routes[i].to_comp)
			continue;

		regmap_update_bits(mmsys->regmap, routes[i].addr,
				   routes[i].mask, routes[i].val);
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_connect);

void mtk_mmsys_ddp_disconnect(struct device *dev,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->routes;
	int i;

	for (i = 0; i < mmsys->data->num_routes; i++) {
		if (cur != routes[i].from_comp || next != routes[i].to_comp)
			continue;

		regmap_clear_bits(mmsys->regmap, routes[i].addr, routes[i].mask);
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_disconnect);

void mtk_mmsys_mdp_write_config(struct device *dev,
				struct mmsys_cmdq_cmd *cmd,
				u32 alias_id, u32 value, u32 mask)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const u32 *configs = mmsys->data->mdp_mmsys_configs;

	cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys,
			    mmsys->addr + configs[alias_id], value, mask);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_write_config);

void mtk_mmsys_mdp_connect(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			   enum mtk_mdp_comp_id cur,
			   enum mtk_mdp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->mdp_routes;
	int i;

	if (!routes) {
		WARN_ON(!routes);
		return;
	}

	WARN_ON(mmsys->subsys_id == 0);
	for (i = 0; i < mmsys->data->mdp_num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp)
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id,
					    mmsys->addr + routes[i].addr,
					    routes[i].val, routes[i].mask);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_connect);

void mtk_mmsys_mdp_disconnect(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			      enum mtk_mdp_comp_id cur,
			      enum mtk_mdp_comp_id next)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->mdp_routes;
	int i;

	WARN_ON(mmsys->subsys_id == 0);
	for (i = 0; i < mmsys->data->mdp_num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp)
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id,
					    mmsys->addr + routes[i].addr,
					    0, routes[i].mask);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_disconnect);

void mtk_mmsys_mdp_isp_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			    enum mtk_mdp_comp_id id)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const unsigned int *isp_ctrl = mmsys->data->mdp_isp_ctrl;
	u32 reg;

	WARN_ON(mmsys->subsys_id == 0);
	/* Direct link */
	if (id == MDP_COMP_CAMIN) {
		/* Reset MDP_DL_ASYNC_TX */
		if (isp_ctrl[ISP_REG_MMSYS_SW0_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MMSYS_SW0_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX]);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX],
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX]);
		}

		/* Reset MDP_DL_ASYNC_RX */
		if (isp_ctrl[ISP_REG_MMSYS_SW1_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MMSYS_SW1_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX]);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX],
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX]);
		}

		/* Enable sof mode */
		if (isp_ctrl[ISP_REG_ISP_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_ISP_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_NO_SOF_MODE]);
		}
	}

	if (id == MDP_COMP_CAMIN2) {
		/* Reset MDP_DL_ASYNC2_TX */
		if (isp_ctrl[ISP_REG_MMSYS_SW0_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MMSYS_SW0_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX2]);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX2],
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_TX2]);
		}

		/* Reset MDP_DL_ASYNC2_RX */
		if (isp_ctrl[ISP_REG_MMSYS_SW1_RST_B]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MMSYS_SW1_RST_B];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX2]);
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX2],
					    isp_ctrl[ISP_BIT_MDP_DL_ASYNC_RX2]);
		}

		/* Enable sof mode */
		if (isp_ctrl[ISP_REG_IPU_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_IPU_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    0x0,
					    isp_ctrl[ISP_BIT_NO_SOF_MODE]);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_isp_ctrl);

void mtk_mmsys_mdp_camin_ctrl(struct device *dev, struct mmsys_cmdq_cmd *cmd,
			      enum mtk_mdp_comp_id id, u32 camin_w, u32 camin_h)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const unsigned int *isp_ctrl = mmsys->data->mdp_isp_ctrl;
	u32 reg;

	WARN_ON(mmsys->subsys_id == 0);
	/* Config for direct link */
	if (id == MDP_COMP_CAMIN) {
		if (isp_ctrl[ISP_REG_MDP_ASYNC_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MDP_ASYNC_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    (camin_h << 16) + camin_w,
					    0x3FFF3FFF);
		}

		if (isp_ctrl[ISP_REG_ISP_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_ISP_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    (camin_h << 16) + camin_w,
					    0x3FFF3FFF);
		}
	}
	if (id == MDP_COMP_CAMIN2) {
		if (isp_ctrl[ISP_REG_MDP_ASYNC_IPU_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_MDP_ASYNC_IPU_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    (camin_h << 16) + camin_w,
					    0x3FFF3FFF);
		}
		if (isp_ctrl[ISP_REG_IPU_RELAY_CFG_WD]) {
			reg = mmsys->addr + isp_ctrl[ISP_REG_IPU_RELAY_CFG_WD];
			cmdq_pkt_write_mask(cmd->pkt, mmsys->subsys_id, reg,
					    (camin_h << 16) + camin_w,
					    0x3FFF3FFF);
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mmsys_mdp_camin_ctrl);

static int mtk_mmsys_reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct mtk_mmsys *mmsys = container_of(rcdev, struct mtk_mmsys, rcdev);
	u32 ctl = do_div(id, MMSYS_SW_RESET_PER_REG);
	u32 reg = mmsys->data->sw_reset_start + (id * sizeof(u32));

	return regmap_set_bits(mmsys->regmap, reg, BIT(ctl));
}

static int mtk_mmsys_reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct mtk_mmsys *mmsys = container_of(rcdev, struct mtk_mmsys, rcdev);
	u32 ctl = do_div(id, MMSYS_SW_RESET_PER_REG);
	u32 reg = mmsys->data->sw_reset_start + (id * sizeof(u32));

	return regmap_clear_bits(mmsys->regmap, reg, BIT(ctl));
}

static int mtk_mmsys_reset(struct reset_controller_dev *rcdev, unsigned long id)
{
	int ret;

	ret = mtk_mmsys_reset_assert(rcdev, id);
	if (ret)
		return ret;

	usleep_range(1000, 1100);

	return mtk_mmsys_reset_deassert(rcdev, id);
}

static const struct reset_control_ops mtk_mmsys_reset_ops = {
	.assert = mtk_mmsys_reset_assert,
	.deassert = mtk_mmsys_reset_deassert,
	.reset = mtk_mmsys_reset,
};

void mtk_mmsys_ddp_config(struct device *dev, enum mtk_mmsys_config_type config,
			  u32 id, u32 val, struct cmdq_pkt *cmdq_pkt)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);
	const struct mtk_mmsys_config *mmsys_config = mmsys->data->config;
	u32 reg_val;
	u32 mask;
	u32 offset;
	int i;

	if (!mmsys->data->num_configs)
		return;

	for (i = 0; i < mmsys->data->num_configs; i++)
		if (config == mmsys_config[i].config && id == mmsys_config[i].id)
			break;

	if (i == mmsys->data->num_configs)
		return;

	offset = mmsys_config[i].addr;
	mask = mmsys_config[i].mask;
	reg_val = val << mmsys_config[i].shift;

#if IS_REACHABLE(CONFIG_MTK_CMDQ)
	if (cmdq_pkt && mmsys->cmdq_base.size) {
		cmdq_pkt_write_mask(cmdq_pkt, mmsys->cmdq_base.subsys,
				    mmsys->cmdq_base.offset + offset, reg_val,
				    mask);
	} else {
#endif
		regmap_update_bits(mmsys->regmap, offset, mask, reg_val);
#if IS_REACHABLE(CONFIG_MTK_CMDQ)
	}
#endif
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_config);

void mtk_mmsys_write_reg(struct device *dev,
			 struct mmsys_cmdq_cmd *cmd,
			 u32 offset, u32 value, u32 mask)
{
	struct mtk_mmsys *mmsys = dev_get_drvdata(dev);

	cmdq_pkt_write_mask(cmd->pkt, mmsys->cmdq_base.subsys,
			    mmsys->addr + offset, value, mask);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_write_reg);

static const struct regmap_config mtk_mmsys_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.fast_io = true,
	.use_relaxed_mmio = true,
};

static int mtk_mmsys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct platform_device *clks;
	struct platform_device *drm;
	struct mtk_mmsys *mmsys;
	struct resource res;
	struct cmdq_client_reg cmdq_reg;
	void __iomem *base;
	int ret;

	mmsys = devm_kzalloc(dev, sizeof(*mmsys), GFP_KERNEL);
	if (!mmsys)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return dev_err_probe(dev, PTR_ERR(base),
				     "Failed to ioremap mmsys registers\n");

	mmsys->regmap = devm_regmap_init_mmio(dev, base, &mtk_mmsys_regmap_config);
	if (IS_ERR(mmsys->regmap))
		return dev_err_probe(dev, PTR_ERR(mmsys->regmap),
				     "Cannot initialize mmsys regmap\n");

	if (of_address_to_resource(dev->of_node, 0, &res) < 0)
		mmsys->addr = 0L;
	else
		mmsys->addr = res.start;

	if (cmdq_dev_get_client_reg(dev, &cmdq_reg, 0) != 0)
		dev_info(dev, "cmdq subsys id has not been set\n");
	mmsys->subsys_id = cmdq_reg.subsys;

	mmsys->data = of_device_get_match_data(&pdev->dev);

	mmsys->rcdev.owner = THIS_MODULE;
	mmsys->rcdev.nr_resets = mmsys->data->num_resets;
	mmsys->rcdev.ops = &mtk_mmsys_reset_ops;
	mmsys->rcdev.of_node = pdev->dev.of_node;
	ret = devm_reset_controller_register(&pdev->dev, &mmsys->rcdev);
	if (ret)
		return dev_err_probe(dev, ret, "Cannot register mmsys reset controller\n");

#if IS_REACHABLE(CONFIG_MTK_CMDQ)
	ret = cmdq_dev_get_client_reg(dev, &mmsys->cmdq_base, 0);
	if (ret)
		dev_dbg(dev, "No mediatek,gce-client-reg!\n");
#endif

	platform_set_drvdata(pdev, mmsys);

	clks = platform_device_register_data(&pdev->dev, mmsys->data->clk_driver,
					     PLATFORM_DEVID_AUTO, NULL, 0);
	if (IS_ERR(clks))
		return PTR_ERR(clks);

	if (mmsys->data->vppsys)
		goto exit;

	drm = platform_device_register_data(&pdev->dev, "mediatek-drm",
					    PLATFORM_DEVID_AUTO, NULL, 0);
	if (IS_ERR(drm)) {
		platform_device_unregister(clks);
		return PTR_ERR(drm);
	}

exit:
	return 0;
}

static const struct of_device_id of_match_mtk_mmsys[] = {
	{
		.compatible = "mediatek,mt2701-mmsys",
		.data = &mt2701_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt2712-mmsys",
		.data = &mt2712_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt6779-mmsys",
		.data = &mt6779_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt6797-mmsys",
		.data = &mt6797_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8167-mmsys",
		.data = &mt8167_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8173-mmsys",
		.data = &mt8173_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8183-mmsys",
		.data = &mt8183_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8192-mmsys",
		.data = &mt8192_mmsys_driver_data,
	},
	{
		.compatible = "mediatek,mt8195-vdosys0",
		.data = &mt8195_vdosys0_driver_data,
	},
	{
		.compatible = "mediatek,mt8195-vdosys1",
		.data = &mt8195_vdosys1_driver_data,
	},
	{
		.compatible = "mediatek,mt8195-vppsys0",
		.data = &mt8195_vppsys0_driver_data,
	},
	{
		.compatible = "mediatek,mt8195-vppsys1",
		.data = &mt8195_vppsys1_driver_data,
	},
	{
		.compatible = "mediatek,mt8365-mmsys",
		.data = &mt8365_mmsys_driver_data,
	},
	{ }
};

static struct platform_driver mtk_mmsys_drv = {
	.driver = {
		.name = "mtk-mmsys",
		.of_match_table = of_match_mtk_mmsys,
	},
	.probe = mtk_mmsys_probe,
};

builtin_platform_driver(mtk_mmsys_drv);
