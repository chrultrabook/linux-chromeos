/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MDP3_CORE_H__
#define __MTK_MDP3_CORE_H__

#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <linux/soc/mediatek/mtk-mmsys.h>
#include <linux/soc/mediatek/mtk-mutex.h>
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-vpu.h"

#define MDP_MODULE_NAME	"mtk-mdp3"

enum mdp_buffer_usage {
	MDP_BUFFER_USAGE_HW_READ,
	MDP_BUFFER_USAGE_MDP,
	MDP_BUFFER_USAGE_MDP2,
	MDP_BUFFER_USAGE_ISP,
	MDP_BUFFER_USAGE_WPE,
};

struct mdp_platform_config {
	bool	rdma_support_10bit;
	bool	rdma_rsz1_sram_sharing;
	bool	rdma_upsample_repeat_only;
	bool	rdma_support_extend_ufo;
	bool	rdma_support_hyfbc;
	bool	rdma_support_afbc;
	bool	rdma_esl_setting;
	bool	rsz_disable_dcm_small_sample;
	bool	rsz_etc_control;
	bool	tdshp_1_1;
	bool	wrot_filter_constraint;
	bool	wrot_support_afbc;
	bool	wrot_support_10bit;
	bool	mdp_version_6885;
	bool	mdp_version_8195;
	u8	tdshp_dyn_contrast_version;
	u32	gce_event_offset;
	bool	support_multi_larb;
};

struct mtk_mdp_driver_data {
	const struct mdp_platform_config *mdp_cfg;
	const enum mdp_comp_event *event;
	unsigned int event_len;
	const struct mdp_comp_data *comp_data;
	const struct mdp_pipe_info *pipe_info;
	unsigned int pipe_info_len;
	const struct mdp_format *format;
	unsigned int format_len;
	const enum mdp_mmsys_config_id *config_table;
};

struct mdp_dev {
	struct platform_device			*pdev;
	struct device				*mdp_mmsys;
	struct mtk_mutex			*mdp_mutex[MDP_PIPE_MAX];
	struct mdp_comp				*comp[MDP_MAX_COMP_COUNT];
	const struct mtk_mdp_driver_data	*mdp_data;
	s32					event[MDP_MAX_EVENT_COUNT];

	struct workqueue_struct			*job_wq;
	struct workqueue_struct			*clock_wq;
	struct mdp_vpu_dev			vpu;
	struct mtk_scp				*scp;
	struct rproc				*rproc_handle;
	/* synchronization protect for accessing vpu working buffer info */
	struct mutex				vpu_lock;
	s32					vpu_count;
	u32					id_count;
	struct ida				mdp_ida;
	struct cmdq_client			*cmdq_clt;
	wait_queue_head_t			callback_wq;

	struct v4l2_device			v4l2_dev;
	struct video_device			*m2m_vdev;
	struct v4l2_m2m_dev			*m2m_dev;
	/* synchronization protect for m2m device operation */
	struct mutex				m2m_lock;
	atomic_t				suspended;
	atomic_t				job_count;
};

struct mdp_pipe_info {
	enum mtk_mdp_pipe_id pipe_id;
	u32 mmsys_id;
	u32 mutex_id;
	u32 sof;
};

int mdp_vpu_get_locked(struct mdp_dev *mdp);
void mdp_vpu_put_locked(struct mdp_dev *mdp);
int mdp_vpu_register(struct mdp_dev *mdp);
void mdp_vpu_unregister(struct mdp_dev *mdp);
void mdp_video_device_release(struct video_device *vdev);

#endif  /* __MTK_MDP3_CORE_H__ */
