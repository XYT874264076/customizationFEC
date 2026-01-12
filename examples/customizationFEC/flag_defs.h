/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_MYFECEXP_FLAG_DEFS_H_
#define EXAMPLES_MYFECEXP_FLAG_DEFS_H_

#include <string>

#include "absl/flags/flag.h"

extern const uint16_t kDefaultServerPort;  // From defaults.[h|cc]

ABSL_FLAG(std::string, server, "192.168.0.1", "The server to connect to.");
ABSL_FLAG(std::string, file, "/home/ubuntu/Desktop/MyFECExp/testVideo/testVideo.mp4", "Path to MP4 file which is ready to play!");
ABSL_FLAG(int,
          port,
          kDefaultServerPort,
          "The port on which the server is listening.");

ABSL_FLAG(
    std::string,
    force_fieldtrials,
    "",
    "Field trials control experimental features. This flag specifies the field "
    "trials in effect. E.g. running with "
    "--force_fieldtrials=WebRTC-FooFeature/Enabled/ "
    "will assign the group Enabled to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");

#endif  // EXAMPLES_MYFECEXP_FLAG_DEFS_H_
