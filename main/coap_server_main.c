/* 
   ESP32 IOT appliance based on the CoAP protocol with rgb leds reacting to sound input.
*/

/*
 * WARNING
 * libcoap is not multi-thread safe, so only this thread must make any coap_*()
 * calls.  Any external (to this thread) data transmitted in/out via libcoap
 * therefore has to be passed in/out by xQueue*() via this thread.
 */

#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_attr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_types.h"
#include "esp_netif.h"

#include "netdb.h"
#include "mdns.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/i2s.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/timer.h"

#include "coap_endpoints.h"

#include "rgb_leds.h"

#include "fft.h"

#define CORE_0 (BaseType_t)(0)
#define CORE_1 (BaseType_t)(1)

#define ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASS
#define ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1

// #define CONFIG_COAP_MBEDTLS_PKI
// #define CONFIG_MBEDTLS_TLS_SERVER

/* The examples use simple Pre-Shared-Key configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_COAP_PSK_KEY "some-agreed-preshared-key"

   Note: PSK will only be used if the URI is prefixed with coaps://
   instead of coap:// and the PSK must be one that the server supports
   (potentially associated with the IDENTITY)
*/
#define COAP_PSK CONFIG_EXAMPLE_COAP_PSK_KEY

/* The examples use CoAP Logging Level that
   you can set via 'make menuconfig'.

   If you'd rather not, just change the below entry to a value
   that is between 0 and 7 with
   the config you want - ie #define EXAMPLE_COAP_LOG_DEFAULT_LEVEL 7
*/
#define EXAMPLE_COAP_LOG_DEFAULT_LEVEL CONFIG_COAP_LOG_DEFAULT_LEVEL

const static char *TAG = "LED mDNS CoAP_server";

#ifdef CONFIG_COAP_MBEDTLS_PKI
/* CA cert, taken from coap_ca.pem
   Server cert, taken from coap_server.crt
   Server key, taken from coap_server.key

   The PEM, CRT and KEY file are examples taken from the wpa2 enterprise
   example.

   To embed it in the app binary, the PEM, CRT and KEY file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
 */
extern uint8_t ca_pem_start[] asm("_binary_coap_ca_pem_start");
extern uint8_t ca_pem_end[]   asm("_binary_coap_ca_pem_end");
extern uint8_t server_crt_start[] asm("_binary_coap_server_crt_start");
extern uint8_t server_crt_end[]   asm("_binary_coap_server_crt_end");
extern uint8_t server_key_start[] asm("_binary_coap_server_key_start");
extern uint8_t server_key_end[]   asm("_binary_coap_server_key_end");
#endif /* CONFIG_COAP_MBEDTLS_PKI */

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to SSID:%s", ESP_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", ESP_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}
 
