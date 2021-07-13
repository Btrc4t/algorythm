#include <stdio.h>
#include "coap_endpoints.h"
#include "mdns.h"
#include "rgb_leds.h"
#include "esp_log.h"

#define MAX_LEN_ROOM_NAME 30
#define MAX_LEN_CTRL_NAME 10

const static char *TAG = "CoAP Endpoints";

EventGroupHandle_t endpoint_events;
char room_name[MAX_LEN_ROOM_NAME];
uint8_t rgb_data[3];
settings_data_t settings;
control_mode ctrl_mode = manual;
char ctrl_text[MAX_LEN_CTRL_NAME];

const char *MODE_STRING[] = {
    FOREACH_MODE(GENERATE_STRING)
};

const char *ENDPOINT_STRING[] = {
    FOREACH_ENDPOINT(GENERATE_STRING)
};

/** Generate host name based on sdkconfig, adding a portion of MAC address to it.
 *  @return host name string allocated from the heap
 */
static char* generate_hostname(void)
{
    uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", LEDS_MDNS_HOSTNAME, mac[3], mac[4], mac[5])) {
        abort();
    }
    return hostname;
}

void initialise_mdns(void)
{
    char* hostname = generate_hostname();
    //initialise mDNS
    ESP_ERROR_CHECK( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    ESP_LOGI(TAG, "mdns hostname set to: %s.local", hostname);
    //set default mDNS instance name
    ESP_ERROR_CHECK( mdns_instance_name_set(LEDS_MDNS_INSTANCE) );

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[endpoint_size];
    //  = {
    //     {"room",INITIAL_DATA},
    //     {"mode",INITIAL_DATA},
    // };

    endpoint x;
    for( x = 0; x < endpoint_size; x++ ) {
        serviceTxtData[x] = (mdns_txt_item_t){(const char *)ENDPOINT_STRING[x], (const char *)INITIAL_DATA};
    }

    //initialize service 
    ESP_ERROR_CHECK( mdns_service_add(NULL, "_http", "_tcp", 80, serviceTxtData, endpoint_size) );
    //can be found on network with mDNS: dig @224.0.0.251 -p 5353 -t ptr +short _http._tcp.local
    //if for example the above command returns LEDSHOST-A0B000.local
    //IP can be found with mDNS: dig @224.0.0.251 -p 5353 +short LEDSHOST-A0B000.local
    //change TXT item value
    //ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "mode", "mic") );
    free(hostname);
}

EventGroupHandle_t get_endpoints_event_group() {    
    if(!endpoint_events) {
        initialise_mdns();
        endpoint_events = xEventGroupCreate();
    }
    return endpoint_events;
}

void set_endpoints_handlers(__attribute__((unused)) handlers_t *handlers) {
    *handlers = (handlers_t) { 
        {hnd_espressif_get_room,hnd_espressif_get_rgb,hnd_espressif_get_mode, hnd_espressif_get_settings},
        {hnd_espressif_put_room,hnd_espressif_put_rgb,hnd_espressif_put_mode, hnd_espressif_put_settings},
        {hnd_espressif_delete_room,hnd_espressif_delete_rgb,hnd_espressif_delete_mode, hnd_espressif_delete_settings} 
    };
}

void hnd_espressif_get_room(coap_context_t *ctx, coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request, coap_binary_t *token,
                  coap_string_t *query, coap_pdu_t *response)
{
    coap_add_data_blocked_response(resource, session, request, response, token,
                                   COAP_MEDIATYPE_TEXT_PLAIN, 0,
                                   strlen(room_name)+1,
                                   (const u_char *)room_name);
}

void hnd_espressif_put_room(coap_context_t *ctx,
                  coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request,
                  coap_binary_t *token,
                  coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    unsigned char *data;

    coap_resource_notify_observers(resource, NULL);

    if (strcmp (room_name, INITIAL_DATA) == 0) {
        response->code = COAP_RESPONSE_CODE(201);
    } else {
        response->code = COAP_RESPONSE_CODE(204);
    }

    /* coap_get_data() sets size to 0 on error */
    (void)coap_get_data(request, &size, &data);
    ESP_LOGI(TAG, "Got coap get request type:%d, code: %d, data: %s",
                 request->type, request->code, request->data);

    if (size == 0) {      /* re-init */
        ESP_LOGI(TAG, "Room name at init is: %s",room_name);
        snprintf(room_name, strlen(INITIAL_DATA)+1, INITIAL_DATA);
    } else {
        size_t maxlen = size >= MAX_LEN_ROOM_NAME ? (size_t) MAX_LEN_ROOM_NAME : size+1;
        memcpy (room_name, data, maxlen);
        room_name[maxlen-1] = '\0';
        xEventGroupSetBits(endpoint_events,E_NAME_BIT);
        ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "room", room_name) );
        //TODO also store in nvm and lookup on start
        ESP_LOGI(TAG, "Copied bytes: %d, data: %s",
                 maxlen, room_name);
    }
}

