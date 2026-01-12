
#ifndef EXAMPLES_CUSTOMIZATION_MAIN_WND_H_
#define EXAMPLES_CUSTOMIZATION_MAIN_WND_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>

#include "api/array_view.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "examples/customizationFEC/peer_connection_client.h"
#include "media/base/media_channel.h"
#include "media/base/video_common.h"
#include "rtc_base/buffer.h"

class MainWndCallback {
 public:
  virtual void UIThreadCallback(int msg_id, void* data) = 0;
  virtual void Close() = 0;

 protected:
  virtual ~MainWndCallback() {}
};

// Pure virtual interface for the main window.
class MainWindow {
 public:
  virtual ~MainWindow() {}

  virtual void RegisterObserver(MainWndCallback* callback) = 0;

  virtual bool IsWindow() = 0;
  virtual void InitializeUI() = 0;
  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) = 0;
  virtual void StopLocalRenderer() = 0;
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) = 0;
  virtual void StopRemoteRenderer() = 0;

  virtual void QueueUIThreadCallback(int msg_id, void* data) = 0;
};

// Forward declarations.
typedef struct _GtkWidget GtkWidget;
typedef union _GdkEvent GdkEvent;
typedef struct _GdkEventKey GdkEventKey;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkTreeViewColumn GtkTreeViewColumn;
typedef struct _cairo cairo_t;

// Implements the main UI of the peer connection client.
// This is functionally equivalent to the MainWnd class in the Windows
// implementation.
class GtkMainWnd : public MainWindow {
 public:
  GtkMainWnd(const char* server, int port);
  ~GtkMainWnd();

  virtual void RegisterObserver(MainWndCallback* callback);
  virtual bool IsWindow();
  virtual void InitializeUI();
  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
  virtual void StopLocalRenderer();
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
  virtual void StopRemoteRenderer();

  virtual void QueueUIThreadCallback(int msg_id, void* data);

  // Creates and shows the main window.
  bool Create();
  // Destroys the window. When the window is destroyed, it ends the main message loop.
  bool Destroy();

  // Callback for when the main window is destroyed.
  void OnDestroyed(GtkWidget* widget, GdkEvent* event);

  void OnRedraw();
  void Draw(GtkWidget* widget, cairo_t* cr);

 protected:
  class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    VideoRenderer(GtkMainWnd* main_wnd, webrtc::VideoTrackInterface* track_to_render);
    virtual ~VideoRenderer();

    // VideoSinkInterface implementation
    void OnFrame(const webrtc::VideoFrame& frame) override;

    rtc::ArrayView<const uint8_t> image() const { return image_; }

    int width() const { return width_; }

    int height() const { return height_; }

   protected:
    void SetSize(int width, int height);
    rtc::Buffer image_;
    int width_;
    int height_;
    GtkMainWnd* main_wnd_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
  };

 protected:
  GtkWidget* window_;     // Our main window.
  GtkWidget* draw_area_;  // The drawing surface for rendering video streams.
  MainWndCallback* callback_;
  std::string server_;
  std::string port_;
  std::unique_ptr<VideoRenderer> local_renderer_;
  std::unique_ptr<VideoRenderer> remote_renderer_;
  int width_ = 0;
  int height_ = 0;
  rtc::Buffer draw_buffer_;
  int draw_buffer_size_;
};

#endif  // EXAMPLES_CUSTOMIZATION_MAIN_WND_H_