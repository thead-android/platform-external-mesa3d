# Copyright © 2025 Imagination Technologies Ltd.
# SPDX-License-Identifier: MIT

pco_usclib_shader_files = files(
  'common.cl',
  'sync.cl',
  'tex.cl',
)

if get_option('imagination-android-precompiled')
  pco_usclib = files(
    '../../android-precompiled/pco/usclib/pco_usclib.cpp',
    '../../android-precompiled/pco/usclib/pco_usclib.h',
  )
  pco_usclib_inc = include_directories('../../android-precompiled/pco/usclib')
else
  pco_usclib_spv = custom_target(
    'pco_usclib.spv',
    input : pco_usclib_shader_files,
    output : 'pco_usclib.spv',
    command : [
      prog_mesa_clc,
      '-o', '@OUTPUT@', pco_usclib_shader_files,
      '--depfile', '@DEPFILE@', '--',
      '-I' + join_paths(meson.project_source_root(), 'include'),
      '-I' + join_paths(meson.project_source_root(), 'src/compiler/libcl'),
      '-I' + join_paths(meson.current_source_dir(), '.'),
      '-I' + join_paths(meson.current_source_dir(), '../..'),
      '-I' + join_paths(meson.current_source_dir(), '../../include'),
      '-I' + join_paths(meson.current_source_dir(), '../../..'),
      '-I' + join_paths(meson.current_build_dir(), '../..'),
      cl_args,
    ],
    depends : [pco_pygen_sources, pvr_xml_pack],
    depfile : 'pco_usclib.h.d',
  )

  pco_usclib = custom_target(
    'pco_usclib.h',
    input : pco_usclib_spv,
    output : ['pco_usclib.cpp', 'pco_usclib.h'],
    command : [prog_vtn_bindgen2, '@INPUT0@', '@OUTPUT0@', '@OUTPUT1@'],
  )
  pco_usclib_inc = include_directories('.')
endif

idep_pco_usclib = declare_dependency(
  sources : [pco_usclib],
  include_directories : pco_usclib_inc,
)
