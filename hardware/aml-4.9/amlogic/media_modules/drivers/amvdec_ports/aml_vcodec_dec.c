/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"
//#include "aml_vcodec_intr.h"
#include "aml_vcodec_util.h"
#include "vdec_drv_if.h"
#include "aml_vcodec_dec_pm.h"
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/crc32.h>
#include "aml_vcodec_adapt.h"
#include <linux/spinlock.h>

#include "aml_vcodec_vfm.h"
#include "../frame_provider/decoder/utils/decoder_bmmu_box.h"
#include "../frame_provider/decoder/utils/decoder_mmu_box.h"

#define OUT_FMT_IDX	0 //default h264
#define CAP_FMT_IDX	8 //capture nv21

#define AML_VDEC_MIN_W	64U
#define AML_VDEC_MIN_H	64U
#define DFT_CFG_WIDTH	AML_VDEC_MIN_W
#define DFT_CFG_HEIGHT	AML_VDEC_MIN_H

#define V4L2_CID_USER_AMLOGIC_BASE (V4L2_CID_USER_BASE + 0x1100)
#define AML_V4L2_SET_DECMODE (V4L2_CID_USER_AMLOGIC_BASE + 0)

#define WORK_ITEMS_MAX (32)

//#define USEC_PER_SEC 1000000

#define call_void_memop(vb, op, args...)				\
	do {								\
		if ((vb)->vb2_queue->mem_ops->op)			\
			(vb)->vb2_queue->mem_ops->op(args);		\
	} while (0)

static struct aml_video_fmt aml_video_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_H264,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_HEVC,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG1,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MJPEG,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV21,
		.type = AML_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV21M,
		.type = AML_FMT_FRAME,
		.num_planes = 2,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.type = AML_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12M,
		.type = AML_FMT_FRAME,
		.num_planes = 2,
	},
};

static const struct aml_codec_framesizes aml_vdec_framesizes[] = {
	{
		.fourcc	= V4L2_PIX_FMT_H264,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 8,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 8 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_HEVC,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 8,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 8 },
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 8,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 8 },
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG1,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 8,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 8 },
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 8,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 8 },
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 8,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 8 },
	},
	{
		.fourcc = V4L2_PIX_FMT_MJPEG,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 8,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 8 },
	},
};

#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(aml_vdec_framesizes)
#define NUM_FORMATS ARRAY_SIZE(aml_video_formats)

extern bool multiplanar;

extern int dmabuf_fd_install_data(int fd, void* data, u32 size);
extern bool is_v4l2_buf_file(struct file *file);

static ulong aml_vcodec_ctx_lock(struct aml_vcodec_ctx *ctx)
{
	ulong flags;

	spin_lock_irqsave(&ctx->slock, flags);

	return flags;
}

static void aml_vcodec_ctx_unlock(struct aml_vcodec_ctx *ctx, ulong flags)
{
	spin_unlock_irqrestore(&ctx->slock, flags);
}

static struct aml_video_fmt *aml_vdec_find_format(struct v4l2_format *f)
{
	struct aml_video_fmt *fmt;
	unsigned int k;

	aml_v4l2_debug(4, "%s, type: %u, planes: %u, fmt: %u\n",
		__func__, f->type, f->fmt.pix_mp.num_planes,
		f->fmt.pix_mp.pixelformat);

	for (k = 0; k < NUM_FORMATS; k++) {
		fmt = &aml_video_formats[k];
		if (fmt->fourcc == f->fmt.pix_mp.pixelformat)
			return fmt;
	}

	return NULL;
}

static struct aml_q_data *aml_vdec_get_q_data(struct aml_vcodec_ctx *ctx,
					      enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->q_data[AML_Q_DATA_SRC];

	return &ctx->q_data[AML_Q_DATA_DST];
}

void aml_vdec_dispatch_event(struct aml_vcodec_ctx *ctx, u32 changes)
{
	struct v4l2_event event = {0};

	if (ctx->receive_cmd_stop) {
		ctx->state = AML_STATE_ABORT;
		changes = V4L2_EVENT_REQUEST_EXIT;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
				ctx->id, __func__);
	}

	switch (changes) {
	case V4L2_EVENT_SRC_CH_RESOLUTION:
	case V4L2_EVENT_SRC_CH_HDRINFO:
	case V4L2_EVENT_REQUEST_RESET:
	case V4L2_EVENT_REQUEST_EXIT:
		event.type = V4L2_EVENT_SOURCE_CHANGE;
		event.u.src_change.changes = changes;
		break;
	default:
		pr_err("unsupport dispatch event %x\n", changes);
		return;
	}

	v4l2_event_queue_fh(&ctx->fh, &event);
	aml_v4l2_debug(3, "[%d] %s() changes: %x",
		ctx->id, __func__, changes);
}

static void aml_vdec_flush_decoder(struct aml_vcodec_ctx *ctx)
{
	aml_v4l2_debug(3, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);

	aml_decoder_flush(ctx->ada_ctx);
}

static void aml_vdec_pic_info_update(struct aml_vcodec_ctx *ctx)
{
	unsigned int dpbsize = 0;
	int ret;

	if (vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->last_decoded_picinfo)) {
		aml_v4l2_err("[%d] Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR", ctx->id);
		return;
	}

	if (ctx->last_decoded_picinfo.visible_width == 0 ||
		ctx->last_decoded_picinfo.visible_height == 0 ||
		ctx->last_decoded_picinfo.coded_width == 0 ||
		ctx->last_decoded_picinfo.coded_height == 0) {
		aml_v4l2_err("Cannot get correct pic info");
		return;
	}

	/*if ((ctx->last_decoded_picinfo.visible_width == ctx->picinfo.visible_width) ||
	    (ctx->last_decoded_picinfo.visible_height == ctx->picinfo.visible_height))
		return;*/

	aml_v4l2_debug(4, "[%d]-> new(%d,%d), old(%d,%d), real(%d,%d)",
			ctx->id, ctx->last_decoded_picinfo.visible_width,
			ctx->last_decoded_picinfo.visible_height,
			ctx->picinfo.visible_width, ctx->picinfo.visible_height,
			ctx->last_decoded_picinfo.coded_width,
			ctx->last_decoded_picinfo.coded_width);

	ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
	if (dpbsize == 0)
		aml_v4l2_err("[%d] Incorrect dpb size, ret=%d", ctx->id, ret);

	/* update picture information */
	ctx->dpb_size = dpbsize;
	ctx->picinfo = ctx->last_decoded_picinfo;
}

void vdec_frame_buffer_release(void *data)
{
	struct file_privdata *priv_data =
		(struct file_privdata *) data;
	struct vframe_s *vf = &priv_data->vf;

	if (decoder_bmmu_box_valide_check(vf->mm_box.bmmu_box)) {
		decoder_bmmu_box_free_idx(vf->mm_box.bmmu_box,
			vf->mm_box.bmmu_idx);
		decoder_bmmu_try_to_release_box(vf->mm_box.bmmu_box);
	}

	if (decoder_mmu_box_valide_check(vf->mm_box.mmu_box)) {
		decoder_mmu_box_free_idx(vf->mm_box.mmu_box,
			vf->mm_box.mmu_idx);
		decoder_mmu_try_to_release_box(vf->mm_box.mmu_box);
	}

	aml_v4l2_debug(2, "%s vf idx: %d, bmmu idx: %d, bmmu_box: %p",
		__func__, vf->index, vf->mm_box.bmmu_idx, vf->mm_box.bmmu_box);
	aml_v4l2_debug(2, "%s vf idx: %d, mmu_idx: %d, mmu_box: %p",
		__func__, vf->index, vf->mm_box.mmu_idx, vf->mm_box.mmu_box);

	memset(data, 0, sizeof(struct file_privdata));
	kfree(data);
}

