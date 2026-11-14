// SPDX-License-Identifier: MIT
#include "scs-telemetry-common.hpp"
#include "scssdk_telemetry.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "common/scssdk_telemetry_truck_common_channels.h"
#include "amtrucks/scssdk_ats.h"
#include "eurotrucks2/scssdk_eut2.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdlib>
#include <limits.h>
#include <unistd.h>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {
// Populate the subset of RenCloud revision 12 that the desktop already understands.
scsTelemetryMap_t snapshot{};
constexpr size_t kSnapshotSize = 3436;
static_assert(offsetof(scsTelemetryMap_t, config_s.jobMarket) + 32 == kSnapshotSize);
static_assert(offsetof(scsTelemetryMap_t, truck_fp) == 2000);
static_assert(offsetof(scsTelemetryMap_t, truck_dp) == 2200);
char directory[PATH_MAX]{};
char staging[64]{};
bool active = false;

void publish() {
 const int dir = open(directory,O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
 if (dir < 0) return;
 struct stat status{};
 if (fstat(dir,&status)==0 && status.st_uid==getuid() && !(status.st_mode & 0077)) {
  const int file = openat(dir,staging,O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC | O_NOFOLLOW,0600);
  if (file >= 0) {
   const auto size = write(file,&snapshot,kSnapshotSize);
   close(file);
   if (size == kSnapshotSize) renameat(dir,staging,dir,"native.bin");
   else unlinkat(dir,staging,0);
  }
 }
 close(dir);
}

template<auto Field>
void SCSAPIFUNC store(const scs_string_t, scs_u32_t, const scs_value_t* value, scs_context_t target) {
 if (!value) return;
 const auto& field = value->*Field;
 std::memcpy(target, &field, sizeof(field));
}

void SCSAPIFUNC storeWorld(const scs_string_t, scs_u32_t, const scs_value_t* value, scs_context_t) {
 if (!value) return;
 const auto& [position, orientation, padding] = value->value_dplacement;
 snapshot.truck_dp = {position.x, position.y, position.z, orientation.heading, orientation.pitch, orientation.roll};
}

void SCSAPIFUNC frameStart(scs_event_t, const void* info, scs_context_t) {
 snapshot.renderTime = static_cast<const scs_telemetry_frame_start_t*>(info)->render_time;
}

void SCSAPIFUNC frameEnd(scs_event_t, const void*, scs_context_t) { publish(); }

void SCSAPIFUNC pause(scs_event_t event, const void*, scs_context_t) {
 snapshot.paused = event == SCS_TELEMETRY_EVENT_paused;
}

void SCSAPIFUNC config(scs_event_t, const void* info, scs_context_t) {
 const auto& event = *static_cast<const scs_telemetry_configuration_t*>(info);
 auto& fields = snapshot.config_s;
 if (std::strcmp(event.id,SCS_TELEMETRY_CONFIG_job)==0) {
  fields.jobMarket[0]='\0';
  for (auto item=event.attributes; item->name; ++item) {
   if (item->value.type==SCS_VALUE_TYPE_string &&
       std::strcmp(item->name,SCS_TELEMETRY_CONFIG_ATTRIBUTE_job_market)==0)
    std::snprintf(fields.jobMarket,sizeof fields.jobMarket,"%s",item->value.value_string.value);
  }
  return;
 }
 if (std::strcmp(event.id,SCS_TELEMETRY_CONFIG_truck)!=0) return;
 fields.truckBrandId[0]=fields.truckBrand[0]=fields.truckId[0]=fields.truckName[0]=fields.truckLicensePlate[0]='\0';
 snapshot.config_fv = {};
 const std::array strings{
  std::pair{SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand_id,fields.truckBrandId},
  std::pair{SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand,fields.truckBrand},
  std::pair{SCS_TELEMETRY_CONFIG_ATTRIBUTE_id,fields.truckId},
  std::pair{SCS_TELEMETRY_CONFIG_ATTRIBUTE_name,fields.truckName},
  std::pair{SCS_TELEMETRY_CONFIG_ATTRIBUTE_license_plate,fields.truckLicensePlate}
 };
 const std::array points{
  std::pair{SCS_TELEMETRY_CONFIG_ATTRIBUTE_cabin_position,&snapshot.config_fv.cabinPositionX},
  std::pair{SCS_TELEMETRY_CONFIG_ATTRIBUTE_head_position,&snapshot.config_fv.headPositionX},
  std::pair{SCS_TELEMETRY_CONFIG_ATTRIBUTE_hook_position,&snapshot.config_fv.truckHookPositionX}
 };
 for (auto item=event.attributes; item->name; ++item) {
  if (item->value.type==SCS_VALUE_TYPE_string) {
   for (const auto& [key,dest]:strings)
    if (std::strcmp(item->name,key)==0) std::snprintf(dest,stringsize,"%s",item->value.value_string.value);
  } else if (item->value.type==SCS_VALUE_TYPE_fvector) {
   for (const auto& [key,dest]:points)
    if (std::strcmp(item->name,key)==0) std::memcpy(dest,&item->value.value_fvector,sizeof(scs_value_fvector_t));
  }
 }
}
}