void hnd_espressif_delete_room(coap_context_t *ctx,
                     coap_resource_t *resource,
                     coap_session_t *session,
                     coap_pdu_t *request,
                     coap_binary_t *token,
                     coap_string_t *query,
                     coap_pdu_t *response)
{
    coap_resource_notify_observers(resource, NULL);
    snprintf(room_name, strlen(room_name)+1, INITIAL_DATA);
    response->code = COAP_RESPONSE_CODE(202);
}

void hnd_espressif_get_settings(coap_context_t *ctx, coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request, coap_binary_t *token,
                  coap_string_t *query, coap_pdu_t *response)
{
    coap_add_data_blocked_response(resource, session, request, response, token,
                                   COAP_MEDIATYPE_TEXT_PLAIN, 0,
                                   sizeof(settings.settings_data),
                                   (const u_char *)settings.settings_data);
}
void hnd_espressif_put_settings(coap_context_t *ctx,
                  coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request,
                  coap_binary_t *token,
                  coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    unsigned char *data;

    coap_resource_notify_observers(resource, NULL);

    // if (strcmp (room_name, INITIAL_DATA) == 0) {
    //     response->code = COAP_RESPONSE_CODE(201);
    // } else {
        response->code = COAP_RESPONSE_CODE(204);
    // }

    /* coap_get_data() sets size to 0 on error */
    (void)coap_get_data(request, &size, &data);
    ESP_LOGI(TAG, "Got coap get request type:%d, code: %d, data: %s",
                 request->type, request->code, request->data);

    if (ctrl_mode != manual) {
        ESP_LOGE(TAG, "Cannot set RGB value when not in manual mode");
    } else if (size == sizeof(settings.settings_data)) {
        memcpy (settings.settings_data, data, size);
        xEventGroupSetBits(endpoint_events, E_PREF_BIT);
    } else {
        ESP_LOGE(TAG, "Got unexpected size for rgb array:%d", size);
    }
}

void hnd_espressif_delete_settings(coap_context_t *ctx,
                     coap_resource_t *resource,
                     coap_session_t *session,
                     coap_pdu_t *request,
                     coap_binary_t *token,
                     coap_string_t *query,
                     coap_pdu_t *response)
{
    coap_resource_notify_observers(resource, NULL);
    const uint8_t empty[12] = {0};
    memcpy(settings.settings_data, empty, sizeof(empty));
    response->code = COAP_RESPONSE_CODE(202);
}