int get_fb_from_queue(struct aml_vcodec_ctx *ctx, struct vdec_v4l2_buffer **out_fb)
{
	ulong flags;
	struct vb2_buffer *dst_buf = NULL;
	struct vdec_v4l2_buffer *pfb;
	struct aml_video_dec_buf *dst_buf_info, *info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2;

	flags = aml_vcodec_ctx_lock(ctx);

	if (ctx->state == AML_STATE_ABORT) {
		aml_vcodec_ctx_unlock(ctx, flags);
		return -1;
	}

	dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (!dst_buf) {
		aml_vcodec_ctx_unlock(ctx, flags);
		return -1;
	}

	aml_v4l2_debug(2, "[%d] %s() vbuf idx: %d, state: %d, ready: %d",
			ctx->id, __func__, dst_buf->index, dst_buf->state,
			v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx));

	dst_vb2_v4l2 = container_of(dst_buf, struct vb2_v4l2_buffer, vb2_buf);
	dst_buf_info = container_of(dst_vb2_v4l2, struct aml_video_dec_buf, vb);

	if (ctx->scatter_mem_enable) {
		pfb			= &dst_buf_info->frame_buffer;
		pfb->mem_type		= VDEC_SCATTER_MEMORY_TYPE;
		pfb->status		= FB_ST_NORMAL;
	} else if (dst_buf->num_planes == 1) {
		pfb = &dst_buf_info->frame_buffer;
		pfb->m.mem[0].dma_addr	= vb2_dma_contig_plane_dma_addr(dst_buf, 0);
		pfb->m.mem[0].addr	= dma_to_phys(v4l_get_dev_from_codec_mm(), pfb->m.mem[0].dma_addr);
		pfb->m.mem[0].size	= ctx->picinfo.y_len_sz + ctx->picinfo.c_len_sz;
		pfb->m.mem[0].offset	= ctx->picinfo.y_len_sz;
		pfb->num_planes		= dst_buf->num_planes;
		pfb->status		= FB_ST_NORMAL;

		aml_v4l2_debug(4, "[%d] idx: %u, 1 plane, y:(0x%lx, %d)",
			ctx->id, dst_buf->index,
			pfb->m.mem[0].addr, pfb->m.mem[0].size);
	} else if (dst_buf->num_planes == 2) {
		pfb			= &dst_buf_info->frame_buffer;
		pfb->m.mem[0].dma_addr	= vb2_dma_contig_plane_dma_addr(dst_buf, 0);
		pfb->m.mem[0].addr	= dma_to_phys(v4l_get_dev_from_codec_mm(), pfb->m.mem[0].dma_addr);
		pfb->m.mem[0].size	= ctx->picinfo.y_len_sz;
		pfb->m.mem[0].offset	= 0;

		pfb->m.mem[1].dma_addr	= vb2_dma_contig_plane_dma_addr(dst_buf, 1);
		pfb->m.mem[1].addr	= dma_to_phys(v4l_get_dev_from_codec_mm(), pfb->m.mem[1].dma_addr);
		pfb->m.mem[1].size	= ctx->picinfo.c_len_sz;
		pfb->m.mem[1].offset	= ctx->picinfo.c_len_sz >> 1;
		pfb->num_planes		= dst_buf->num_planes;
		pfb->status		= FB_ST_NORMAL;

		aml_v4l2_debug(4, "[%d] idx: %u, 2 planes, y:(0x%lx, %d), c:(0x%lx, %d)",
			ctx->id, dst_buf->index,
			pfb->m.mem[0].addr, pfb->m.mem[0].size,
			pfb->m.mem[1].addr, pfb->m.mem[1].size);
	} else {
		pfb			= &dst_buf_info->frame_buffer;
		pfb->m.mem[0].dma_addr	= vb2_dma_contig_plane_dma_addr(dst_buf, 0);
		pfb->m.mem[0].addr	= dma_to_phys(v4l_get_dev_from_codec_mm(), pfb->m.mem[0].dma_addr);
		pfb->m.mem[0].size	= ctx->picinfo.y_len_sz;
		pfb->m.mem[0].offset	= 0;

		pfb->m.mem[1].dma_addr	= vb2_dma_contig_plane_dma_addr(dst_buf, 1);
		pfb->m.mem[1].addr	= dma_to_phys(v4l_get_dev_from_codec_mm(), pfb->m.mem[2].dma_addr);
		pfb->m.mem[1].size	= ctx->picinfo.c_len_sz >> 1;
		pfb->m.mem[1].offset	= 0;

		pfb->m.mem[2].dma_addr	= vb2_dma_contig_plane_dma_addr(dst_buf, 2);
		pfb->m.mem[2].addr	= dma_to_phys(v4l_get_dev_from_codec_mm(), pfb->m.mem[3].dma_addr);
		pfb->m.mem[2].size	= ctx->picinfo.c_len_sz >> 1;
		pfb->m.mem[2].offset	= 0;
		pfb->num_planes		= dst_buf->num_planes;
		pfb->status		= FB_ST_NORMAL;

		aml_v4l2_debug(4, "[%d] idx: %u, 3 planes, y:(0x%lx, %d), u:(0x%lx, %d), v:(0x%lx, %d)",
			ctx->id, dst_buf->index,
			pfb->m.mem[0].addr, pfb->m.mem[0].size,
			pfb->m.mem[1].addr, pfb->m.mem[1].size,
			pfb->m.mem[2].addr, pfb->m.mem[2].size);
	}

	dst_buf_info->used = true;
	ctx->buf_used_count++;

	*out_fb = pfb;

	info = container_of(pfb, struct aml_video_dec_buf, frame_buffer);

	v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	aml_vcodec_ctx_unlock(ctx, flags);

	return 0;
}
EXPORT_SYMBOL(get_fb_from_queue);

int put_fb_to_queue(struct aml_vcodec_ctx *ctx, struct vdec_v4l2_buffer *in_fb)
{
	struct aml_video_dec_buf *dstbuf;

	pr_info("[%d] %s() [%d]\n", ctx->id, __func__, __LINE__);

	if (in_fb == NULL) {
		aml_v4l2_debug(4, "[%d] No free frame buffer", ctx->id);
		return -1;
	}

	aml_v4l2_debug(4, "[%d] tmp_frame_addr = 0x%p", ctx->id, in_fb);

	dstbuf = container_of(in_fb, struct aml_video_dec_buf, frame_buffer);

	mutex_lock(&ctx->lock);

	if (!dstbuf->used)
		goto out;

	aml_v4l2_debug(4,
			"[%d] status=%x queue id=%d to rdy_queue",
			ctx->id, in_fb->status,
			dstbuf->vb.vb2_buf.index);

	v4l2_m2m_buf_queue(ctx->m2m_ctx, &dstbuf->vb);

	dstbuf->used = false;
out:
	mutex_unlock(&ctx->lock);

	return 0;

}
EXPORT_SYMBOL(put_fb_to_queue);

void trans_vframe_to_user(struct aml_vcodec_ctx *ctx, struct vdec_v4l2_buffer *fb)
{
	struct aml_video_dec_buf *dstbuf = NULL;
	struct vframe_s *vf = (struct vframe_s *)fb->vf_handle;

	aml_v4l2_debug(3, "[%d] FROM (%s %s) vf: %p, ts: %llx, idx: %d",
		ctx->id, vf_get_provider(ctx->ada_ctx->recv_name)->name,
		ctx->ada_ctx->vfm_path != FRAME_BASE_PATH_V4L_VIDEO ? "OSD" : "VIDEO",
		vf, vf->timestamp, vf->index);

	aml_v4l2_debug(4, "[%d] FROM Y:(%lx, %u) C/U:(%lx, %u) V:(%lx, %u)",
		ctx->id, fb->m.mem[0].addr, fb->m.mem[0].size,
		fb->m.mem[1].addr, fb->m.mem[1].size,
		fb->m.mem[2].addr, fb->m.mem[2].size);

	dstbuf = container_of(fb, struct aml_video_dec_buf, frame_buffer);
	if (dstbuf->frame_buffer.num_planes == 1) {
		vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 0, fb->m.mem[0].bytes_used);
	} else if (dstbuf->frame_buffer.num_planes == 2) {
		vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 0, fb->m.mem[0].bytes_used);
		vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 1, fb->m.mem[1].bytes_used);
	}
	dstbuf->vb.vb2_buf.timestamp = vf->timestamp;
	dstbuf->ready_to_display = true;

	if (vf->flag & VFRAME_FLAG_EMPTY_FRAME_V4L) {
		dstbuf->lastframe = true;
		dstbuf->vb.flags = V4L2_BUF_FLAG_LAST;
		if (dstbuf->frame_buffer.num_planes == 1) {
			vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 0, 0);
		} else if (dstbuf->frame_buffer.num_planes == 2) {
			vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 0, 0);
			vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 1, 0);
		}
		ctx->has_receive_eos = true;
		pr_info("[%d] recevie a empty frame. idx: %d, state: %d\n",
			ctx->id, dstbuf->vb.vb2_buf.index,
			dstbuf->vb.vb2_buf.state);
	}

	aml_v4l2_debug(4, "[%d] receive vbuf idx: %d, state: %d",
		ctx->id, dstbuf->vb.vb2_buf.index,
		dstbuf->vb.vb2_buf.state);

	if (dstbuf->vb.vb2_buf.state == VB2_BUF_STATE_ACTIVE) {
		/* binding vframe handle. */
		vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		dstbuf->privdata.vf = *vf;

		v4l2_m2m_buf_done(&dstbuf->vb, VB2_BUF_STATE_DONE);
	}

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_FLUSHING &&
		ctx->has_receive_eos) {
		ctx->state = AML_STATE_FLUSHED;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_FLUSHED)",
			ctx->id, __func__);
	}
	mutex_unlock(&ctx->state_lock);

	if (dstbuf->lastframe &&
		ctx->q_data[AML_Q_DATA_SRC].resolution_changed) {

		/* make the run to stanby until new buffs to enque. */
		ctx->v4l_codec_dpb_ready = false;

		/*
		 * After all buffers containing decoded frames from
		 * before the resolution change point ready to be
		 * dequeued on the CAPTURE queue, the driver sends a
		 * V4L2_EVENT_SOURCE_CHANGE event for source change
		 * type V4L2_EVENT_SRC_CH_RESOLUTION, also the upper
		 * layer will get new information from cts->picinfo.
		 */
		aml_vdec_dispatch_event(ctx, V4L2_EVENT_SRC_CH_RESOLUTION);
	}

	ctx->decoded_frame_cnt++;
}

