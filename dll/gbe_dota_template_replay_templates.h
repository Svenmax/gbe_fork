#ifndef GBE_DOTA_TEMPLATE_REPLAY_TEMPLATES_H
#define GBE_DOTA_TEMPLATE_REPLAY_TEMPLATES_H

#include <cstddef>
#include <cstdint>

#include <steam/steamtypes.h>

// D.12.1: canned template-replay protocol assets live outside the handler logic TU.
// Definitions: gbe_dota_template_replay_templates.cpp

extern const char * const GBE_kDota8730TemplateHex;
extern const char * const GBE_kDota8331TemplateHex;
extern const char * const GBE_kDotaOfficial8745TemplateHex;

extern const uint8 GBE_kDota8678Template[10];
extern const uint8 GBE_kDota8136Template[17];
extern const uint8 GBE_kDota2538Template[16];
extern const uint8 GBE_kDota2618Template[19];
extern const uint8 GBE_kDota8674Template[19];
extern const uint8 GBE_kDota8677Template[21];
extern const uint8 GBE_kDota7198Template[333];
extern const uint8 GBE_kDota8079Template[19];
extern const uint8 GBE_kDota8854Template[53];
extern const uint8 GBE_kDota9024Template[23];

#endif
