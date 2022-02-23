// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include "mtk-mdp3-cmdq.h"
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-m2m.h"

#define MDP_PATH_MAX_COMPS	IMG_MAX_COMPONENTS
#define BYTE_PER_MB_Y				(4)
#define BYTE_PER_MB_C				(2)

struct mdp_path {
	struct mdp_dev		*mdp_dev;
	struct mdp_comp_ctx	comps[MDP_PATH_MAX_COMPS];
	u32			num_comps;
	const struct img_config	*config;
	const struct img_ipi_frameparam *param;
	const struct v4l2_rect	*composes[IMG_MAX_HW_OUTPUTS];
	struct v4l2_rect	bounds[IMG_MAX_HW_OUTPUTS];
};

#define has_op(ctx, op) \
	((ctx)->comp->ops && (ctx)->comp->ops->op)
#define call_op(ctx, op, ...) \
	(has_op(ctx, op) ? (ctx)->comp->ops->op(ctx, ##__VA_ARGS__) : 0)
struct mdp_path_subfrm {
	s32	mutex_id;
	u32	mutex_mod;
	u32	mutex_mod2;
	s32	sofs[MDP_PATH_MAX_COMPS];
	u32	num_sofs;

	s32	mutex2_id;
	u32	mutex2_mod;
	u32	mutex2_mod2;
	s32	sof2s[MDP_PATH_MAX_COMPS];
	u32	num_sof2s;
};

static bool is_output_disable(const struct img_compparam *param, u32 count)
{
	return (count < param->num_subfrms) ?
		(param->frame.output_disable ||
		param->subfrms[count].tile_disable) :
		true;
}

static int mdp_get_mutex_idx(const struct mtk_mdp_driver_data *data,
			     enum mtk_mdp_pipe_id pipe_id)
{
	int i = 0;

	for (i = 0; i < data->pipe_info_len; i++) {
		if (pipe_id == data->pipe_info[i].pipe_id)
			return i;
	}

	return -ENODEV;
}

int mdp_get_event_idx(struct mdp_dev *mdp, enum mdp_comp_event event)
{
	int i = 0;

	for (i = 0; i < mdp->mdp_data->event_len; i++) {
		if (event == mdp->mdp_data->event[i])
			return i;
	}

	return -ENODEV;
}

static int mdp_path_subfrm_require(struct mdp_path_subfrm *subfrm,
				   const struct mdp_path *path,
				   struct mmsys_cmdq_cmd *cmd, u32 count)
{
	const struct img_config *config = path->config;
	const struct mdp_comp_ctx *ctx;
	const struct mtk_mdp_driver_data *data = path->mdp_dev->mdp_data;
	struct device *dev = &path->mdp_dev->pdev->dev;
	struct mtk_mutex **mutex = path->mdp_dev->mdp_mutex;
	struct mtk_mutex **mutex2 = path->mdp_dev->mdp_mutex2;
	s32 mutex_id = MDP_PIPE_NONE;
	s32 mutex2_id = MDP_PIPE_NONE;
	u32 mutex_sof = 0;
	u32 mutex2_sof = 0;
	int index, j;
	enum mtk_mdp_comp_id public_id = MDP_COMP_NONE;

	/* Default value */
	memset(subfrm, 0, sizeof(*subfrm));

	for (index = 0; index < config->num_components; index++) {
		if (is_dummy_engine(path->mdp_dev, config->components[index].type))
			continue;

		ctx = &path->comps[index];
		if (is_output_disable(ctx->param, count))
			continue;

		public_id = path->comps[index].comp->public_id;
		switch (public_id) {
		case MDP_COMP_ISP_IMGI:
			j = mdp_get_mutex_idx(data, MDP_PIPE_IMGI);
			mutex_id = data->pipe_info[j].mutex_id;
			break;
		case MDP_COMP_WPEI:
			j = mdp_get_mutex_idx(data, MDP_PIPE_WPEI);
			mutex_id = data->pipe_info[j].mutex_id;
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_WPEI2:
			j = mdp_get_mutex_idx(data, MDP_PIPE_WPEI2);
			mutex_id = data->pipe_info[j].mutex_id;
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_RDMA0:
			j = mdp_get_mutex_idx(data, MDP_PIPE_RDMA0);
			mutex_id = data->pipe_info[j].mutex_id;
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sofs[subfrm->num_sofs++] = MDP_COMP_RDMA0;
			break;
		case MDP_COMP_VPP1_SOUT:
			j = mdp_get_mutex_idx(data, MDP_PIPE_VPP1_SOUT);
			mutex_id = data->pipe_info[j].mutex_id;
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->mutex2_mod2 |= data->comp_data[public_id].mutex.mod2;
			break;
		case MDP_COMP_FG0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_STITCH:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_HDR0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_AAL0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_RSZ0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sofs[subfrm->num_sofs++] = MDP_COMP_RSZ0;
			break;
		case MDP_COMP_TDSHP0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sofs[subfrm->num_sofs++] = MDP_COMP_TDSHP0;
			break;
		case MDP_COMP_COLOR0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_OVL0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_PAD0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_TCC0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_CCORR0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_WDMA:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sofs[subfrm->num_sofs++] = MDP_COMP_WDMA;
			break;
		case MDP_COMP_WROT0:
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sofs[subfrm->num_sofs++] = MDP_COMP_WROT0;
			break;
		case MDP_COMP_SPLIT:
			j = mdp_get_mutex_idx(data, MDP_PIPE_SPLIT);
			mutex2_id = data->pipe_info[j].mutex_id;
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_SPLIT;
			mutex2_sof = data->pipe_info[j].sof;
			break;
		case MDP_COMP_SPLIT2:
			j = mdp_get_mutex_idx(data, MDP_PIPE_SPLIT2);
			mutex2_id = data->pipe_info[j].mutex_id;
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_SPLIT2;
			mutex2_sof = data->pipe_info[j].sof;
			break;
		case MDP_COMP_RDMA1:
			j = mdp_get_mutex_idx(data, MDP_PIPE_RDMA1);
			mutex2_id = data->pipe_info[j].mutex_id;
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_RDMA1;
			break;
		case MDP_COMP_RDMA2:
			j = mdp_get_mutex_idx(data, MDP_PIPE_RDMA2);
			mutex2_id = data->pipe_info[j].mutex_id;
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_RDMA2;
			break;
		case MDP_COMP_RDMA3:
			j = mdp_get_mutex_idx(data, MDP_PIPE_RDMA3);
			mutex2_id = data->pipe_info[j].mutex_id;
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_RDMA3;
			break;
		case MDP_COMP_VPP0_SOUT:
			j = mdp_get_mutex_idx(data, MDP_PIPE_VPP0_SOUT);
			mutex2_id = data->pipe_info[j].mutex_id;
			subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->mutex2_mod2 |= data->comp_data[public_id].mutex.mod2;
			break;
		case MDP_COMP_TCC1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_FG1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_FG2:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_FG3:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_HDR1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_HDR2:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_HDR3:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_AAL1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_AAL2:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_AAL3:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_RSZ1:
			if (data->comp_data[public_id].mutex.mmsys_id) {
				subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
				subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_RSZ1;
			} else {
				subfrm->mutex_mod |= data->comp_data[public_id].mutex.mod;
				subfrm->sofs[subfrm->num_sofs++] = MDP_COMP_RSZ1;
			}
			break;
		case MDP_COMP_RSZ2:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_RSZ2;
			break;
		case MDP_COMP_RSZ3:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_RSZ3;
			break;
		case MDP_COMP_TDSHP1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_TDSHP1;
			break;
		case MDP_COMP_TDSHP2:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_TDSHP2;
			break;
		case MDP_COMP_TDSHP3:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_TDSHP3;
			break;
		case MDP_COMP_COLOR1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_COLOR2:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_COLOR3:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_OVL1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_PAD1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_PAD2:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_PAD3:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			break;
		case MDP_COMP_WROT1:
			subfrm->mutex2_mod |= data->comp_data[public_id].mutex.mod;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_WROT1;
			break;
		case MDP_COMP_WROT2:
			subfrm->mutex2_mod2 |= data->comp_data[public_id].mutex.mod2;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_WROT2;
			break;
		case MDP_COMP_WROT3:
			subfrm->mutex2_mod2 |= data->comp_data[public_id].mutex.mod2;
			subfrm->sof2s[subfrm->num_sof2s++] = MDP_COMP_WROT3;
			break;
		case MDP_COMP_VDO0DL0:
			subfrm->mutex2_mod2 |= data->comp_data[public_id].mutex.mod2;
			break;
		case MDP_COMP_VDO1DL0:
			subfrm->mutex2_mod2 |= data->comp_data[public_id].mutex.mod2;
			break;
		case MDP_COMP_VDO0DL1:
			subfrm->mutex2_mod2 |= data->comp_data[public_id].mutex.mod2;
			break;
		case MDP_COMP_VDO1DL1:
			subfrm->mutex2_mod2 |= data->comp_data[public_id].mutex.mod2;
			break;
		default:
			break;
		}
	}

	subfrm->mutex_id = mutex_id;
	subfrm->mutex2_id = mutex2_id;

	if (mutex_id == MDP_PIPE_NONE && mutex2_id == MDP_PIPE_NONE) {
		dev_err(dev, "No mutex assigned");
		return -EINVAL;
	}

	/* Set mutex modules */
	if (subfrm->mutex_mod || subfrm->mutex_mod2) {
		mtk_mutex_add_mod_by_cmdq(mutex[mutex_id], subfrm->mutex_mod,
					  subfrm->mutex_mod2, mutex_sof, cmd);
	}

	if (subfrm->mutex2_mod || subfrm->mutex2_mod2) {
		mtk_mutex_add_mod_by_cmdq(mutex2[mutex2_id], subfrm->mutex2_mod,
					  subfrm->mutex2_mod2, mutex2_sof, cmd);
	}
	return 0;
}

static int mdp_path_subfrm_run(const struct mdp_path_subfrm *subfrm,
			       const struct mdp_path *path,
			       struct mmsys_cmdq_cmd *cmd)
{
	struct device *dev = &path->mdp_dev->pdev->dev;
	struct mtk_mutex **mutex = path->mdp_dev->mdp_mutex;
	struct mtk_mutex **mutex2 = path->mdp_dev->mdp_mutex2;
	s32 mutex_id = subfrm->mutex_id;
	s32 mutex2_id = subfrm->mutex2_id;

	if (mutex_id == MDP_PIPE_NONE && mutex2_id == MDP_PIPE_NONE) {
		dev_err(dev, "Incorrect mutex id");
		return -EINVAL;
	}

	if (subfrm->mutex_mod || subfrm->mutex_mod2) {
		int index, evt;

		/* Wait WROT SRAM shared to DISP RDMA */
		/* Clear SOF event for each engine */
		for (index = 0; index < subfrm->num_sofs; index++) {
			switch (subfrm->sofs[index]) {
			case MDP_COMP_RDMA0:
				evt = mdp_get_event_idx(path->mdp_dev, RDMA0_SOF);
				break;
			case MDP_COMP_TDSHP0:
				evt = mdp_get_event_idx(path->mdp_dev, TDSHP0_SOF);
				break;
			case MDP_COMP_RSZ0:
				evt = mdp_get_event_idx(path->mdp_dev, RSZ0_SOF);
				break;
			case MDP_COMP_RSZ1:
				evt = mdp_get_event_idx(path->mdp_dev, RSZ1_SOF);
				break;
			case MDP_COMP_WDMA:
				evt = mdp_get_event_idx(path->mdp_dev, WDMA0_SOF);
				break;
			case MDP_COMP_WROT0:
				evt = mdp_get_event_idx(path->mdp_dev, WROT0_SOF);
				break;
			default:
				evt = -1;
				break;
			}
			if (evt > 0)
				MM_REG_CLEAR(cmd, evt);
		}

		/* Enable the mutex */
		mtk_mutex_enable_by_cmdq(mutex[mutex_id], cmd);

		/* Wait SOF events and clear mutex modules (optional) */
		for (index = 0; index < subfrm->num_sofs; index++) {
			switch (subfrm->sofs[index]) {
			case MDP_COMP_RDMA0:
				evt = mdp_get_event_idx(path->mdp_dev, RDMA0_SOF);
				break;
			case MDP_COMP_TDSHP0:
				evt = mdp_get_event_idx(path->mdp_dev, TDSHP0_SOF);
				break;
			case MDP_COMP_RSZ0:
				evt = mdp_get_event_idx(path->mdp_dev, RSZ0_SOF);
				break;
			case MDP_COMP_RSZ1:
				evt = mdp_get_event_idx(path->mdp_dev, RSZ1_SOF);
				break;
			case MDP_COMP_WDMA:
				evt = mdp_get_event_idx(path->mdp_dev, WDMA0_SOF);
				break;
			case MDP_COMP_WROT0:
				evt = mdp_get_event_idx(path->mdp_dev, WROT0_SOF);
				break;
			default:
				evt = -1;
				break;
			}
			if (evt > 0)
				MM_REG_WAIT(cmd, evt);
		}
	}

	if (subfrm->mutex2_mod || subfrm->mutex2_mod2) {
		int index, evt;

		/* Clear SOF event for each engine */
		for (index = 0; index < subfrm->num_sof2s; index++) {
			switch (subfrm->sof2s[index]) {
			case MDP_COMP_RDMA1:
				evt = mdp_get_event_idx(path->mdp_dev, RDMA1_SOF);
				break;
			case MDP_COMP_RDMA2:
				evt = mdp_get_event_idx(path->mdp_dev, RDMA2_SOF);
				break;
			case MDP_COMP_RDMA3:
				evt = mdp_get_event_idx(path->mdp_dev, RDMA3_SOF);
				break;
			case MDP_COMP_WROT1:
				evt = mdp_get_event_idx(path->mdp_dev, WROT1_SOF);
				break;
			case MDP_COMP_WROT2:
				evt = mdp_get_event_idx(path->mdp_dev, WROT2_SOF);
				break;
			case MDP_COMP_WROT3:
				evt = mdp_get_event_idx(path->mdp_dev, WROT3_SOF);
				break;
			default:
				evt = -1;
				break;
			}
			if (evt > 0)
				MM_REG_CLEAR(cmd, evt);
		}

		/* Enable the mutex */
		mtk_mutex_enable_by_cmdq(mutex2[mutex2_id], cmd);

		/* Wait SOF events and clear mutex modules (optional) */
		for (index = 0; index < subfrm->num_sof2s; index++) {
			switch (subfrm->sof2s[index]) {
			case MDP_COMP_RDMA1:
				evt = mdp_get_event_idx(path->mdp_dev, RDMA1_SOF);
				break;
			case MDP_COMP_RDMA2:
				evt = mdp_get_event_idx(path->mdp_dev, RDMA2_SOF);
				break;
			case MDP_COMP_RDMA3:
				evt = mdp_get_event_idx(path->mdp_dev, RDMA3_SOF);
				break;
			case MDP_COMP_WROT1:
				evt = mdp_get_event_idx(path->mdp_dev, WROT1_SOF);
				break;
			case MDP_COMP_WROT2:
				evt = mdp_get_event_idx(path->mdp_dev, WROT2_SOF);
				break;
			case MDP_COMP_WROT3:
				evt = mdp_get_event_idx(path->mdp_dev, WROT3_SOF);
				break;
			default:
				evt = -1;
				break;
			}
			if (evt > 0)
				MM_REG_WAIT(cmd, evt);
		}
	}
	return 0;
}

static int mdp_path_ctx_init(struct mdp_dev *mdp, struct mdp_path *path)
{
	const struct img_config *config = path->config;
	int index, ret;

	if (config->num_components < 1)
		return -EINVAL;

	for (index = 0; index < config->num_components; index++) {
		if (is_dummy_engine(mdp, config->components[index].type))
			continue;

		ret = mdp_comp_ctx_init(mdp, &path->comps[index],
					&config->components[index],
					path->param);
		if (ret)
			return ret;
	}

	return 0;
}

static int mdp_path_config_subfrm(struct mmsys_cmdq_cmd *cmd,
				  struct mdp_path *path, u32 count)
{
	struct mdp_path_subfrm subfrm;
	const struct img_config *config = path->config;
	const struct img_mmsys_ctrl *ctrl = &config->ctrls[count];
	const struct img_mux *set;
	struct device *mmsys_dev = path->mdp_dev->mdp_mmsys;
	const struct mtk_mdp_driver_data *data = path->mdp_dev->mdp_data;
	struct mdp_comp_ctx *ctx;
	int index, ret;

	/* Acquire components */
	ret = mdp_path_subfrm_require(&subfrm, path, cmd, count);
	if (ret)
		return ret;
	/* Enable mux settings */
	for (index = 0; index < ctrl->num_sets; index++) {
		set = &ctrl->sets[index];
		if (data->mdp_cfg->mdp_version_8195) {
			if (set->vpp_id)
				mmsys_dev = path->mdp_dev->mdp_mmsys2;
			else
				mmsys_dev = path->mdp_dev->mdp_mmsys;
		}

		mtk_mmsys_write_reg_by_cmdq(mmsys_dev, cmd,
					    set->reg, set->value, 0xFFFFFFFF);
	}
	/* Config sub-frame information */
	for (index = (config->num_components - 1); index >= 0; index--) {
		if (is_dummy_engine(path->mdp_dev, config->components[index].type))
			continue;

		ctx = &path->comps[index];
		if (is_output_disable(ctx->param, count))
			continue;
		ret = call_op(ctx, config_subfrm, cmd, count);
		if (ret)
			return ret;
	}
	/* Run components */
	ret = mdp_path_subfrm_run(&subfrm, path, cmd);
	if (ret)
		return ret;
	/* Wait components done */
	for (index = 0; index < config->num_components; index++) {
		if (is_dummy_engine(path->mdp_dev, config->components[index].type))
			continue;

		ctx = &path->comps[index];
		if (is_output_disable(ctx->param, count))
			continue;
		ret = call_op(ctx, wait_comp_event, cmd);
		if (ret)
			return ret;
	}
	/* Advance to the next sub-frame */
	for (index = 0; index < config->num_components; index++) {
		if (is_dummy_engine(path->mdp_dev, config->components[index].type))
			continue;

		ctx = &path->comps[index];
		ret = call_op(ctx, advance_subfrm, cmd, count);
		if (ret)
			return ret;
	}
	/* Disable mux settings */
	for (index = 0; index < ctrl->num_sets; index++) {
		set = &ctrl->sets[index];
		if (data->mdp_cfg->mdp_version_8195) {
			if (set->vpp_id)
				mmsys_dev = path->mdp_dev->mdp_mmsys2;
			else
				mmsys_dev = path->mdp_dev->mdp_mmsys;
		}

		mtk_mmsys_write_reg_by_cmdq(mmsys_dev, cmd,
					    set->reg, 0, 0xFFFFFFFF);
	}

	return 0;
}

static int mdp_path_config(struct mdp_dev *mdp, struct mmsys_cmdq_cmd *cmd,
			   struct mdp_path *path)
{
	const struct img_config *config = path->config;
	struct mdp_comp_ctx *ctx;
	int index, count, ret;

	/* Config path frame */
	/* Reset components */
	for (index = 0; index < config->num_components; index++) {
		if (is_dummy_engine(mdp, config->components[index].type))
			continue;

		ctx = &path->comps[index];
		ret = call_op(ctx, init_comp, cmd);
		if (ret)
			return ret;
	}
	/* Config frame mode */
	for (index = 0; index < config->num_components; index++) {
		const struct v4l2_rect *compose =
			path->composes[ctx->param->outputs[0]];

		if (is_dummy_engine(mdp, config->components[index].type))
			continue;

		ctx = &path->comps[index];
		ret = call_op(ctx, config_frame, cmd, compose);
		if (ret)
			return ret;
	}

	/* Config path sub-frames */
	for (count = 0; count < config->num_subfrms; count++) {
		ret = mdp_path_config_subfrm(cmd, path, count);
		if (ret)
			return ret;
	}
	/* Post processing information */
	for (index = 0; index < config->num_components; index++) {
		if (is_dummy_engine(mdp, config->components[index].type))
			continue;

		ctx = &path->comps[index];
		ret = call_op(ctx, post_process, cmd);
		if (ret)
			return ret;
	}
	return 0;
}

static int mdp_hyfbc_config(struct mdp_dev *mdp, struct mmsys_cmdq_cmd *cmd,
			    struct mdp_path *path, struct mdp_cmdq_param *param)
{
	struct device *dev = &mdp->pdev->dev;
	const struct img_config *config = path->config;
	struct mdp_m2m_ctx *ctx;
	struct mdp_comp_ctx *comp_ctx = &path->comps[0];
	const struct mdp_rdma_data *rdma = &comp_ctx->param->rdma;
	struct hyfbc_init_info hyfbc;
	struct mdp_frame *frame;
	u32 wrot_id;
	int ret;

	ctx = (struct mdp_m2m_ctx *)param->mdp_ctx;
	frame = &ctx->curr_param.output;

	if (!MDP_COLOR_IS_HYFBC_COMPRESS(frame->mdp_fmt->mdp_color) ||
	    frame->format.fmt.pix_mp.width % 32 == 0)
		return 0;

	// First engine should be rdma engine
	if (!is_rdma(mdp, config->components[0].type)) {
		dev_dbg(dev, "Not RDMA engine id, end patch.");
		return 0;
	}

	wrot_id = config->components[(config->num_components - 1)].type;

	hyfbc.is10b = (MDP_COLOR_IS_10BIT(frame->mdp_fmt->mdp_color));
	hyfbc.width_in_mb = DIV_ROUND_UP(frame->format.fmt.pix_mp.width, 16);
	hyfbc.height_in_mb = DIV_ROUND_UP(frame->format.fmt.pix_mp.height, 16);
	hyfbc.w_stride_in_mb = DIV_ROUND_UP(ALIGN(frame->stride.width, 32), 16);
	hyfbc.h_stride_in_mb = DIV_ROUND_UP(ALIGN(frame->stride.height, 32), 16);
	hyfbc.byte_per_mb = BYTE_PER_MB_Y;
	hyfbc.pa_base = rdma->ufo_dec_y;

	ret = mdp_hyfbc_init(mdp, cmd, &hyfbc, wrot_id);
	if (ret) {
		dev_err(dev, "mdp_hyfbc_init: y patch fail.");
		return ret;
	}

	hyfbc.byte_per_mb = BYTE_PER_MB_C;
	hyfbc.pa_base = rdma->ufo_dec_c;

	ret = mdp_hyfbc_init(mdp, cmd, &hyfbc, wrot_id);
	if (ret) {
		dev_err(dev, "mdp_hyfbc_init: c patch fail.");
		return ret;
	}

	return 0;
}

static void mdp_auto_release_work(struct work_struct *work)
{
	struct mdp_cmdq_cb_param *cb_param;
	struct mdp_dev *mdp;
	int i;

	cb_param = container_of(work, struct mdp_cmdq_cb_param,
				auto_release_work);
	mdp = cb_param->mdp;

	i = mdp_get_mutex_idx(mdp->mdp_data, MDP_PIPE_RDMA0);
	mtk_mutex_unprepare(mdp->mdp_mutex[mdp->mdp_data->pipe_info[i].mutex_id]);

	i = mdp_get_mutex_idx(mdp->mdp_data, MDP_PIPE_RDMA1);
	if (i >= 0)
		mtk_mutex_unprepare(mdp->mdp_mutex2[mdp->mdp_data->pipe_info[i].mutex_id]);
	mdp_comp_clocks_off(&mdp->pdev->dev, cb_param->comps,
			    cb_param->num_comps);

	kfree(cb_param->comps);
	kfree(cb_param);

	atomic_dec(&mdp->job_count);
	wake_up(&mdp->callback_wq);
}

static void mdp_handle_cmdq_callback(struct cmdq_cb_data data)
{
	struct mdp_cmdq_cb_param *cb_param;
	struct mdp_dev *mdp;
	struct device *dev;
	int i;

	if (!data.data) {
		pr_info("%s:no callback data\n", __func__);
		return;
	}

	cb_param = (struct mdp_cmdq_cb_param *)data.data;
	mdp = cb_param->mdp;
	dev = &mdp->pdev->dev;

	if (cb_param->mdp_ctx)
		mdp_m2m_job_finish(cb_param->mdp_ctx);

	if (cb_param->user_cmdq_cb) {
		struct cmdq_cb_data user_cb_data;

		user_cb_data.sta = data.sta;
		user_cb_data.data = cb_param->user_cb_data;
		cb_param->user_cmdq_cb(user_cb_data);
	}

	cmdq_pkt_destroy(cb_param->pkt);
	INIT_WORK(&cb_param->auto_release_work, mdp_auto_release_work);
	if (!queue_work(mdp->clock_wq, &cb_param->auto_release_work)) {
		dev_err(dev, "%s:queue_work fail!\n", __func__);
		i = mdp_get_mutex_idx(mdp->mdp_data, MDP_PIPE_RDMA0);
		mtk_mutex_unprepare(mdp->mdp_mutex[mdp->mdp_data->pipe_info[i].mutex_id]);

		i = mdp_get_mutex_idx(mdp->mdp_data, MDP_PIPE_RDMA1);
		if (i >= 0)
			mtk_mutex_unprepare(mdp->mdp_mutex2[mdp->mdp_data->pipe_info[i].mutex_id]);

		mdp_comp_clocks_off(&mdp->pdev->dev, cb_param->comps,
				    cb_param->num_comps);

		kfree(cb_param->comps);
		kfree(cb_param);

		atomic_dec(&mdp->job_count);
		wake_up(&mdp->callback_wq);
	}
}

int mdp_cmdq_send(struct mdp_dev *mdp, struct mdp_cmdq_param *param)
{
	struct mmsys_cmdq_cmd cmd;
	struct mdp_path *path = NULL;
	struct mdp_cmdq_cb_param *cb_param = NULL;
	struct mdp_comp *comps = NULL;
	struct device *dev = &mdp->pdev->dev;
	int i, ret;

	if (atomic_read(&mdp->suspended))
		return -ECANCELED;

	atomic_inc(&mdp->job_count);

	cmd.pkt = cmdq_pkt_create(mdp->cmdq_clt, SZ_16K);
	if (IS_ERR(cmd.pkt)) {
		atomic_dec(&mdp->job_count);
		wake_up(&mdp->callback_wq);
		return PTR_ERR(cmd.pkt);
	}
	cmd.event = &mdp->event[0];

	path = kzalloc(sizeof(*path), GFP_KERNEL);
	if (!path) {
		ret = -ENOMEM;
		goto err_destroy_pkt;
	}

	path->mdp_dev = mdp;
	path->config = param->config;
	path->param = param->param;
	for (i = 0; i < param->param->num_outputs; i++) {
		path->bounds[i].left = 0;
		path->bounds[i].top = 0;
		path->bounds[i].width =
			param->param->outputs[i].buffer.format.width;
		path->bounds[i].height =
			param->param->outputs[i].buffer.format.height;
		path->composes[i] = param->composes[i] ?
			param->composes[i] : &path->bounds[i];
	}

	ret = mdp_path_ctx_init(mdp, path);
	if (ret) {
		dev_err(dev, "mdp_path_ctx_init error\n");
		goto err_destroy_pkt;
	}

	i = mdp_get_mutex_idx(mdp->mdp_data, MDP_PIPE_RDMA0);
	mtk_mutex_prepare(mdp->mdp_mutex[mdp->mdp_data->pipe_info[i].mutex_id]);

	i = mdp_get_mutex_idx(mdp->mdp_data, MDP_PIPE_RDMA1);
	if (i >= 0)
		mtk_mutex_prepare(mdp->mdp_mutex2[mdp->mdp_data->pipe_info[i].mutex_id]);

	for (i = 0; i < param->config->num_components; i++) {
		if (is_dummy_engine(mdp, path->config->components[i].type))
			continue;

		mdp_comp_clock_on(&mdp->pdev->dev, path->comps[i].comp);
	}

	if (mdp->mdp_data->mdp_cfg->mdp_version_8195) {
		ret = mdp_hyfbc_config(mdp, &cmd, path, param);
		if (ret)
			goto err_destroy_pkt;
	}

	ret = mdp_path_config(mdp, &cmd, path);
	if (ret) {
		dev_err(dev, "mdp_path_config error\n");
		goto err_destroy_pkt;
	}

	cb_param = kzalloc(sizeof(*cb_param), GFP_KERNEL);
	if (!cb_param) {
		ret = -ENOMEM;
		goto err_destroy_pkt;
	}

	comps = kcalloc(param->config->num_components, sizeof(*comps),
			GFP_KERNEL);
	if (!comps) {
		ret = -ENOMEM;
		goto err_destroy_pkt;
	}

	for (i = 0; i < param->config->num_components; i++) {
		if (is_dummy_engine(mdp, path->config->components[i].type))
			continue;

		memcpy(&comps[i], path->comps[i].comp,
		       sizeof(struct mdp_comp));
	}
	cb_param->mdp = mdp;
	cb_param->user_cmdq_cb = param->cmdq_cb;
	cb_param->user_cb_data = param->cb_data;
	cb_param->pkt = cmd.pkt;
	cb_param->comps = comps;
	cb_param->num_comps = param->config->num_components;
	cb_param->mdp_ctx = param->mdp_ctx;

	cmdq_pkt_finalize(cmd.pkt);
	ret = cmdq_pkt_flush_async(cmd.pkt,
				   mdp_handle_cmdq_callback,
				   (void *)cb_param);
	if (ret) {
		dev_err(dev, "cmdq_pkt_flush_async fail!\n");
		goto err_clock_off;
	}
	kfree(path);
	return 0;

err_clock_off:
	i = mdp_get_mutex_idx(mdp->mdp_data, MDP_PIPE_RDMA0);
	mtk_mutex_unprepare(mdp->mdp_mutex[mdp->mdp_data->pipe_info[i].mutex_id]);

	i = mdp_get_mutex_idx(mdp->mdp_data, MDP_PIPE_RDMA1);
	if (i >= 0)
		mtk_mutex_unprepare(mdp->mdp_mutex2[mdp->mdp_data->pipe_info[i].mutex_id]);

	mdp_comp_clocks_off(&mdp->pdev->dev, cb_param->comps,
			    cb_param->num_comps);
err_destroy_pkt:
	cmdq_pkt_destroy(cmd.pkt);
	atomic_dec(&mdp->job_count);
	wake_up(&mdp->callback_wq);
	kfree(comps);
	kfree(cb_param);
	kfree(path);

	return ret;
}

int mdp_cmdq_sendtask(struct platform_device *pdev, struct img_config *config,
		      struct img_ipi_frameparam *param,
		      struct v4l2_rect *compose,
		      void (*cmdq_cb)(struct cmdq_cb_data data), void *cb_data)
{
	struct mdp_dev *mdp = platform_get_drvdata(pdev);
	struct mdp_cmdq_param task = {
		.config = config,
		.param = param,
		.composes[0] = compose,
		.cmdq_cb = cmdq_cb,
		.cb_data = cb_data,
	};

	return mdp_cmdq_send(mdp, &task);
}
EXPORT_SYMBOL_GPL(mdp_cmdq_sendtask);