void hnd_espressif_get_rgb(coap_context_t *ctx, coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request, coap_binary_t *token,
                  coap_string_t *query, coap_pdu_t *response)
{
    coap_add_data_blocked_response(resource, session, request, response, token,
                                   COAP_MEDIATYPE_TEXT_PLAIN, 0,
                                   sizeof(rgb_data),
                                   (const u_char *)rgb_data);
}
void hnd_espressif_put_rgb(coap_context_t *ctx,
                  coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request,
                  coap_binary_t *token,
                  coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    unsigned char *data;

    coap_resource_notify_observers(resource, NULL);

    // if (strcmp (room_name, INITIAL_DATA) == 0) {
    //     response->code = COAP_RESPONSE_CODE(201);
    // } else {
        response->code = COAP_RESPONSE_CODE(204);
    // }

    /* coap_get_data() sets size to 0 on error */
    (void)coap_get_data(request, &size, &data);
    ESP_LOGI(TAG, "Got coap get request type:%d, code: %d, data: %s",
                 request->type, request->code, request->data);

    if (ctrl_mode != manual) {
        ESP_LOGE(TAG, "Cannot set RGB value when not in manual mode");
    } else if (size == sizeof(rgb_data)) {
        memcpy (rgb_data, data, size);
        set_rgb(rgb_data[COLOR_R_IDX], rgb_data[COLOR_G_IDX], rgb_data[COLOR_B_IDX], 100);
    } else {
        ESP_LOGE(TAG, "Got unexpected size for rgb array:%d", size);
    }
}

void hnd_espressif_delete_rgb(coap_context_t *ctx,
                     coap_resource_t *resource,
                     coap_session_t *session,
                     coap_pdu_t *request,
                     coap_binary_t *token,
                     coap_string_t *query,
                     coap_pdu_t *response)
{
    coap_resource_notify_observers(resource, NULL);
    const uint8_t empty[3] = {0};
    memcpy(rgb_data, empty, sizeof(empty));
    response->code = COAP_RESPONSE_CODE(202);
}

void hnd_espressif_get_mode(coap_context_t *ctx, coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request, coap_binary_t *token,
                  coap_string_t *query, coap_pdu_t *response)
{
    coap_add_data_blocked_response(resource, session, request, response, token,
                                   COAP_MEDIATYPE_TEXT_PLAIN, 0,
                                   strlen(ctrl_text)+1,
                                   (const u_char *)ctrl_text);
}

void hnd_espressif_put_mode(coap_context_t *ctx,
                  coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request,
                  coap_binary_t *token,
                  coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    unsigned char *data;

    coap_resource_notify_observers(resource, NULL);

    if (strcmp (room_name, INITIAL_DATA) == 0) {
        response->code = COAP_RESPONSE_CODE(201);
    } else {
        response->code = COAP_RESPONSE_CODE(204);
    }

    /* coap_get_data() sets size to 0 on error */
    (void)coap_get_data(request, &size, &data);
    ESP_LOGI(TAG, "Got coap get request type:%d, code: %d, data: %s",
                 request->type, request->code, request->data);

    if (size == 1 && data[0] < modes_size) {      /* re-init */
        ctrl_mode = data[0];
        snprintf(ctrl_text, sizeof(MODE_STRING[data[0]]), MODE_STRING[data[0]]);
        xEventGroupSetBits(endpoint_events,E_MODE_BIT);
        ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "mode", MODE_STRING[data[0]]) );
    } else {
        ESP_LOGE(TAG, "Got unexpected size for mode:%d, data[0]:%d", size, data[0]);
    }
}

void hnd_espressif_delete_mode(coap_context_t *ctx,
                     coap_resource_t *resource,
                     coap_session_t *session,
                     coap_pdu_t *request,
                     coap_binary_t *token,
                     coap_string_t *query,
                     coap_pdu_t *response)
{
    coap_resource_notify_observers(resource, NULL);
    snprintf(ctrl_text, strlen(ctrl_text)+1, MODE_STRING[manual]);
    ctrl_mode = manual;
    response->code = COAP_RESPONSE_CODE(202);
}

int verify_cn_callback(const char *cn,
                   const uint8_t *asn1_public_cert,
                   size_t asn1_length,
                   coap_session_t *session,
                   unsigned depth,
                   int validated,
                   void *arg
                  )
{
    coap_log(LOG_INFO, "CN '%s' presented by server (%s)\n",
             cn, depth ? "CA" : "Certificate");
    return 1;
}