static int get_display_buffer(struct aml_vcodec_ctx *ctx, struct vdec_v4l2_buffer **out)
{
	int ret = -1;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);

	ret = vdec_if_get_param(ctx, GET_PARAM_DISP_FRAME_BUFFER, out);
	if (ret) {
		aml_v4l2_err("[%d] Cannot get param : GET_PARAM_DISP_FRAME_BUFFER", ctx->id);
		return -1;
	}

	if (!*out) {
		aml_v4l2_debug(4, "[%d] No display frame buffer", ctx->id);
		return -1;
	}

	return ret;
}

static void aml_check_dpb_ready(struct aml_vcodec_ctx *ctx)
{
	if (!ctx->v4l_codec_dpb_ready) {
		int buf_ready_num;

		/* is there enough dst bufs for decoding? */
		buf_ready_num = v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx);
		if ((ctx->dpb_size) &&
			((buf_ready_num + ctx->buf_used_count) >= ctx->dpb_size))
			ctx->v4l_codec_dpb_ready = true;
		aml_v4l2_debug(2, "[%d] %s() dpb: %d, ready: %d, used: %d, dpb is ready: %s",
				ctx->id, __func__, ctx->dpb_size,
				buf_ready_num, ctx->buf_used_count,
				ctx->v4l_codec_dpb_ready ? "yes" : "no");
	}
}

static int is_vdec_ready(struct aml_vcodec_ctx *ctx)
{
	struct aml_vcodec_dev *dev = ctx->dev;

	if (!is_input_ready(ctx->ada_ctx)) {
		pr_err("[%d] %s() the decoder intput has not ready.\n",
			ctx->id, __func__);
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		return 0;
	}

	if (ctx->state == AML_STATE_PROBE) {
		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_PROBE) {
			ctx->state = AML_STATE_READY;
			ctx->v4l_codec_ready = true;
			wake_up_interruptible(&ctx->wq);
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_READY)",
				ctx->id, __func__);
		}
		mutex_unlock(&ctx->state_lock);
	}

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_READY) {
		if (ctx->m2m_ctx->out_q_ctx.q.streaming &&
			ctx->m2m_ctx->cap_q_ctx.q.streaming) {
			ctx->state = AML_STATE_ACTIVE;
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ACTIVE)",
				ctx->id, __func__);
		}
	}
	mutex_unlock(&ctx->state_lock);

	/* check dpb ready */
	//aml_check_dpb_ready(ctx);

	return 1;
}

static bool is_enough_work_items(struct aml_vcodec_ctx *ctx)
{
	struct aml_vcodec_dev *dev = ctx->dev;

	if (vdec_frame_number(ctx->ada_ctx) >= WORK_ITEMS_MAX) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		return false;
	}

	return true;
}

static void aml_wait_dpb_ready(struct aml_vcodec_ctx *ctx)
{
	ulong expires;

	expires = jiffies + msecs_to_jiffies(1000);
	while (!ctx->v4l_codec_dpb_ready) {
		u32 ready_num = 0;

		if (time_after(jiffies, expires)) {
			pr_err("the DPB state has not ready.\n");
			break;
		}

		ready_num = v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx);
		if ((ready_num + ctx->buf_used_count) >= ctx->dpb_size)
			ctx->v4l_codec_dpb_ready = true;
	}
}

static void aml_vdec_worker(struct work_struct *work)
{
	struct aml_vcodec_ctx *ctx =
		container_of(work, struct aml_vcodec_ctx, decode_work);
	struct aml_vcodec_dev *dev = ctx->dev;
	struct vb2_buffer *src_buf;
	struct aml_vcodec_mem buf;
	bool res_chg = false;
	int ret;
	struct aml_video_dec_buf *src_buf_info;
	struct vb2_v4l2_buffer *src_vb2_v4l2;

	aml_v4l2_debug(4, "[%d] entry [%d] [%s]", ctx->id, __LINE__, __func__);

	if (ctx->state < AML_STATE_INIT ||
		ctx->state > AML_STATE_FLUSHED) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		goto out;
	}

	if (!is_vdec_ready(ctx)) {
		pr_err("[%d] %s() the decoder has not ready.\n",
			ctx->id, __func__);
		goto out;
	}

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_buf == NULL) {
		pr_err("[%d] src_buf empty!\n", ctx->id);
		goto out;
	}

	/*this case for google, but some frames are droped on ffmpeg, so disabled temp.*/
	if (0 && !is_enough_work_items(ctx))
		goto out;

	src_vb2_v4l2 = container_of(src_buf, struct vb2_v4l2_buffer, vb2_buf);
	src_buf_info = container_of(src_vb2_v4l2, struct aml_video_dec_buf, vb);

	if (src_buf_info->lastframe) {
		/*the empty data use to flushed the decoder.*/
		aml_v4l2_debug(2, "[%d] Got empty flush input buffer.", ctx->id);

		/*
		 * when inputs a small amount of src buff, then soon to
		 * switch state FLUSHING, must to wait the DBP to be ready.
		 */
		if (!ctx->v4l_codec_dpb_ready) {
			v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
			goto out;
		}

		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_ACTIVE) {
			ctx->state = AML_STATE_FLUSHING;// prepare flushing
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_FLUSHING-LASTFRM)",
				ctx->id, __func__);
		}
		mutex_unlock(&ctx->state_lock);

		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);

		/* sets eos data for vdec input. */
		aml_vdec_flush_decoder(ctx);

		goto out;
	}

	buf.vaddr = vb2_plane_vaddr(src_buf, 0);
	buf.dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);

	buf.size = src_buf->planes[0].bytesused;
	if (!buf.vaddr) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		aml_v4l2_err("[%d] id=%d src_addr is NULL!!",
				ctx->id, src_buf->index);
		goto out;
	}

	src_buf_info->used = true;

	/* pr_err("%s() [%d], size: 0x%zx, crc: 0x%x\n",
		__func__, __LINE__, buf.size, crc32(0, buf.va, buf.size));*/

	/* pts = (time / 10e6) * (90k / fps) */
	aml_v4l2_debug(4, "[%d] %s() timestamp: 0x%llx", ctx->id , __func__,
		src_buf->timestamp);

	ret = vdec_if_decode(ctx, &buf, src_buf->timestamp, &res_chg);
	if (ret > 0) {
		/*
		 * we only return src buffer with VB2_BUF_STATE_DONE
		 * when decode success without resolution change.
		 */
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_DONE);
	} else if (ret && ret != -EAGAIN) {
		src_buf_info->error = (ret == -EIO ? true : false);
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_ERROR);
		aml_v4l2_err("[%d] %s() error processing src data. %d.",
			ctx->id, __func__, ret);
	} else if (res_chg) {
		/* wait the DPB state to be ready. */
		aml_wait_dpb_ready(ctx);

		src_buf_info->used = false;
		aml_vdec_pic_info_update(ctx);
		/*
		 * On encountering a resolution change in the stream.
		 * The driver must first process and decode all
		 * remaining buffers from before the resolution change
		 * point, so call flush decode here
		 */
		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_ACTIVE) {
			ctx->state = AML_STATE_FLUSHING;// prepare flushing
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_FLUSHING-RESCHG)",
				ctx->id, __func__);
		}
		mutex_unlock(&ctx->state_lock);

		ctx->q_data[AML_Q_DATA_SRC].resolution_changed = true;
		v4l2_m2m_job_pause(dev->m2m_dev_dec, ctx->m2m_ctx);

		aml_vdec_flush_decoder(ctx);

		goto out;
	}

	v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
out:
	return;
}

static void aml_vdec_reset(struct aml_vcodec_ctx *ctx)
{
	if (ctx->state == AML_STATE_ABORT) {
		pr_err("[%d] %s() the decoder will be exited.\n",
			ctx->id, __func__);
		goto out;
	}

	if (aml_codec_reset(ctx->ada_ctx, &ctx->reset_flag)) {
		ctx->state = AML_STATE_ABORT;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
			ctx->id, __func__);
		goto out;
	}

	if (ctx->state ==  AML_STATE_RESET) {
		ctx->state = AML_STATE_PROBE;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_PROBE)",
			ctx->id, __func__);

		aml_v4l2_debug(0, "[%d] %s() dpb: %d, ready: %d, used: %d",
				ctx->id, __func__, ctx->dpb_size,
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx), ctx->buf_used_count);

		/* vdec has ready to decode subsequence data of new resolution. */
		ctx->q_data[AML_Q_DATA_SRC].resolution_changed = false;
		v4l2_m2m_job_resume(ctx->dev->m2m_dev_dec, ctx->m2m_ctx);
	}

