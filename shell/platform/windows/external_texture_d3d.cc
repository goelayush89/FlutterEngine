// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/external_texture_d3d.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <iostream>

#include "flutter/shell/platform/embedder/embedder_struct_macros.h"

namespace flutter {

ExternalTextureD3d::ExternalTextureD3d(
    FlutterDesktopGpuSurfaceType type,
    const FlutterDesktopGpuSurfaceTextureCallback texture_callback,
    void* user_data,
    const AngleSurfaceManager* surface_manager,
    const GlProcs& gl_procs)
    : type_(type),
      texture_callback_(texture_callback),
      user_data_(user_data),
      surface_manager_(surface_manager),
      gl_(gl_procs) {}

ExternalTextureD3d::~ExternalTextureD3d() {
  ReleaseImage();

  if (gl_texture_ != 0) {
    gl_.glDeleteTextures(1, &gl_texture_);
  }
}

bool ExternalTextureD3d::PopulateTexture(size_t width,
                                         size_t height,
                                         FlutterOpenGLTexture* opengl_texture) {
  const FlutterDesktopGpuSurfaceDescriptor* descriptor =
      texture_callback_(width, height, user_data_);

  if (!CreateOrUpdateTexture(descriptor)) {
    return false;
  }

  // Populate the texture object used by the engine.
  opengl_texture->target = GL_TEXTURE_2D;
  opengl_texture->name = gl_texture_;
  opengl_texture->format = GL_RGBA;
  opengl_texture->destruction_callback = nullptr;
  opengl_texture->user_data = nullptr;
  opengl_texture->width = SAFE_ACCESS(descriptor, visible_width, 0);
  opengl_texture->height = SAFE_ACCESS(descriptor, visible_height, 0);

  return true;
}

void ExternalTextureD3d::ReleaseImage() {
  if (egl_surface_ != EGL_NO_SURFACE) {
    eglReleaseTexImage(surface_manager_->egl_display(), egl_surface_,
                       EGL_BACK_BUFFER);
    eglDestroySurface(surface_manager_->egl_display(), egl_surface_);
    egl_surface_ = EGL_NO_SURFACE;
  }
}

bool ExternalTextureD3d::CreateOrUpdateTexture(
    const FlutterDesktopGpuSurfaceDescriptor* descriptor) {
  if (descriptor == nullptr ||
      SAFE_ACCESS(descriptor, handle, nullptr) == nullptr) {
    ReleaseImage();
    return false;
  }

  if (gl_texture_ == 0) {
    gl_.glGenTextures(1, &gl_texture_);

    gl_.glBindTexture(GL_TEXTURE_2D, gl_texture_);
    gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else {
    gl_.glBindTexture(GL_TEXTURE_2D, gl_texture_);
  }

  auto handle = SAFE_ACCESS(descriptor, handle, nullptr);
  if (handle != last_surface_handle_) {
    ReleaseImage();

    EGLint attributes[] = {
        EGL_WIDTH,
        static_cast<EGLint>(SAFE_ACCESS(descriptor, width, 0)),
        EGL_HEIGHT,
        static_cast<EGLint>(SAFE_ACCESS(descriptor, height, 0)),
        EGL_TEXTURE_TARGET,
        EGL_TEXTURE_2D,
        EGL_TEXTURE_FORMAT,
        EGL_TEXTURE_RGBA,  // always EGL_TEXTURE_RGBA
        EGL_NONE};

    egl_surface_ = surface_manager_->CreateSurfaceFromHandle(
        (type_ == kFlutterDesktopGpuSurfaceTypeD3d11Texture2D)
            ? EGL_D3D_TEXTURE_ANGLE
            : EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
        handle, attributes);

    if (egl_surface_ == EGL_NO_SURFACE ||
        eglBindTexImage(surface_manager_->egl_display(), egl_surface_,
                        EGL_BACK_BUFFER) == EGL_FALSE) {
      std::cerr << "Binding D3D surface failed." << std::endl;
    }
    last_surface_handle_ = handle;
  }

  auto release_callback = SAFE_ACCESS(descriptor, release_callback, nullptr);
  if (release_callback) {
    release_callback(SAFE_ACCESS(descriptor, release_context, nullptr));
  }
  return egl_surface_ != EGL_NO_SURFACE;
}

}  // namespace flutter
