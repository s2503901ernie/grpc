/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <grpc/support/port_platform.h>

#include <limits.h>
#include <string.h>

#include "src/core/lib/channel/channel_stack_builder.h"
#include "src/core/lib/config/core_configuration.h"
#include "src/core/lib/debug/trace.h"
#include "src/core/lib/security/authorization/sdk_server_authz_filter.h"
#include "src/core/lib/security/context/security_context.h"
#include "src/core/lib/security/credentials/credentials.h"
#include "src/core/lib/security/credentials/plugin/plugin_credentials.h"
#include "src/core/lib/security/security_connector/security_connector.h"
#include "src/core/lib/security/transport/auth_filters.h"
#include "src/core/lib/security/transport/secure_endpoint.h"
#include "src/core/lib/security/transport/security_handshaker.h"
#include "src/core/lib/surface/init.h"
#include "src/core/tsi/transport_security_interface.h"

void grpc_security_pre_init(void) {}

static bool maybe_prepend_client_auth_filter(
    grpc_core::ChannelStackBuilder* builder) {
  const grpc_channel_args* args = builder->channel_args();
  if (args) {
    for (size_t i = 0; i < args->num_args; i++) {
      if (0 == strcmp(GRPC_ARG_SECURITY_CONNECTOR, args->args[i].key)) {
        builder->PrependFilter(&grpc_client_auth_filter, nullptr);
        break;
      }
    }
  }
  return true;
}

static bool maybe_prepend_server_auth_filter(
    grpc_core::ChannelStackBuilder* builder) {
  const grpc_channel_args* args = builder->channel_args();
  if (args) {
    for (size_t i = 0; i < args->num_args; i++) {
      if (0 == strcmp(GRPC_SERVER_CREDENTIALS_ARG, args->args[i].key)) {
        builder->PrependFilter(&grpc_server_auth_filter, nullptr);
        break;
      }
    }
  }
  return true;
}

static bool maybe_prepend_sdk_server_authz_filter(
    grpc_core::ChannelStackBuilder* builder) {
  const grpc_channel_args* args = builder->channel_args();
  const auto* provider =
      grpc_channel_args_find_pointer<grpc_authorization_policy_provider>(
          args, GRPC_ARG_AUTHORIZATION_POLICY_PROVIDER);
  if (provider != nullptr) {
    builder->PrependFilter(&grpc_core::SdkServerAuthzFilter::kFilterVtable,
                           nullptr);
  }
  return true;
}

namespace grpc_core {
void RegisterSecurityFilters(CoreConfiguration::Builder* builder) {
  // Register the auth client with a priority < INT_MAX to allow the authority
  // filter -on which the auth filter depends- to be higher on the channel
  // stack.
  builder->channel_init()->RegisterStage(GRPC_CLIENT_SUBCHANNEL, INT_MAX - 1,
                                         maybe_prepend_client_auth_filter);
  builder->channel_init()->RegisterStage(GRPC_CLIENT_DIRECT_CHANNEL,
                                         INT_MAX - 1,
                                         maybe_prepend_client_auth_filter);
  builder->channel_init()->RegisterStage(GRPC_SERVER_CHANNEL, INT_MAX - 1,
                                         maybe_prepend_server_auth_filter);
  // Register the SdkServerAuthzFilter with a priority less than
  // server_auth_filter to allow server_auth_filter on which the sdk filter
  // depends on to be higher on the channel stack.
  builder->channel_init()->RegisterStage(GRPC_SERVER_CHANNEL, INT_MAX - 2,
                                         maybe_prepend_sdk_server_authz_filter);
}
}  // namespace grpc_core