out:
	complete(&ctx->comp);
	return;
}

void wait_vcodec_ending(struct aml_vcodec_ctx *ctx)
{
	struct aml_vcodec_dev *dev = ctx->dev;

	/* pause inject output data to vdec. */
	v4l2_m2m_job_pause(dev->m2m_dev_dec, ctx->m2m_ctx);

	/* flush worker. */
	flush_workqueue(dev->decode_workqueue);

	ctx->v4l_codec_ready = false;
	ctx->v4l_codec_dpb_ready = false;

	/* stop decoder. */
	if (ctx->state > AML_STATE_INIT)
		aml_vdec_reset(ctx);
}

void try_to_capture(struct aml_vcodec_ctx *ctx)
{
	int ret = 0;
	struct vdec_v4l2_buffer *fb = NULL;

	ret = get_display_buffer(ctx, &fb);
	if (ret) {
		aml_v4l2_debug(4, "[%d] %s() [%d], the que have no disp buf,ret: %d",
			ctx->id, __func__, __LINE__, ret);
		return;
	}

	trans_vframe_to_user(ctx, fb);
}
EXPORT_SYMBOL_GPL(try_to_capture);

static int vdec_thread(void *data)
{
	struct sched_param param =
		{.sched_priority = MAX_RT_PRIO / 2};
	struct aml_vdec_thread *thread =
		(struct aml_vdec_thread *) data;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *) thread->priv;

	sched_setscheduler(current, SCHED_FIFO, &param);

	for (;;) {
		aml_v4l2_debug(3, "[%d] %s() state: %d", ctx->id,
			__func__, ctx->state);

		if (down_interruptible(&thread->sem))
			break;

		if (thread->stop)
			break;

		/* handle event. */
		thread->func(ctx);
	}

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return 0;
}

void aml_thread_notify(struct aml_vcodec_ctx *ctx,
	enum aml_thread_type type)
{
	struct aml_vdec_thread *thread = NULL;

	list_for_each_entry(thread, &ctx->vdec_thread_list, node) {
		if (thread->task == NULL)
			continue;

		if (thread->type == type)
			up(&thread->sem);
	}
}
EXPORT_SYMBOL_GPL(aml_thread_notify);

int aml_thread_start(struct aml_vcodec_ctx *ctx, aml_thread_func func,
	enum aml_thread_type type, const char *thread_name)
{
	struct aml_vdec_thread *thread;
	int ret = 0;

	thread = kzalloc(sizeof(*thread), GFP_KERNEL);
	if (thread == NULL)
		return -ENOMEM;

	thread->type = type;
	thread->func = func;
	thread->priv = ctx;
	sema_init(&thread->sem, 0);

	thread->task = kthread_run(vdec_thread, thread, "aml-%s", thread_name);
	if (IS_ERR(thread->task)) {
		ret = PTR_ERR(thread->task);
		thread->task = NULL;
		goto err;
	}

	list_add(&thread->node, &ctx->vdec_thread_list);

	return 0;

err:
	kfree(thread);

	return ret;
}
EXPORT_SYMBOL_GPL(aml_thread_start);

void aml_thread_stop(struct aml_vcodec_ctx *ctx)
{
	struct aml_vdec_thread *thread = NULL;

	while (!list_empty(&ctx->vdec_thread_list)) {
		thread = list_entry(ctx->vdec_thread_list.next,
			struct aml_vdec_thread, node);
		list_del(&thread->node);
		thread->stop = true;
		up(&thread->sem);
		kthread_stop(thread->task);
		thread->task = NULL;
		kfree(thread);
	}
}
EXPORT_SYMBOL_GPL(aml_thread_stop);

static int vidioc_try_decoder_cmd(struct file *file, void *priv,
				struct v4l2_decoder_cmd *cmd)
{
	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
	case V4L2_DEC_CMD_START:
		if (cmd->flags != 0) {
			aml_v4l2_err("cmd->flags=%u", cmd->flags);
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vidioc_decoder_cmd(struct file *file, void *priv,
				struct v4l2_decoder_cmd *cmd)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *src_vq, *dst_vq;
	int ret;

	ret = vidioc_try_decoder_cmd(file, priv, cmd);
	if (ret)
		return ret;

	aml_v4l2_debug(2, "[%d] %s() [%d], cmd: %u",
		ctx->id, __func__, __LINE__, cmd->cmd);
	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
				V4L2_BUF_TYPE_VIDEO_CAPTURE);
	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
		if (ctx->state != AML_STATE_ACTIVE) {
			if (ctx->state >= AML_STATE_IDLE &&
				ctx->state <= AML_STATE_PROBE) {
				ctx->state = AML_STATE_ABORT;
				aml_vdec_dispatch_event(ctx, V4L2_EVENT_REQUEST_EXIT);
				aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
					ctx->id, __func__);
				return 0;
			}
		}

		src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (!vb2_is_streaming(src_vq)) {
			pr_err("[%d] Output stream is off. No need to flush.\n", ctx->id);
			return 0;
		}

		if (!vb2_is_streaming(dst_vq)) {
			pr_err("[%d] Capture stream is off. No need to flush.\n", ctx->id);
			return 0;
		}

		/* flush src */
		v4l2_m2m_buf_queue(ctx->m2m_ctx, &ctx->empty_flush_buf->vb);
		v4l2_m2m_try_schedule(ctx->m2m_ctx);//pay attention
		ctx->receive_cmd_stop = true;
		break;

	case V4L2_DEC_CMD_START:
		aml_v4l2_debug(4, "[%d] CMD V4L2_DEC_CMD_START ", ctx->id);
		vb2_clear_last_buffer_dequeued(dst_vq);//pay attention
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int vidioc_decoder_streamon(struct file *file, void *priv,
	enum v4l2_buf_type i)
{
	struct v4l2_fh *fh = file->private_data;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *q;

	q = v4l2_m2m_get_vq(fh->m2m_ctx, i);
	if (!V4L2_TYPE_IS_OUTPUT(q->type)) {
		if (ctx->is_stream_off) {
			mutex_lock(&ctx->state_lock);
			if ((ctx->state == AML_STATE_ACTIVE ||
				ctx->state == AML_STATE_FLUSHING ||
				ctx->state == AML_STATE_FLUSHED) ||
				(ctx->reset_flag == 2)) {
				ctx->state = AML_STATE_RESET;
				ctx->v4l_codec_ready = false;
				ctx->v4l_codec_dpb_ready = false;

				aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_RESET)",
					ctx->id, __func__);
				aml_vdec_reset(ctx);
			}
			mutex_unlock(&ctx->state_lock);

			ctx->is_stream_off = false;
		}
	}
	return v4l2_m2m_ioctl_streamon(file, priv, i);
}

static int vidioc_decoder_streamoff(struct file *file, void *priv,
	enum v4l2_buf_type i)
{
	struct v4l2_fh *fh = file->private_data;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *q;

	q = v4l2_m2m_get_vq(fh->m2m_ctx, i);
	if (!V4L2_TYPE_IS_OUTPUT(q->type)) {
		ctx->is_stream_off = true;
	}

	return v4l2_m2m_ioctl_streamoff(file, priv, i);
}

static int vidioc_decoder_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *rb)
{
	struct v4l2_fh *fh = file->private_data;
	struct vb2_queue *q;

	q = v4l2_m2m_get_vq(fh->m2m_ctx, rb->type);

	if (!rb->count)
		vb2_queue_release(q);

	return v4l2_m2m_ioctl_reqbufs(file, priv, rb);
}

void aml_vcodec_dec_release(struct aml_vcodec_ctx *ctx)
{
	ulong flags;

	flags = aml_vcodec_ctx_lock(ctx);
	ctx->state = AML_STATE_ABORT;
	aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
		ctx->id, __func__);
	aml_vcodec_ctx_unlock(ctx, flags);

	vdec_if_deinit(ctx);
}

