#include "lwm2m_client.h"
#include "lwm2m_client_objects.h"
#include "lwm2m_objects/light_control.h"

#define OBJ_COUNT (5)

uint8_t connected = 0;
lwm2m_object_t *obj_list[OBJ_COUNT];
lwm2m_client_data_t client_data;

/* callback to set the light state */
static void _state_handle(void *arg, bool state)
{
    int *led = (int *)arg;
    switch(*led) {
#ifdef LED0_ON
        case 0:
            state ? LED0_ON :  LED0_OFF;
            break;
#endif /* LED0_ON */
#ifdef LED1_ON
        case 1:
            state ? LED1_ON :  LED1_OFF;
            break;
#endif /* LED1_ON */
        default:
            printf("The LED %d does not exist\n", *led);
    }
}

/* callback to set the light dimmer value */
static void _dimmer_handle(void *arg, uint8_t value)
{
    uint8_t *num = (uint8_t *)arg;
    printf("Setting dimmer of LED %d to %d\n", *num, value);
}

#ifdef LED0_ON
int led0 = 0;
#endif /* LED0_ON */
#ifdef LED1_ON
int led1 = 1;
#endif /* LED1_ON */

/* create an array of parameters for light instances */
static light_ctrl_params_t params[] = {
#ifdef LED0_ON
    { .arg = &led0, .type = "LED 0", .state_handle = _state_handle,
      .dimmer_handle = _dimmer_handle },
#endif /* LED0_ON */
#ifdef LED1_ON
    { .arg = &led1, .type = "LED 1", .state_handle = _state_handle,
      .dimmer_handle = _dimmer_handle },
#endif /* LED1_ON */
};
#define LEDS_NUMOF (sizeof(params) / sizeof(params[0]))


void lwm2m_cli_init(void)
{
    /* this call is needed before creating any objects */
    lwm2m_client_init(&client_data);

    /* add objects that will be registered */
    obj_list[0] = lwm2m_client_get_security_object(&client_data);
    obj_list[1] = lwm2m_client_get_server_object(&client_data);
    obj_list[2] = lwm2m_client_get_device_object(&client_data);
    obj_list[3] = lwm2m_client_get_acc_ctrl_object(&client_data);
    obj_list[4] = object_light_control_get(params, LEDS_NUMOF);

    if (!obj_list[0] || !obj_list[1] || !obj_list[2]) {
        puts("Could not create mandatory objects");
    }
}

int lwm2m_cli_cmd(int argc, char **argv)
{
    if (argc == 1) {
        goto help_error;
    }

    if (!strcmp(argv[1], "start")) {
        /* run the LwM2M client */
        if (!connected && lwm2m_client_run(&client_data, obj_list, OBJ_COUNT)) {
            lwm2m_context_t *ctx = lwm2m_client_get_ctx(&client_data);
            /* enable the on time refresh for every instance */
            for (unsigned i = 0; i < LEDS_NUMOF; i++) {
                object_light_control_enable_ontime(ctx, obj_list[4], i);
            }
            connected = 1;
        }
        return 0;
    }

    if (!strcmp(argv[1], "light")) {
        if (argc == 3) {
            lwm2m_context_t *lwm2m_ctx = lwm2m_client_get_ctx(&client_data);
            uint16_t led = atoi(argv[2]);
            return object_light_control_toggle(lwm2m_ctx, obj_list[4], led);
        }
        else {
            printf("usage: %s %s <light_num>\n", argv[0], argv[1]);
            return 1;
        }
    }

help_error:
    printf("usage: %s <start|light>\n", argv[0]);
    return 1;
}