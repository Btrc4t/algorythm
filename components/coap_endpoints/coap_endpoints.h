#ifndef COAP_ENDPOINTS_H
#define COAP_ENDPOINTS_H

#include "coap3/coap.h"
#include "freertos/event_groups.h"

#define INITIAL_DATA "Undefined"
#define USE_COAP "Use CoAP"

#define MAX_LEN_ROOM_NAME 30
#define MAX_LEN_CTRL_NAME 10

#define LEDS_MDNS_HOSTNAME CONFIG_LEDS_MDNS_HOSTNAME
#define LEDS_MDNS_INSTANCE CONFIG_LEDS_MDNS_INSTANCE

/* The endpoint event group has three events:
 * - an update occured on the rgb endpoint
 * - an update occured on the mode endpoint 
 * - an update occured on the room (name) endpoint 
 * These events are used to save updates in NVS*/
#define E_RGB_BIT            BIT0
#define E_MODE_BIT           BIT1
#define E_NAME_BIT           BIT2
#define E_PREF_BIT           BIT3
#define ALL_ENDPOINT_EVENTS  (E_RGB_BIT | E_MODE_BIT | E_NAME_BIT | E_PREF_BIT)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

#define FOREACH_ENDPOINT(ENDPOINT) \
        ENDPOINT(room)   \
        ENDPOINT(rgb)  \
        ENDPOINT(mode)   \
        ENDPOINT(prefs) \
        ENDPOINT(endpoint_size)  \

enum endpoint_enum {
    FOREACH_ENDPOINT(GENERATE_ENUM)
};

typedef enum endpoint_enum endpoint; 

#define FOREACH_MODE(MODE) \
        MODE(manual)   \
        MODE(audio_intensity)  \
        MODE(audio_freq)  \
        MODE(audio) \
        MODE(off) \
        MODE(audio_hold) \
        MODE(modes_size)  \

enum modes_enum {
    FOREACH_MODE(GENERATE_ENUM)
};

typedef enum modes_enum control_mode; 

typedef struct handlers_t
{
  coap_method_handler_t get_handlers[endpoint_size];
  coap_method_handler_t put_handlers[endpoint_size];
  coap_method_handler_t del_handlers[endpoint_size];
} handlers_t;

typedef struct settings_storage
{
  uint16_t amp_min;
  uint16_t amp_max;
  uint16_t freq_b_start;
  uint16_t freq_b_end;
  uint16_t freq_g_end;
  uint16_t freq_r_end;
  uint16_t hold_mode_int;
} settings_storage;

typedef union settings_data_t {
   uint8_t settings_data[14];
   settings_storage settings_st;
} settings_data_t;

/* FreeRTOS event group to signal when color, mode or endpoint name are updated
    to store changes in NVS*/
extern EventGroupHandle_t endpoint_events;

extern const char *MODE_STRING[];
extern const char *ENDPOINT_STRING[];

extern char room_name[MAX_LEN_ROOM_NAME];
extern uint8_t rgb_data[3];
extern settings_data_t settings;
extern control_mode ctrl_mode;
extern char ctrl_text[MAX_LEN_CTRL_NAME];

EventGroupHandle_t get_endpoints_event_group();

void set_endpoints_handlers(__attribute__((unused)) handlers_t *handlers);

void initialise_mdns(void);

char* generate_name(char *prefix);

void hnd_espressif_get_room(coap_resource_t *resource,
                  coap_session_t *session, const coap_pdu_t *request,
                  const coap_string_t *query, coap_pdu_t *response);

void hnd_espressif_put_room(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response);

void hnd_espressif_delete_room(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response);

void hnd_espressif_get_settings(coap_resource_t *resource,
                  coap_session_t *session, const coap_pdu_t *request,
                  const coap_string_t *query, coap_pdu_t *response);

void hnd_espressif_put_settings(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response);

void hnd_espressif_delete_settings(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response);

void hnd_espressif_get_rgb(coap_resource_t *resource,
                  coap_session_t *session, const coap_pdu_t *request,
                  const coap_string_t *query, coap_pdu_t *response);

void hnd_espressif_put_rgb(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response);

void hnd_espressif_delete_rgb(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response);

void hnd_espressif_get_mode(coap_resource_t *resource,
                  coap_session_t *session, const coap_pdu_t *request,
                  const coap_string_t *query, coap_pdu_t *response);

void hnd_espressif_put_mode(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response);

void hnd_espressif_delete_mode(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response);

int verify_cn_callback(const char *cn,
                   const uint8_t *asn1_public_cert,
                   size_t asn1_length,
                   coap_session_t *session,
                   unsigned depth,
                   int validated,
                   void *arg
                  );

#endif /* COAP_ENDPOINTS_H */
