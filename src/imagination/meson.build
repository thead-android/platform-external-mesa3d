# Copyright © 2022 Imagination Technologies Ltd.
# SPDX-License-Identifier: MIT

imagination_c_args = cc.get_supported_arguments(
  '-Werror=pointer-arith',
)

inc_imagination = include_directories([
  '.',
  'common',
  'include',
])

if get_option('imagination-android-precompiled')
  inc_imagination = [inc_imagination, include_directories('android-precompiled')]
endif

if with_imagination_vk or with_tools.contains('imagination')
  subdir('common')
  subdir('csbgen')
  subdir('pco')
  subdir('pdsc')
endif

if with_imagination_vk
  subdir('vulkan')

  if with_tools.contains('drm-shim')
    subdir('drm-shim')
  endif
endif