static void coap_server(void *p)
{
    BaseType_t core = xPortGetCoreID();
    ESP_LOGI(TAG, "running coap on core %d",core);
    coap_context_t *ctx = NULL;
    coap_address_t serv_addr;
    coap_resource_t *resource = NULL;

    coap_set_log_level(EXAMPLE_COAP_LOG_DEFAULT_LEVEL);

    while (1) {
        coap_endpoint_t *ep = NULL;
        unsigned wait_ms;

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family      = AF_INET;
        serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
        serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(NULL);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            continue;
        }
#ifdef CONFIG_COAP_MBEDTLS_PSK
        /* Need PSK setup before we set up endpoints */
        coap_context_set_psk(ctx, "CoAP",
                             (const uint8_t *)COAP_PSK,
                             sizeof(COAP_PSK) - 1);
#endif /* CONFIG_COAP_MBEDTLS_PSK */

#ifdef CONFIG_COAP_MBEDTLS_PKI
        unsigned int ca_pem_bytes = ca_pem_end - ca_pem_start;
        unsigned int server_crt_bytes = server_crt_end - server_crt_start;
        unsigned int server_key_bytes = server_key_end - server_key_start;
        coap_dtls_pki_t dtls_pki;

        memset (&dtls_pki, 0, sizeof(dtls_pki));
        dtls_pki.version = COAP_DTLS_PKI_SETUP_VERSION;
        if (ca_pem_bytes) {
            /*
             * Add in additional certificate checking.
             * This list of enabled can be tuned for the specific
             * requirements - see 'man coap_encryption'.
             *
             * Note: A list of root ca file can be setup separately using
             * coap_context_set_pki_root_cas(), but the below is used to
             * define what checking actually takes place.
             */
            dtls_pki.verify_peer_cert        = 1;
            dtls_pki.require_peer_cert       = 1;
            dtls_pki.allow_self_signed       = 1;
            dtls_pki.allow_expired_certs     = 1;
            dtls_pki.cert_chain_validation   = 1;
            dtls_pki.cert_chain_verify_depth = 2;
            dtls_pki.check_cert_revocation   = 1;
            dtls_pki.allow_no_crl            = 1;
            dtls_pki.allow_expired_crl       = 1;
            dtls_pki.allow_bad_md_hash       = 1;
            dtls_pki.allow_short_rsa_length  = 1;
            dtls_pki.validate_cn_call_back   = verify_cn_callback;
            dtls_pki.cn_call_back_arg        = NULL;
            dtls_pki.validate_sni_call_back  = NULL;
            dtls_pki.sni_call_back_arg       = NULL;
        }
        dtls_pki.pki_key.key_type = COAP_PKI_KEY_PEM_BUF;
        dtls_pki.pki_key.key.pem_buf.public_cert = server_crt_start;
        dtls_pki.pki_key.key.pem_buf.public_cert_len = server_crt_bytes;
        dtls_pki.pki_key.key.pem_buf.private_key = server_key_start;
        dtls_pki.pki_key.key.pem_buf.private_key_len = server_key_bytes;
        dtls_pki.pki_key.key.pem_buf.ca_cert = ca_pem_start;
        dtls_pki.pki_key.key.pem_buf.ca_cert_len = ca_pem_bytes;

        coap_context_set_pki(ctx, &dtls_pki);
#endif /* CONFIG_COAP_MBEDTLS_PKI */
        // TODO later on enforce DTLS only - remove the two insecure endpoints here
        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);
        if (!ep) {
            ESP_LOGE(TAG, "udp: coap_new_endpoint() failed");
            goto clean_up;
        }
        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_TCP);
        if (!ep) {
            ESP_LOGE(TAG, "tcp: coap_new_endpoint() failed");
            goto clean_up;
        }
#if defined(CONFIG_COAP_MBEDTLS_PSK) || defined(CONFIG_COAP_MBEDTLS_PKI)
        if (coap_dtls_is_supported()) {
#ifndef CONFIG_MBEDTLS_TLS_SERVER
            /* This is not critical as unencrypted support is still available */
            ESP_LOGI(TAG, "MbedTLS (D)TLS Server Mode not configured");
#else /* CONFIG_MBEDTLS_TLS_SERVER */
            serv_addr.addr.sin.sin_port = htons(COAPS_DEFAULT_PORT);
            ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_DTLS);
            if (!ep) {
                ESP_LOGE(TAG, "dtls: coap_new_endpoint() failed");
                goto clean_up;
            }
            ESP_LOGI(TAG, "MbedTLS (D)TLS Server Mode configured");
#endif /* CONFIG_MBEDTLS_TLS_SERVER */
        } else {
            /* This is not critical as unencrypted support is still available */
            ESP_LOGI(TAG, "MbedTLS (D)TLS Server Mode not configured");
        }
#endif /* CONFIG_COAP_MBEDTLS_PSK CONFIG_COAP_MBEDTLS_PKI */
        
        handlers_t *handlers = malloc(sizeof(handlers_t));
        set_endpoints_handlers(handlers);
	
        endpoint x;
        for( x = 0; x < endpoint_size; x++ ){
            ESP_LOGI(TAG,"ENDPOINT STRING IS: %s",ENDPOINT_STRING[x]);
            coap_str_const_t *endpoint_string = &(coap_str_const_t){strlen(ENDPOINT_STRING[x]),(const uint8_t *)ENDPOINT_STRING[x]};

            ESP_LOGI(TAG, "Setting up CoAP endpoint:%s, length: %d",
                    endpoint_string->s, endpoint_string->length);
            
            resource = coap_resource_init(endpoint_string, 0);
            if (!resource) {
                ESP_LOGE(TAG, "coap_resource_init() failed");
                goto clean_up;
            }

            if (handlers->get_handlers[x] == NULL) ESP_LOGE(TAG,"get handler for endpoint:%s is null",ENDPOINT_STRING[x]);

            coap_register_handler(resource, COAP_REQUEST_GET, handlers->get_handlers[x]);
            coap_register_handler(resource, COAP_REQUEST_PUT, handlers->put_handlers[x]);
            coap_register_handler(resource, COAP_REQUEST_DELETE, handlers->del_handlers[x]);
            /* We possibly want to Observe the GETs */
            coap_resource_set_get_observable(resource, 1);
            coap_add_resource(ctx, resource);

            ESP_LOGI(TAG, "CoAP resource added:%s, length: %d",
                    room_name, strlen(room_name));
            
        }

        wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;
        while (1) {
            int result = coap_run_once(ctx, wait_ms);
            if (result < 0) {
                break;
            } else if (result && (unsigned)result < wait_ms) {
                /* decrement if there is a result wait time returned */
                wait_ms -= result;
            }
            if (result) {
                /* result must have been >= wait_ms, so reset wait_ms */
                wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;
            }
        }
    }
