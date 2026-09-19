/*
 * Copyright © 2022 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <xf86drm.h>

#include "drm-uapi/pvr_drm.h"
#include "pvr_drm.h"
#include "pvr_drm_job_null.h"
#include "pvr_winsys.h"
#include "util/libsync.h"
#include "vk_alloc.h"
#include "vk_drm_syncobj.h"
#include "vk_log.h"
#include "vk_sync.h"
#include "vk_util.h"

VkResult pvr_drm_winsys_null_job_submit(struct pvr_winsys *ws,
                                        struct vk_sync_wait *waits,
                                        uint32_t wait_count,
                                        struct vk_sync_signal *signal_sync)
{
   struct pvr_drm_winsys *drm_ws = to_pvr_drm_winsys(ws);
   const int render_fd = drm_ws->base.render_fd;
   VkResult result;
   int ret;

   if (wait_count == 0) {
      uint32_t dst_syncobj =
         vk_sync_as_drm_syncobj(signal_sync->sync)->syncobj;

      if (signal_sync->signal_value) {
         ret = drmSyncobjTimelineSignal(render_fd,
                                        &dst_syncobj,
                                        &signal_sync->signal_value,
                                        1);
      } else {
         ret = drmSyncobjSignal(render_fd, &dst_syncobj, 1);
      }

      if (ret) {
         return vk_errorf(NULL,
                          VK_ERROR_OUT_OF_DEVICE_MEMORY,
                          "Failed to signal null job syncobj. Errno: %d - %s.",
                          errno,
                          strerror(errno));
      }

      return VK_SUCCESS;
   }

   if (wait_count == 1) {
      struct vk_sync *src_sync = waits[0].sync;
      struct vk_sync *dst_sync = signal_sync->sync;

      ret = drmSyncobjTransfer(render_fd,
                               vk_sync_as_drm_syncobj(dst_sync)->syncobj,
                               signal_sync->signal_value,
                               vk_sync_as_drm_syncobj(src_sync)->syncobj,
                               waits[0].wait_value,
                               0);
      if (ret) {
         return vk_errorf(NULL,
                          VK_ERROR_OUT_OF_DEVICE_MEMORY,
                          "Failed to submit transfer syncobj. Errno: %d - %s.",
                          errno,
                          strerror(errno));
      }

      return VK_SUCCESS;
   }

   simple_mtx_lock(&drm_ws->null_job_sync_mutex);

   if (!drm_ws->null_job_syncobj) {
      ret = drmSyncobjCreate(render_fd, 0, &drm_ws->null_job_syncobj);
      if (ret) {
         result = vk_errorf(NULL,
                            VK_ERROR_OUT_OF_DEVICE_MEMORY,
                            "Failed to create null job syncobj. Errno: %d - %s.",
                            errno,
                            strerror(errno));
         goto out_unlock;
      }
   } else {
      ret = drmSyncobjReset(render_fd, &drm_ws->null_job_syncobj, 1);
      if (ret) {
         result = vk_errorf(NULL,
                            VK_ERROR_OUT_OF_DEVICE_MEMORY,
                            "Failed to reset null job syncobj. Errno: %d - %s.",
                            errno,
                            strerror(errno));
         goto out_discard_syncobj;
      }
   }

   for (uint32_t i = 0; i < wait_count; i++) {
      struct vk_sync *src_sync = waits[i].sync;

      if (!src_sync)
         continue;

      ret = drmSyncobjTransfer(render_fd,
                               drm_ws->null_job_syncobj,
                               i + 1,
                               vk_sync_as_drm_syncobj(src_sync)->syncobj,
                               waits[i].wait_value,
                               0);
      if (ret) {
         result =
            vk_errorf(NULL,
                      VK_ERROR_OUT_OF_DEVICE_MEMORY,
                      "Failed to create temporary syncobj. Errno: %d - %s.",
                      errno,
                      strerror(errno));
         goto out_discard_syncobj;
      }
   }

   ret = drmSyncobjTransfer(render_fd,
                            vk_sync_as_drm_syncobj(signal_sync->sync)->syncobj,
                            signal_sync->signal_value,
                            drm_ws->null_job_syncobj,
                            wait_count,
                            0);
   if (ret) {
      result = vk_errorf(NULL,
                         VK_ERROR_OUT_OF_DEVICE_MEMORY,
                         "Syncobj transfer failed. Errno: %d - %s.",
                         errno,
                         strerror(errno));
   } else {
      result = VK_SUCCESS;
   }

   goto out_unlock;

out_discard_syncobj:
   drmSyncobjDestroy(render_fd, drm_ws->null_job_syncobj);
   drm_ws->null_job_syncobj = 0;

out_unlock:
   simple_mtx_unlock(&drm_ws->null_job_sync_mutex);
   return result;
}
