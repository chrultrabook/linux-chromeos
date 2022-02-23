/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef MTK_MUTEX_H
#define MTK_MUTEX_H

struct regmap;
struct device;
struct mtk_mutex;

struct mtk_mutex *mtk_mutex_get(struct device *dev);
struct mtk_mutex *mtk_mutex_mdp_get(struct device *dev,
				    enum mtk_mdp_pipe_id id);
int mtk_mutex_prepare(struct mtk_mutex *mutex);
void mtk_mutex_add_comp(struct mtk_mutex *mutex,
			enum mtk_ddp_comp_id id);
u32 mtk_mutex_get_mdp_mod(struct mtk_mutex *mutex, enum mtk_mdp_comp_id id);
void mtk_mutex_enable(struct mtk_mutex *mutex);
void mtk_mutex_disable(struct mtk_mutex *mutex);
void mtk_mutex_remove_comp(struct mtk_mutex *mutex,
			   enum mtk_ddp_comp_id id);
void mtk_mutex_unprepare(struct mtk_mutex *mutex);
void mtk_mutex_put(struct mtk_mutex *mutex);
void mtk_mutex_acquire(struct mtk_mutex *mutex);
void mtk_mutex_release(struct mtk_mutex *mutex);

#endif /* MTK_MUTEX_H */