clean_up:
    coap_free_context(ctx);
    coap_cleanup();

    vTaskDelete(NULL);
}

/**
 * @brief Handles NVS (non-volatile storage) for the session variables
 */
void nvs_storage_daemon(void *arg)
{
    BaseType_t core = xPortGetCoreID();
    ESP_LOGI(TAG, "running nvs on core %d",core);
    EventGroupHandle_t endpoint_events = get_endpoints_event_group();
    color_data color_storage;
    esp_err_t err;

    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Reading last session from NVS ... ");
        
        /* Read Color Data and Color Mode */
        // Value will default to 0, if not set yet in NVS
        err = nvs_get_i32(handle, "color_storage", &color_storage.all);
        switch (err) {
            case ESP_OK:
                ESP_LOGI(TAG, "Read color successfuly\n");
                rgb_data[COLOR_R_IDX] = color_storage.color.r;
                rgb_data[COLOR_G_IDX] = color_storage.color.g;
                rgb_data[COLOR_B_IDX] = color_storage.color.b;
                ctrl_mode = color_storage.color.mode;
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG, "The color values are not initialized yet! Mode is set to Audio.\n");
                ctrl_mode = audio;
                break;   
            default :
                ESP_LOGI(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
        }
        ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", ENDPOINT_STRING[mode], MODE_STRING[ctrl_mode]) );

        /* Read Settings */
        // Read and check required size
        size_t required_size;
        // This call sets required size
        nvs_get_blob(handle, "settings_storage", NULL, &required_size);
        if (required_size != sizeof(settings.settings_data)) {
            ESP_LOGE(TAG,"Invalid length for settings_storage: %d, correct len: %d",required_size, sizeof(settings.settings_data));
            err = ESP_ERR_NVS_NOT_FOUND;
        } else {
            // This call reads the string
            err = nvs_get_blob(handle, "settings_storage", settings.settings_data, &required_size);
        }

        switch (err) {
            case ESP_OK:
                ESP_LOGI(TAG, "Read settings successfuly\n");
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG, "The settings values will be initialized with defaults!\n");
                settings.settings_st.amp_min = SOUND_AMPLITUDE_MIN_TRESH;
                settings.settings_st.amp_max = SOUND_AMPLITUDE_MAX_TRESH;
                settings.settings_st.freq_b_start = BLUE_FREQ_START;
                settings.settings_st.freq_b_end = BLUE_FREQ_END;
                settings.settings_st.freq_g_end = GREEN_FREQ_END;
                settings.settings_st.freq_r_end = RED_FREQ_END;
                break;
            default :
                ESP_LOGI(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
        }
        
        /* Read Room Name */
        // This call sets required size
        nvs_get_str(handle, "name_storage", NULL, &required_size);
        if (required_size > MAX_LEN_ROOM_NAME) {
            ESP_LOGE(TAG,"Invalid length for name storage");
            err = ESP_ERR_NVS_NOT_FOUND;
        } else {
            // This call reads the string
            err = nvs_get_str(handle, "name_storage", room_name, &required_size);
        }
        
        switch (err) {
            case ESP_OK:
                ESP_LOGI(TAG, "Read room name successfuly\n");
                ESP_LOGI(TAG, "room: %s",room_name);
                ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", ENDPOINT_STRING[room], room_name) );
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG, "The room name value is not initialized yet!\n");
                break;
            default :
                ESP_LOGI(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
        }
    }
    nvs_close(handle);

    // Task loop
    for(;;) {
        // Waits for notifications
        EventBits_t xbit = xEventGroupWaitBits(endpoint_events, ALL_ENDPOINT_EVENTS, pdTRUE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "task has event bits decimal value: %d\n",xbit);
        err = nvs_open("storage", NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        } else {
            // Check updates
            if (xbit & E_RGB_BIT) {
                color_storage.color.r = rgb_data[COLOR_R_IDX];
                color_storage.color.g = rgb_data[COLOR_G_IDX];
                color_storage.color.b = rgb_data[COLOR_B_IDX];
            }
            if (xbit & E_MODE_BIT) {
                color_storage.color.mode = ctrl_mode;
            }
            if (xbit & (E_RGB_BIT | E_MODE_BIT)) {
                // Write
                ESP_LOGI(TAG, "Updating color + mode values in NVS ... ");
                err = nvs_set_i32(handle, "color_storage", color_storage.all);
                ESP_LOGI(TAG, "%s", ((err != ESP_OK) ? "Failed!\n" : "Done\n"));
            }
            
            if (xbit & E_NAME_BIT) {
                ESP_LOGI(TAG, "Updating room name string in NVS ... ");
                err = nvs_set_str(handle, "name_storage", room_name);
                ESP_LOGI(TAG, "%s", ((err != ESP_OK) ? "Failed!\n" : "Done\n"));
            }

            if (xbit & E_PREF_BIT) {
                ESP_LOGI(TAG, "Updating settings in NVS ... ");
                err = nvs_set_blob(handle, "settings_storage", settings.settings_data, sizeof(settings.settings_data));
                ESP_LOGI(TAG, "%s", ((err != ESP_OK) ? "Failed!\n" : "Done\n"));
            }

            ESP_LOGI(TAG, "Committing updates in NVS ... ");
            err = nvs_commit(handle);
            ESP_LOGI(TAG, "%s", ((err != ESP_OK) ? "Failed!\n" : "Done\n"));
        }
        nvs_close(handle);
    }
}