extern "C" __attribute__((visibility("default"))) SCSAPI_RESULT scs_telemetry_init(
 scs_u32_t version, const scs_telemetry_init_params_t* parameters) {
 if (version!=SCS_TELEMETRY_VERSION_1_00 && version!=SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
 if (!parameters || active) return SCS_RESULT_generic_error;
 const auto& sdk = *static_cast<const scs_telemetry_init_params_v100_t*>(parameters);
 const int game = std::strcmp(sdk.common.game_id,SCS_GAME_ID_EUT2)==0 ? ETS2
                : std::strcmp(sdk.common.game_id,SCS_GAME_ID_ATS)==0 ? ATS : UnknownGame;
 if (!game) return SCS_RESULT_unsupported;
 const char* home = std::getenv("HOME");
 if (!home || !*home) return SCS_RESULT_generic_error;
 const int length=std::snprintf(directory,sizeof directory,"%s/.local/share/TruckAudioSim/telemetry",home);
 if (length<=0 || static_cast<size_t>(length)>=sizeof directory) return SCS_RESULT_generic_error;
 std::snprintf(staging,sizeof staging,"native-%ld.next",static_cast<long>(getpid()));
 snapshot={};
 snapshot.sdkActive=true;
 snapshot.paused=true;
 snapshot.scs_values.telemetry_plugin_revision=PLUGIN_REVID;
 snapshot.scs_values.game=game;
 const bool events=sdk.register_for_event(SCS_TELEMETRY_EVENT_frame_start,frameStart,nullptr)==SCS_RESULT_ok
  && sdk.register_for_event(SCS_TELEMETRY_EVENT_frame_end,frameEnd,nullptr)==SCS_RESULT_ok
  && sdk.register_for_event(SCS_TELEMETRY_EVENT_paused,pause,nullptr)==SCS_RESULT_ok
  && sdk.register_for_event(SCS_TELEMETRY_EVENT_started,pause,nullptr)==SCS_RESULT_ok
  && sdk.register_for_event(SCS_TELEMETRY_EVENT_configuration,config,nullptr)==SCS_RESULT_ok;
 if (!events) return SCS_RESULT_generic_error;
 const auto channel=[&](const char* name, scs_value_type_t type, scs_telemetry_channel_callback_t fn, void* context) {
  return sdk.register_for_channel(name,SCS_U32_NIL,type,SCS_TELEMETRY_CHANNEL_FLAG_no_value,fn,context);
 };
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_world_placement,SCS_VALUE_TYPE_dplacement,storeWorld,nullptr);
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_cabin_offset,SCS_VALUE_TYPE_fplacement,store<&scs_value_t::value_fplacement>,&snapshot.truck_fp.cabinOffsetX);
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_head_offset,SCS_VALUE_TYPE_fplacement,store<&scs_value_t::value_fplacement>,&snapshot.truck_fp.headOffsetX);
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear,SCS_VALUE_TYPE_s32,store<&scs_value_t::value_s32>,&snapshot.truck_i.gear);
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_effective_steering,SCS_VALUE_TYPE_float,store<&scs_value_t::value_float>,&snapshot.truck_f.gameSteer);
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_electric_enabled,SCS_VALUE_TYPE_bool,store<&scs_value_t::value_bool>,&snapshot.truck_b.electricEnabled);
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_engine_enabled,SCS_VALUE_TYPE_bool,store<&scs_value_t::value_bool>,&snapshot.truck_b.engineEnabled);
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_lblinker,SCS_VALUE_TYPE_bool,store<&scs_value_t::value_bool>,&snapshot.truck_b.blinkerLeftActive);
 channel(SCS_TELEMETRY_TRUCK_CHANNEL_rblinker,SCS_VALUE_TYPE_bool,store<&scs_value_t::value_bool>,&snapshot.truck_b.blinkerRightActive);
 active=true;
 sdk.common.log(SCS_LOG_TYPE_message,"[TruckAudioSim] Linux telemetry enabled");
 return SCS_RESULT_ok;
}

extern "C" __attribute__((visibility("default"))) SCSAPI_VOID scs_telemetry_shutdown() {
 if (!active) return;
 snapshot.sdkActive=false;
 publish();
 active=false;
}