void aml_vcodec_dec_set_default_params(struct aml_vcodec_ctx *ctx)
{
	struct aml_q_data *q_data;

	ctx->m2m_ctx->q_lock = &ctx->dev->dev_mutex;
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	INIT_WORK(&ctx->decode_work, aml_vdec_worker);
	ctx->colorspace = V4L2_COLORSPACE_REC709;
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;
	ctx->dev->dec_capability = 0;//VCODEC_CAPABILITY_4K_DISABLED;//disable 4k

	q_data = &ctx->q_data[AML_Q_DATA_SRC];
	memset(q_data, 0, sizeof(struct aml_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->fmt = &aml_video_formats[OUT_FMT_IDX];
	q_data->field = V4L2_FIELD_NONE;

	q_data->sizeimage[0] = (1024 * 1024);//DFT_CFG_WIDTH * DFT_CFG_HEIGHT; //1m
	q_data->bytesperline[0] = 0;

	q_data = &ctx->q_data[AML_Q_DATA_DST];
	memset(q_data, 0, sizeof(struct aml_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->coded_width = DFT_CFG_WIDTH;
	q_data->coded_height = DFT_CFG_HEIGHT;
	q_data->fmt = &aml_video_formats[CAP_FMT_IDX];
	q_data->field = V4L2_FIELD_NONE;

	v4l_bound_align_image(&q_data->coded_width,
				AML_VDEC_MIN_W,
				AML_VDEC_MAX_W, 4,
				&q_data->coded_height,
				AML_VDEC_MIN_H,
				AML_VDEC_MAX_H, 5, 6);

	q_data->sizeimage[0] = q_data->coded_width * q_data->coded_height;
	q_data->bytesperline[0] = q_data->coded_width;
	q_data->sizeimage[1] = q_data->sizeimage[0] / 2;
	q_data->bytesperline[1] = q_data->coded_width;

	ctx->state = AML_STATE_IDLE;
	aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_IDLE)",
		ctx->id, __func__);
}

static int vidioc_vdec_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *buf)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (ctx->state == AML_STATE_ABORT) {
		aml_v4l2_err("[%d] Call on QBUF after unrecoverable error, type = %s",
				ctx->id, V4L2_TYPE_IS_OUTPUT(buf->type) ?
				"OUT" : "IN");
		return -EIO;
	}

	return v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_vdec_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *buf)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;

	if (ctx->state == AML_STATE_ABORT) {
		aml_v4l2_err("[%d] Call on DQBUF after unrecoverable error, type = %s",
				ctx->id, V4L2_TYPE_IS_OUTPUT(buf->type) ?
				"OUT" : "IN");
		if (!V4L2_TYPE_IS_OUTPUT(buf->type))
			return -EIO;
	}

	ret = v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);

	if (!ret && !V4L2_TYPE_IS_OUTPUT(buf->type)) {
		struct vb2_queue *vq;
		struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
		struct aml_video_dec_buf *aml_buf = NULL;
		struct file *file = NULL;

		mutex_lock(&ctx->lock);
		vq = v4l2_m2m_get_vq(ctx->m2m_ctx, buf->type);
		vb2_v4l2 = to_vb2_v4l2_buffer(vq->bufs[buf->index]);
		aml_buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);

		file = fget(vb2_v4l2->private);
		if (is_v4l2_buf_file(file) &&
			!aml_buf->privdata.is_install) {
			dmabuf_fd_install_data(vb2_v4l2->private,
				(void*)&aml_buf->privdata,
				sizeof(struct file_privdata));
			aml_buf->privdata.is_install = true;
		}
		fput(file);
		mutex_unlock(&ctx->lock);
	}

	return ret;
}

static int vidioc_vdec_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	strlcpy(cap->driver, AML_VCODEC_DEC_NAME, sizeof(cap->driver));
	strlcpy(cap->bus_info, AML_PLATFORM_STR, sizeof(cap->bus_info));
	strlcpy(cap->card, AML_PLATFORM_STR, sizeof(cap->card));

	return 0;
}

static int vidioc_vdec_subscribe_evt(struct v4l2_fh *fh,
				     const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

static int vidioc_try_fmt(struct v4l2_format *f, struct aml_video_fmt *fmt)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	int i;

	pix_fmt_mp->field = V4L2_FIELD_NONE;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pix_fmt_mp->num_planes = 1;
		pix_fmt_mp->plane_fmt[0].bytesperline = 0;
	} else if (!V4L2_TYPE_IS_OUTPUT(f->type)) {
		int tmp_w, tmp_h;

		pix_fmt_mp->height = clamp(pix_fmt_mp->height,
					AML_VDEC_MIN_H,
					AML_VDEC_MAX_H);
		pix_fmt_mp->width = clamp(pix_fmt_mp->width,
					AML_VDEC_MIN_W,
					AML_VDEC_MAX_W);

		/*
		 * Find next closer width align 64, heign align 64, size align
		 * 64 rectangle
		 * Note: This only get default value, the real HW needed value
		 *       only available when ctx in AML_STATE_PROBE state
		 */
		tmp_w = pix_fmt_mp->width;
		tmp_h = pix_fmt_mp->height;
		v4l_bound_align_image(&pix_fmt_mp->width,
					AML_VDEC_MIN_W,
					AML_VDEC_MAX_W, 6,
					&pix_fmt_mp->height,
					AML_VDEC_MIN_H,
					AML_VDEC_MAX_H, 6, 9);

		if (pix_fmt_mp->width < tmp_w &&
			(pix_fmt_mp->width + 64) <= AML_VDEC_MAX_W)
			pix_fmt_mp->width += 64;
		if (pix_fmt_mp->height < tmp_h &&
			(pix_fmt_mp->height + 64) <= AML_VDEC_MAX_H)
			pix_fmt_mp->height += 64;

		aml_v4l2_debug(4,
			"before resize width=%d, height=%d, after resize width=%d, height=%d, sizeimage=%d",
			tmp_w, tmp_h, pix_fmt_mp->width,
			pix_fmt_mp->height,
			pix_fmt_mp->width * pix_fmt_mp->height);

		pix_fmt_mp->num_planes = fmt->num_planes;
		pix_fmt_mp->plane_fmt[0].sizeimage =
				pix_fmt_mp->width * pix_fmt_mp->height;
		pix_fmt_mp->plane_fmt[0].bytesperline = pix_fmt_mp->width;

		if (pix_fmt_mp->num_planes == 2) {
			pix_fmt_mp->plane_fmt[1].sizeimage =
				(pix_fmt_mp->width * pix_fmt_mp->height) / 2;
			pix_fmt_mp->plane_fmt[1].bytesperline =
				pix_fmt_mp->width;
		}
	}

	for (i = 0; i < pix_fmt_mp->num_planes; i++)
		memset(&(pix_fmt_mp->plane_fmt[i].reserved[0]), 0x0,
			   sizeof(pix_fmt_mp->plane_fmt[0].reserved));

	pix_fmt_mp->flags = 0;
	memset(&pix_fmt_mp->reserved, 0x0, sizeof(pix_fmt_mp->reserved));
	return 0;
}

static int vidioc_try_fmt_vid_cap_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct aml_video_fmt *fmt = NULL;

	fmt = aml_vdec_find_format(f);
	if (!fmt)
		return -EINVAL;

	return vidioc_try_fmt(f, fmt);
}

static int vidioc_try_fmt_vid_out_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	struct aml_video_fmt *fmt = NULL;

	fmt = aml_vdec_find_format(f);
	if (!fmt)
		return -EINVAL;

	if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
		aml_v4l2_err("sizeimage of output format must be given");
		return -EINVAL;
	}

	return vidioc_try_fmt(f, fmt);
}

static int vidioc_vdec_g_selection(struct file *file, void *priv,
			struct v4l2_selection *s)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct aml_q_data *q_data;

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	q_data = &ctx->q_data[AML_Q_DATA_DST];

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.visible_width;
		s->r.height = ctx->picinfo.visible_height;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.coded_width;
		s->r.height = ctx->picinfo.coded_height;
		break;
	case V4L2_SEL_TGT_COMPOSE:
		if (vdec_if_get_param(ctx, GET_PARAM_CROP_INFO, &(s->r))) {
			/* set to default value if header info not ready yet*/
			s->r.left = 0;
			s->r.top = 0;
			s->r.width = q_data->visible_width;
			s->r.height = q_data->visible_height;
		}
		break;
	default:
		return -EINVAL;
	}

	if (ctx->state < AML_STATE_PROBE) {
		/* set to default value if header info not ready yet*/
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = q_data->visible_width;
		s->r.height = q_data->visible_height;
		return 0;
	}

	return 0;
}