/**
 * @brief I2S ADC mic input
 */
void i2s_adc_audio_processing(void*arg)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN,
        .sample_rate =  EXAMPLE_I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .channel_format = EXAMPLE_I2S_FORMAT,
        .intr_alloc_flags = 0,
        .dma_buf_count = 2,
        .dma_buf_len = (EXAMPLE_I2S_READ_LEN/2),
        .use_apll = 1,
    };
    // Install and start i2s driver
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    // Init ADC pad
    i2s_set_adc_mode(I2S_ADC_UNIT, I2S_ADC_CHANNEL);

    size_t bytes_read;
    int i2s_read_len = EXAMPLE_I2S_READ_LEN;

    char* i2s_read_buff = (char*) calloc(i2s_read_len, sizeof(char));
    uint16_t* i2s_proc_buff;
    i2s_adc_enable(I2S_NUM_0);
   
#if DEBUG_MIC_INPUT
    FFT_PRECISION mag_max = 0;
    FFT_PRECISION mag_max_freq = 0;
    int64_t time_processing;
#endif
    short range = 0;
    int c, location_max = 0, location_min = 0;
    FFTTransformer * transformer = create_fft_transformer((EXAMPLE_I2S_READ_LEN/2), FFT_SCALED_OUTPUT);
    FFT_PRECISION * fft_input = (FFT_PRECISION *) malloc((EXAMPLE_I2S_READ_LEN/2)  * sizeof(FFT_PRECISION));
    FFT_PRECISION * rgb_magnitudes;
    // Task loop
    for (;;) {
        if (ctrl_mode == manual) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
#if DEBUG_MIC_INPUT
        ESP_LOGI(TAG,"Color mode %d", ctrl_mode);
        time_processing = esp_timer_get_time();
#endif
        rgb_magnitudes = (FFT_PRECISION*)calloc(3, sizeof(FFT_PRECISION));
        i2s_read(I2S_NUM_0, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
        i2s_proc_buff = (uint16_t*) i2s_read_buff;

        location_max = 0;
        location_min = 0;
        for (c = 1; c < (bytes_read / 2); c++) {
            if (i2s_proc_buff[c] > i2s_proc_buff[location_max])
                location_max = c;
            if (i2s_proc_buff[c] < i2s_proc_buff[location_min])
                location_min = c;
        }

        range = i2s_proc_buff[location_max] - i2s_proc_buff[location_min];
        if ((range/16) == 0) continue;

#if DEBUG_MIC_INPUT
        printf("Max: buff[%04d] = %04d.\n", location_max+1, i2s_proc_buff[location_max]);
        printf("Min: buff[%04d] = %04d.\n", location_min+1, i2s_proc_buff[location_min]);
        if (i2s_proc_buff[location_min] == 0)
            ESP_LOGE(TAG, "Mic saturated");
        printf("Range: %d\n====\n", range);
#endif
        if (ctrl_mode == audio || ctrl_mode == audio_freq) {
            for(int i = 0; i < (EXAMPLE_I2S_READ_LEN/2); i += 1) {
                fft_input[i] = (i2s_proc_buff[i] - range) / (range/16);
            }

            // Transform signal
            fft_forward(transformer, fft_input);
#if DEBUG_MIC_INPUT
            ESP_LOGI(TAG,"Calculated FFT transform");
            mag_max = 0;
            mag_max_freq = 0;
#endif
            // Process output
            for(int i = 0; i < (EXAMPLE_I2S_READ_LEN/2); i += 2) {
                uint16_t freq = 16 * i / 2 * EXAMPLE_I2S_SAMPLE_RATE / EXAMPLE_I2S_READ_LEN;
                FFT_PRECISION cos_comp = fft_input[i];
                FFT_PRECISION sin_comp = fft_input[i+1];
                FFT_PRECISION mag = sqrt((cos_comp * cos_comp) + (sin_comp * sin_comp));
#if DEBUG_MIC_INPUT
                if (mag > mag_max && freq > 0 && freq < (EXAMPLE_I2S_SAMPLE_RATE/2)) {
                    mag_max = mag;
                    mag_max_freq = freq;
                }
#endif
                if (settings.settings_st.freq_b_start < freq 
                && freq < settings.settings_st.freq_b_end) {
                    rgb_magnitudes[COLOR_B_IDX] += mag;
                } else if (settings.settings_st.freq_b_end < freq 
                && freq < settings.settings_st.freq_g_end) {
                    rgb_magnitudes[COLOR_G_IDX] += mag;
                } else if (settings.settings_st.freq_g_end < freq 
                && freq < settings.settings_st.freq_r_end) {
                    rgb_magnitudes[COLOR_R_IDX] += mag;
                }
            }

            location_max = 0;
            for (c = 1; c < 3; c++) {
                if (rgb_magnitudes[c] > rgb_magnitudes[location_max])
                    location_max = c;
            }

            rgb_data[COLOR_R_IDX] = (uint8_t)(255*rgb_magnitudes[COLOR_R_IDX]/rgb_magnitudes[location_max]);
            rgb_data[COLOR_G_IDX] = (uint8_t)(255*rgb_magnitudes[COLOR_G_IDX]/rgb_magnitudes[location_max]);
            rgb_data[COLOR_B_IDX] = (uint8_t)(255*rgb_magnitudes[COLOR_B_IDX]/rgb_magnitudes[location_max]);
        
        }
        
        if (ctrl_mode == audio || ctrl_mode == audio_intensity) {
            range -= settings.settings_st.amp_min;
            if (range < 0) range = 0;

            set_rgb(rgb_data[COLOR_R_IDX], rgb_data[COLOR_G_IDX], rgb_data[COLOR_B_IDX], 
                range > settings.settings_st.amp_max ? 100 : (100*range/settings.settings_st.amp_max));
        } else {
             set_rgb(rgb_data[COLOR_R_IDX], rgb_data[COLOR_G_IDX], rgb_data[COLOR_B_IDX], 100);
        }
        free(rgb_magnitudes);

        
#if DEBUG_MIC_INPUT
        printf("Max magnitude:%lf at frequency:%lf Hz\n", mag_max, mag_max_freq);
        time_processing = (esp_timer_get_time() - time_processing) / 1000;
        ESP_LOGI(TAG, "bytes read: %d, time spent processing + vTaskDelay: %lld ms", bytes_read, (time_processing + 1));
#endif
        bytes_read = 0;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    i2s_adc_disable(I2S_NUM_0);
    free(i2s_read_buff);
    free_fft_transformer(transformer);
    i2s_read_buff = NULL;
    vTaskDelete(NULL);
}

void app_main(void)
{    
    //Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    wifi_init_sta();

    xTaskCreatePinnedToCore(nvs_storage_daemon, "nvs_storage_daemon", 4096, NULL, 5, NULL, CORE_1);
    xTaskCreatePinnedToCore(coap_server, "coap_server", 8 * 1024, NULL, 5, NULL, CORE_1);
    xTaskCreatePinnedToCore(i2s_adc_audio_processing, "i2s_adc_audio_processing", 4096, NULL, 5, NULL, CORE_0);

    mcpwm_gpio_init_config();
}