static int vidioc_vdec_s_selection(struct file *file, void *priv,
				struct v4l2_selection *s)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.visible_width;
		s->r.height = ctx->picinfo.visible_height;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vidioc_vdec_s_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp;
	struct aml_q_data *q_data;
	int ret = 0;
	struct aml_video_fmt *fmt;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);

	q_data = aml_vdec_get_q_data(ctx, f->type);
	if (!q_data)
		return -EINVAL;

	pix_mp = &f->fmt.pix_mp;
	if ((f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
	    vb2_is_busy(&ctx->m2m_ctx->out_q_ctx.q)) {
		aml_v4l2_err("[%d] out_q_ctx buffers already requested", ctx->id);
	}

	if ((!V4L2_TYPE_IS_OUTPUT(f->type)) &&
	    vb2_is_busy(&ctx->m2m_ctx->cap_q_ctx.q)) {
		aml_v4l2_err("[%d] cap_q_ctx buffers already requested", ctx->id);
	}

	fmt = aml_vdec_find_format(f);
	if (fmt == NULL) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			f->fmt.pix.pixelformat =
				aml_video_formats[OUT_FMT_IDX].fourcc;
			fmt = aml_vdec_find_format(f);
		} else if (!V4L2_TYPE_IS_OUTPUT(f->type)) {
			f->fmt.pix.pixelformat =
				aml_video_formats[CAP_FMT_IDX].fourcc;
			fmt = aml_vdec_find_format(f);
		}
	}

	q_data->fmt = fmt;
	vidioc_try_fmt(f, q_data->fmt);
	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q_data->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
		q_data->coded_width = pix_mp->width;
		q_data->coded_height = pix_mp->height;

		aml_v4l2_debug(4, "[%d] %s() [%d], w: %d, h: %d, size: %d",
			ctx->id, __func__, __LINE__, pix_mp->width, pix_mp->height,
			pix_mp->plane_fmt[0].sizeimage);

		ctx->colorspace = f->fmt.pix_mp.colorspace;
		ctx->ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
		ctx->quantization = f->fmt.pix_mp.quantization;
		ctx->xfer_func = f->fmt.pix_mp.xfer_func;

		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_IDLE) {
			ret = vdec_if_init(ctx, q_data->fmt->fourcc);
			if (ret) {
				aml_v4l2_err("[%d]: vdec_if_init() fail ret=%d",
					ctx->id, ret);
				mutex_unlock(&ctx->state_lock);
				return -EINVAL;
			}
			ctx->state = AML_STATE_INIT;
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_INIT)",
				ctx->id, __func__);
		}
		mutex_unlock(&ctx->state_lock);
	}

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *priv,
				struct v4l2_frmsizeenum *fsize)
{
	int i = 0;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (fsize->index != 0)
		return -EINVAL;

	for (i = 0; i < NUM_SUPPORTED_FRAMESIZE; ++i) {
		if (fsize->pixel_format != aml_vdec_framesizes[i].fourcc)
			continue;

		fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise = aml_vdec_framesizes[i].stepwise;
		if (!(ctx->dev->dec_capability &
				VCODEC_CAPABILITY_4K_DISABLED)) {
			aml_v4l2_debug(4, "[%d] 4K is enabled", ctx->id);
			fsize->stepwise.max_width =
					VCODEC_DEC_4K_CODED_WIDTH;
			fsize->stepwise.max_height =
					VCODEC_DEC_4K_CODED_HEIGHT;
		}
		aml_v4l2_debug(4, "[%d] %x, %d %d %d %d %d %d",
				ctx->id, ctx->dev->dec_capability,
				fsize->stepwise.min_width,
				fsize->stepwise.max_width,
				fsize->stepwise.step_width,
				fsize->stepwise.min_height,
				fsize->stepwise.max_height,
				fsize->stepwise.step_height);
		return 0;
	}

	return -EINVAL;
}

static int vidioc_enum_fmt(struct v4l2_fmtdesc *f, bool output_queue)
{
	struct aml_video_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (output_queue && (aml_video_formats[i].type != AML_FMT_DEC))
			continue;
		if (!output_queue && (aml_video_formats[i].type != AML_FMT_FRAME))
			continue;

		if (j == f->index) {
			fmt = &aml_video_formats[i];
			f->pixelformat = fmt->fourcc;
			return 0;
		}
		++j;
	}

	return -EINVAL;
}

static int vidioc_vdec_enum_fmt_vid_cap_mplane(struct file *file, void *pirv,
					       struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false);
}

static int vidioc_vdec_enum_fmt_vid_out_mplane(struct file *file, void *priv,
					       struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true);
}

static int vidioc_vdec_g_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct vb2_queue *vq;
	struct aml_q_data *q_data;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		aml_v4l2_err("[%d] no vb2 queue for type=%d", ctx->id, f->type);
		return -EINVAL;
	}

	q_data = aml_vdec_get_q_data(ctx, f->type);

	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->colorspace = ctx->colorspace;
	pix_mp->ycbcr_enc = ctx->ycbcr_enc;
	pix_mp->quantization = ctx->quantization;
	pix_mp->xfer_func = ctx->xfer_func;

	if ((!V4L2_TYPE_IS_OUTPUT(f->type)) &&
	    (ctx->state >= AML_STATE_PROBE)) {
		/* Until STREAMOFF is called on the CAPTURE queue
		 * (acknowledging the event), the driver operates as if
		 * the resolution hasn't changed yet.
		 * So we just return picinfo yet, and update picinfo in
		 * stop_streaming hook function
		 */
		/* it is used for alloc the decode buffer size. */
		q_data->sizeimage[0] = ctx->picinfo.y_len_sz;
		q_data->sizeimage[1] = ctx->picinfo.c_len_sz;

		/* it is used for alloc the EGL image buffer size. */
		q_data->coded_width = ctx->picinfo.coded_width;
		q_data->coded_height = ctx->picinfo.coded_height;

		q_data->bytesperline[0] = ctx->picinfo.coded_width;
		q_data->bytesperline[1] = ctx->picinfo.coded_width;

		/*
		 * Width and height are set to the dimensions
		 * of the movie, the buffer is bigger and
		 * further processing stages should crop to this
		 * rectangle.
		 */
		pix_mp->width = q_data->coded_width;
		pix_mp->height = q_data->coded_height;

		/*
		 * Set pixelformat to the format in which mt vcodec
		 * outputs the decoded frame
		 */
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->plane_fmt[1].bytesperline = q_data->bytesperline[1];
		pix_mp->plane_fmt[1].sizeimage = q_data->sizeimage[1];
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/*
		 * This is run on OUTPUT
		 * The buffer contains compressed image
		 * so width and height have no meaning.
		 * Assign value here to pass v4l2-compliance test
		 */
		pix_mp->width = q_data->visible_width;
		pix_mp->height = q_data->visible_height;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->num_planes = q_data->fmt->num_planes;
	} else {
		pix_mp->width = q_data->coded_width;
		pix_mp->height = q_data->coded_height;
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->plane_fmt[1].bytesperline = q_data->bytesperline[1];
		pix_mp->plane_fmt[1].sizeimage = q_data->sizeimage[1];

		aml_v4l2_debug(4, "[%d] type=%d state=%d Format information could not be read, not ready yet!",
				ctx->id, f->type, ctx->state);
		return -EINVAL;
	}

	return 0;
}

/*int vidioc_vdec_g_ctrl(struct file *file, void *fh,
	struct v4l2_control *a)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);

	if (a->id == V4L2_CID_MIN_BUFFERS_FOR_CAPTURE)
		a->value = 20;

	return 0;
}*/

static int vb2ops_vdec_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers,
				unsigned int *nplanes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vq);
	struct aml_q_data *q_data;
	unsigned int i;

	q_data = aml_vdec_get_q_data(ctx, vq->type);

	if (q_data == NULL) {
		aml_v4l2_err("[%d] vq->type=%d err", ctx->id, vq->type);
		return -EINVAL;
	}

	if (*nplanes) {
		for (i = 0; i < *nplanes; i++) {
			if (sizes[i] < q_data->sizeimage[i])
				return -EINVAL;
			//alloc_devs[i] = &ctx->dev->plat_dev->dev;
			alloc_devs[i] = v4l_get_dev_from_codec_mm();//alloc mm from the codec mm
		}
	} else {
		if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			*nplanes = 2;
		else
			*nplanes = 1;

		for (i = 0; i < *nplanes; i++) {
			sizes[i] = q_data->sizeimage[i];
			//alloc_devs[i] = &ctx->dev->plat_dev->dev;
			alloc_devs[i] = v4l_get_dev_from_codec_mm();//alloc mm from the codec mm
		}
	}

	pr_info("[%d] type: %d, plane: %d, buf cnt: %d, size: [Y: %u, C: %u]\n",
		ctx->id, vq->type, *nplanes, *nbuffers, sizes[0], sizes[1]);

	return 0;
}

static int vb2ops_vdec_buf_prepare(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct aml_q_data *q_data;
	int i;

	aml_v4l2_debug(4, "[%d] (%d) id=%d",
			ctx->id, vb->vb2_queue->type, vb->index);

	q_data = aml_vdec_get_q_data(ctx, vb->vb2_queue->type);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) < q_data->sizeimage[i]) {
			aml_v4l2_err("[%d] data will not fit into plane %d (%lu < %d)",
				ctx->id, i, vb2_plane_size(vb, i),
				q_data->sizeimage[i]);
		}
	}

	return 0;
}

static void vb2ops_vdec_buf_queue(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_video_dec_buf *buf = NULL;
	struct aml_vcodec_mem src_mem;
	unsigned int dpb = 0;

	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);

	aml_v4l2_debug(3, "[%d] %s(), vb: %p, type: %d, idx: %d, state: %d, used: %d",
		ctx->id, __func__, vb, vb->vb2_queue->type,
		vb->index, vb->state, buf->used);
	/*
	 * check if this buffer is ready to be used after decode
	 */
	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		aml_v4l2_debug(3, "[%d] %s() [%d], y_addr: %lx, vf_h: %lx, state: %d", ctx->id,
			__func__, __LINE__, buf->frame_buffer.m.mem[0].addr,
			buf->frame_buffer.vf_handle, buf->frame_buffer.status);

		if (!buf->que_in_m2m) {
			aml_v4l2_debug(2, "[%d] enque capture buf idx %d, %p",
				ctx->id, vb->index, vb);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, vb2_v4l2);
			buf->que_in_m2m = true;
			buf->queued_in_vb2 = true;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;

			/* check dpb ready */
			aml_check_dpb_ready(ctx);
		} else if (buf->frame_buffer.status == FB_ST_DISPLAY) {
			buf->queued_in_vb2 = false;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;

			/* recycle vf */
			video_vf_put(ctx->ada_ctx->recv_name,
				&buf->frame_buffer, ctx->id);
		}
		return;
	}

	v4l2_m2m_buf_queue(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));

	if (ctx->state != AML_STATE_INIT) {
		aml_v4l2_debug(4, "[%d] already init driver %d",
				ctx->id, ctx->state);
		return;
	}

	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
	if (buf->lastframe) {
		/* This shouldn't happen. Just in case. */
		aml_v4l2_err("[%d] Invalid flush buffer.", ctx->id);
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		return;
	}

	src_mem.vaddr = vb2_plane_vaddr(vb, 0);
	src_mem.dma_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	src_mem.size = vb->planes[0].bytesused;
	if (vdec_if_probe(ctx, &src_mem, NULL)) {
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_DONE);
		return;
	}

	if (vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->picinfo)) {
		pr_err("[%d] GET_PARAM_PICTURE_INFO err\n", ctx->id);
		return;
	}

	if (vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpb)) {
		pr_err("[%d] GET_PARAM_DPB_SIZE err\n", ctx->id);
		return;
	}

	if (!dpb)
		return;

	ctx->dpb_size = dpb;
	ctx->last_decoded_picinfo = ctx->picinfo;
	aml_vdec_dispatch_event(ctx, V4L2_EVENT_SRC_CH_RESOLUTION);

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_INIT) {
		ctx->state = AML_STATE_PROBE;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_PROBE)",
			ctx->id, __func__);
	}
	mutex_unlock(&ctx->state_lock);
}

static void vb2ops_vdec_buf_finish(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_video_dec_buf *buf = NULL;
	bool buf_error;

	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		buf->queued_in_v4l2 = false;
		buf->queued_in_vb2 = false;
	}
	buf_error = buf->error;

	if (buf_error) {
		aml_v4l2_err("[%d] Unrecoverable error on buffer.", ctx->id);
		ctx->state = AML_STATE_ABORT;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
			ctx->id, __func__);
	}
}

static int vb2ops_vdec_buf_init(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
					struct vb2_v4l2_buffer, vb2_buf);
	struct aml_video_dec_buf *buf = container_of(vb2_v4l2,
					struct aml_video_dec_buf, vb);
	unsigned int size, phy_addr = 0;
	char *owner = __getname();

	aml_v4l2_debug(4, "[%d] (%d) id=%d",
		ctx->id, vb->vb2_queue->type, vb->index);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		buf->used = false;
		buf->ready_to_display = false;
		buf->queued_in_v4l2 = false;
		buf->frame_buffer.status = FB_ST_NORMAL;
	} else {
		buf->lastframe = false;
	}

	/* codec_mm buffers count */
	if (V4L2_TYPE_IS_OUTPUT(vb->type)) {
		size = vb->planes[0].length;
		phy_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
		snprintf(owner, PATH_MAX, "%s-%d", "v4l-input", ctx->id);
		strncpy(buf->mem_onwer, owner, sizeof(buf->mem_onwer));
		buf->mem_onwer[sizeof(buf->mem_onwer) - 1] = '\0';

		buf->mem[0] = v4l_reqbufs_from_codec_mm(buf->mem_onwer,
				phy_addr, size, vb->index);
		aml_v4l2_debug(3, "[%d] IN alloc, addr: %x, size: %u, idx: %u",
			ctx->id, phy_addr, size, vb->index);
	} else {
		snprintf(owner, PATH_MAX, "%s-%d", "v4l-output", ctx->id);
		strncpy(buf->mem_onwer, owner, sizeof(buf->mem_onwer));
		buf->mem_onwer[sizeof(buf->mem_onwer) - 1] = '\0';

		if ((vb->memory == VB2_MEMORY_MMAP) && (vb->num_planes == 1)) {
			size = vb->planes[0].length;
			phy_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
			buf->mem[0] = v4l_reqbufs_from_codec_mm(buf->mem_onwer,
				phy_addr, size, vb->index);
			aml_v4l2_debug(3, "[%d] OUT Y alloc, addr: %x, size: %u, idx: %u",
				ctx->id, phy_addr, size, vb->index);
		} else if ((vb->memory == VB2_MEMORY_MMAP) && (vb->num_planes == 2)) {
			size = vb->planes[0].length;
			phy_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
			buf->mem[0] = v4l_reqbufs_from_codec_mm(buf->mem_onwer,
				phy_addr, size, vb->index);
			aml_v4l2_debug(3, "[%d] OUT Y alloc, addr: %x, size: %u, idx: %u",
				ctx->id, phy_addr, size, vb->index);

			size = vb->planes[1].length;
			phy_addr = vb2_dma_contig_plane_dma_addr(vb, 1);
			buf->mem[1] = v4l_reqbufs_from_codec_mm(buf->mem_onwer,
					phy_addr, size, vb->index);
			aml_v4l2_debug(3, "[%d] OUT C alloc, addr: %x, size: %u, idx: %u",
				ctx->id, phy_addr, size, vb->index);
		}
	}

	__putname(owner);

	return 0;
}

static void codec_mm_bufs_cnt_clean(struct vb2_queue *q)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_video_dec_buf *buf = NULL;
	int i;

	for (i = 0; i < q->num_buffers; ++i) {
		vb2_v4l2 = to_vb2_v4l2_buffer(q->bufs[i]);
		buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
		if (IS_ERR_OR_NULL(buf->mem[0]))
			return;

		if (V4L2_TYPE_IS_OUTPUT(q->bufs[i]->type)) {
			v4l_freebufs_back_to_codec_mm(buf->mem_onwer, buf->mem[0]);

			aml_v4l2_debug(3, "[%d] IN clean, addr: %lx, size: %u, idx: %u",
				ctx->id, buf->mem[0]->phy_addr, buf->mem[0]->buffer_size, i);
			buf->mem[0] = NULL;
			continue;
		}

		if (q->memory == VB2_MEMORY_MMAP) {
			v4l_freebufs_back_to_codec_mm(buf->mem_onwer, buf->mem[0]);
			v4l_freebufs_back_to_codec_mm(buf->mem_onwer, buf->mem[1]);

			aml_v4l2_debug(3, "[%d] OUT Y clean, addr: %lx, size: %u, idx: %u",
				ctx->id, buf->mem[0]->phy_addr, buf->mem[0]->buffer_size, i);
			aml_v4l2_debug(3, "[%d] OUT C clean, addr: %lx, size: %u, idx: %u",
				ctx->id, buf->mem[1]->phy_addr, buf->mem[1]->buffer_size, i);
			buf->mem[0] = NULL;
			buf->mem[1] = NULL;
		}
	}
}

static int vb2ops_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(q);

	ctx->has_receive_eos = false;
	v4l2_m2m_set_dst_buffered(ctx->fh.m2m_ctx, true);

	return 0;
}

static void vb2ops_vdec_stop_streaming(struct vb2_queue *q)
{
	struct aml_video_dec_buf *buf = NULL;
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	int i;

	aml_v4l2_debug(3, "[%d] (%d) state=(%x) frame_cnt=%d",
			ctx->id, q->type, ctx->state, ctx->decoded_frame_cnt);

	codec_mm_bufs_cnt_clean(q);

	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		while ((vb2_v4l2 = v4l2_m2m_src_buf_remove(ctx->m2m_ctx)))
			v4l2_m2m_buf_done(vb2_v4l2, VB2_BUF_STATE_ERROR);
	} else {
		for (i = 0; i < q->num_buffers; ++i) {
			vb2_v4l2 = to_vb2_v4l2_buffer(q->bufs[i]);
			buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
			buf->frame_buffer.status = FB_ST_NORMAL;
			buf->que_in_m2m = false;
			buf->privdata.is_install = false;

			if (vb2_v4l2->vb2_buf.state == VB2_BUF_STATE_ACTIVE)
				v4l2_m2m_buf_done(vb2_v4l2, VB2_BUF_STATE_ERROR);

			/*pr_info("idx: %d, state: %d\n",
				q->bufs[i]->index, q->bufs[i]->state);*/
		}
	}
	ctx->buf_used_count = 0;
}

static void m2mops_vdec_device_run(void *priv)
{
	struct aml_vcodec_ctx *ctx = priv;
	struct aml_vcodec_dev *dev = ctx->dev;

	aml_v4l2_debug(4, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);

	queue_work(dev->decode_workqueue, &ctx->decode_work);
}

void vdec_device_vf_run(struct aml_vcodec_ctx *ctx)
{
	aml_v4l2_debug(3, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);

	if (ctx->state < AML_STATE_INIT ||
		ctx->state > AML_STATE_FLUSHED)
		return;

	aml_thread_notify(ctx, AML_THREAD_CAPTURE);
}

static int m2mops_vdec_job_ready(void *m2m_priv)
{
	struct aml_vcodec_ctx *ctx = m2m_priv;

	aml_v4l2_debug(4, "[%d] %s(), state: %d",  ctx->id,
		__func__, ctx->state);

	if (ctx->state < AML_STATE_PROBE ||
		ctx->state > AML_STATE_FLUSHED)
		return 0;

	return 1;
}

static void m2mops_vdec_job_abort(void *priv)
{
	struct aml_vcodec_ctx *ctx = priv;

	aml_v4l2_debug(3, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);
}

static int aml_vdec_g_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct aml_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret = 0;

	aml_v4l2_debug(4, "%s() [%d]", __func__, __LINE__);

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= AML_STATE_PROBE) {
			ctrl->val = ctx->dpb_size;
		} else {
			pr_err("Seqinfo not ready.\n");
			ctrl->val = 0;
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int aml_vdec_try_s_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct aml_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);

	aml_v4l2_debug(4, "%s() [%d]", __func__, __LINE__);

	if (ctrl->id == AML_V4L2_SET_DECMODE) {
		ctx->is_drm_mode = ctrl->val;
		pr_info("set stream mode: %x\n", ctrl->val);
	}

	return 0;
}

static const struct v4l2_ctrl_ops aml_vcodec_dec_ctrl_ops = {
	.g_volatile_ctrl = aml_vdec_g_v_ctrl,
	.try_ctrl = aml_vdec_try_s_v_ctrl,
};

static const struct v4l2_ctrl_config ctrl_st_mode = {
	.name	= "stream mode",
	.id	= AML_V4L2_SET_DECMODE,
	.ops	= &aml_vcodec_dec_ctrl_ops,
	.type	= V4L2_CTRL_TYPE_BOOLEAN,
	.flags	= V4L2_CTRL_FLAG_WRITE_ONLY,
	.min	= 0,
	.max	= 1,
	.step	= 1,
	.def	= 0,
};

int aml_vcodec_dec_ctrls_setup(struct aml_vcodec_ctx *ctx)
{
	int ret;
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, 1);
	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&aml_vcodec_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				0, 32, 1, 1);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	if (ctx->ctrl_hdl.error) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &ctrl_st_mode, NULL);
	if (ctx->ctrl_hdl.error) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);

	return 0;
err:
	aml_v4l2_err("[%d] Adding control failed %d",
			ctx->id, ctx->ctrl_hdl.error);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
	return ret;
}

static int vidioc_vdec_g_parm(struct file *file, void *fh,
		     struct v4l2_streamparm *a)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);

	if (a->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		if (vdec_if_get_param(ctx, GET_PARAM_CONFIG_INFO,
			&ctx->config.parm.dec)) {
			pr_err("[%d] GET_PARAM_CONFIG_INFO err\n", ctx->id);
			return -1;
		}
		memcpy(a->parm.raw_data, ctx->config.parm.data,
			sizeof(a->parm.raw_data));
	}

	return 0;
}

static int vidioc_vdec_s_parm(struct file *file, void *fh,
		     struct v4l2_streamparm *a)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);

	if (a->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		struct aml_dec_params *in =
			(struct aml_dec_params *) a->parm.raw_data;
		struct aml_dec_params *dec = &ctx->config.parm.dec;

		ctx->config.type = V4L2_CONFIG_PARM_DECODE;

		if (in->parms_status & V4L2_CONFIG_PARM_DECODE_CFGINFO)
			dec->cfg = in->cfg;
		if (in->parms_status & V4L2_CONFIG_PARM_DECODE_PSINFO)
			dec->ps = in->ps;
		if (in->parms_status & V4L2_CONFIG_PARM_DECODE_HDRINFO)
			dec->hdr = in->hdr;
		if (in->parms_status & V4L2_CONFIG_PARM_DECODE_CNTINFO)
			dec->cnt = in->cnt;

		dec->parms_status |= in->parms_status;
	}

	return 0;
}

static void m2mops_vdec_lock(void *m2m_priv)
{
	struct aml_vcodec_ctx *ctx = m2m_priv;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);
	mutex_lock(&ctx->dev->dev_mutex);
}

static void m2mops_vdec_unlock(void *m2m_priv)
{
	struct aml_vcodec_ctx *ctx = m2m_priv;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);
	mutex_unlock(&ctx->dev->dev_mutex);
}

const struct v4l2_m2m_ops aml_vdec_m2m_ops = {
	.device_run	= m2mops_vdec_device_run,
	.job_ready	= m2mops_vdec_job_ready,
	.job_abort	= m2mops_vdec_job_abort,
	.lock		= m2mops_vdec_lock,
	.unlock		= m2mops_vdec_unlock,
};

static const struct vb2_ops aml_vdec_vb2_ops = {
	.queue_setup	= vb2ops_vdec_queue_setup,
	.buf_prepare	= vb2ops_vdec_buf_prepare,
	.buf_queue	= vb2ops_vdec_buf_queue,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
	.buf_init	= vb2ops_vdec_buf_init,
	.buf_finish	= vb2ops_vdec_buf_finish,
	.start_streaming = vb2ops_vdec_start_streaming,
	.stop_streaming	= vb2ops_vdec_stop_streaming,
};

const struct v4l2_ioctl_ops aml_vdec_ioctl_ops = {
	.vidioc_streamon		= vidioc_decoder_streamon,
	.vidioc_streamoff		= vidioc_decoder_streamoff,
	.vidioc_reqbufs			= vidioc_decoder_reqbufs,
	.vidioc_querybuf		= v4l2_m2m_ioctl_querybuf,
	.vidioc_expbuf			= v4l2_m2m_ioctl_expbuf,//??
	//.vidioc_g_ctrl		= vidioc_vdec_g_ctrl,

	.vidioc_qbuf			= vidioc_vdec_qbuf,
	.vidioc_dqbuf			= vidioc_vdec_dqbuf,

	.vidioc_try_fmt_vid_cap_mplane	= vidioc_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_cap		= vidioc_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane	= vidioc_try_fmt_vid_out_mplane,
	.vidioc_try_fmt_vid_out		= vidioc_try_fmt_vid_out_mplane,

	.vidioc_s_fmt_vid_cap_mplane	= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_cap		= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out		= vidioc_vdec_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane	= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_cap		= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out		= vidioc_vdec_g_fmt,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,

	.vidioc_enum_fmt_vid_cap_mplane	= vidioc_vdec_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_cap	= vidioc_vdec_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out_mplane	= vidioc_vdec_enum_fmt_vid_out_mplane,
	.vidioc_enum_fmt_vid_out	= vidioc_vdec_enum_fmt_vid_out_mplane,
	.vidioc_enum_framesizes		= vidioc_enum_framesizes,

	.vidioc_querycap		= vidioc_vdec_querycap,
	.vidioc_subscribe_event		= vidioc_vdec_subscribe_evt,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
	.vidioc_g_selection             = vidioc_vdec_g_selection,
	.vidioc_s_selection             = vidioc_vdec_s_selection,

	.vidioc_decoder_cmd		= vidioc_decoder_cmd,
	.vidioc_try_decoder_cmd		= vidioc_try_decoder_cmd,

	.vidioc_g_parm			= vidioc_vdec_g_parm,
	.vidioc_s_parm			= vidioc_vdec_s_parm,
};

int aml_vcodec_dec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq)
{
	struct aml_vcodec_ctx *ctx = priv;
	int ret = 0;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);

	src_vq->type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes	= VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv	= ctx;
	src_vq->buf_struct_size = sizeof(struct aml_video_dec_buf);
	src_vq->ops		= &aml_vdec_vb2_ops;
	src_vq->mem_ops		= &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock		= &ctx->dev->dev_mutex;
	ret = vb2_queue_init(src_vq);
	if (ret) {
		aml_v4l2_err("[%d] Failed to initialize videobuf2 queue(output)", ctx->id);
		return ret;
	}

	dst_vq->type		= multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
					V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes	= VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	dst_vq->drv_priv	= ctx;
	dst_vq->buf_struct_size = sizeof(struct aml_video_dec_buf);
	dst_vq->ops		= &aml_vdec_vb2_ops;
	dst_vq->mem_ops		= &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock		= &ctx->dev->dev_mutex;
	ret = vb2_queue_init(dst_vq);
	if (ret) {
		vb2_queue_release(src_vq);
		aml_v4l2_err("[%d] Failed to initialize videobuf2 queue(capture)", ctx->id);
	}

	return ret;
}
