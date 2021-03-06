#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothub_message.h"
#include "iothub_client_ll.h"
#include "iothubtransportmqtt.h"
#include "driver/gpio.h"
#include "parson.h"

#define EXAMPLE_IOTHUB_CONNECTION_STRING CONFIG_IOTHUB_CONNECTION_STRING

typedef struct EVENT_INSTANCE_TAG {
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId;
} EVENT_INSTANCE;

typedef struct MESSAGE_TAG
{
    // 현재 상태와 다음 상태가 다른 지를 나타내기 위한 변수
    // 값이 -1이면 같음, index - 1 이면 다름.
    int output[4];
    int state[4];
}Message;

typedef struct EVENT_INSTANCE_AND_Message
{
    EVENT_INSTANCE event_i;
    Message m;
}DATA;

const char* connection_string = EXAMPLE_IOTHUB_CONNECTION_STRING;
static int iterator; 
static char msg_text[1024];

static DATA data;
static IOTHUB_CLIENT_LL_HANDLE iothub_client_handle;

static const char* TAG = "DeviceTest";

/**
 * 
 *  --- ABOUT MQTT ---
 * 
 * Define typedef struct EVENT_INSTANCE (O)
 * 
 * Define function ReceiveMessageCallback (Return type : IOTHUBMESSAGE_DISPOSITION_RESULT)
 * - It is called when message comes from IoT Hub
 * --This function searches three components
 * - 1. message id and correlation id of message
 * - 2. message content
 * - 3. all custom property of message
 * --This function returns 3 different options 
 * - 1. IOTHUBMESSAGE_ACCEPTED : Message handled successfully.
 *                               IoTHubClient library doesn't call callback function with same message.
 * - 2. IOTHUBMESSAGE_REJECTED : Message didn't handle and will not handle later.
 *                               IoTHubClient library doesn't call callback function with same message.
 * - 3. IOTHUBMESSAGE_ABANDONED : Message didn't handle successfully.
 *                                IoTHubClient library has to call callback function with same message.
 * 
 * Define function SendConfirmationCallback
 * - It is called whenever you send message
 * - If result is IOTHUB_CLIENT_CONFIRMATION_OK then print the detail of result and increase callback counter
 * - Free IOTHUB_MESSAGE_HANDLE (using 'IoTHubMessage_Destroy')
 * 
 * Define function iothub_client_sample_mqtt_run (Return type : void)
 * - Initialize platform (using 'platform_init')
 * - Allocate IoT Hub client handle (which is 'IoTHubClient_LL_CreateFromConnectionString')
 * - Set message callback (using 'IoTHubClient_ll_SetMessageCallback', 'ReceiveMessageCallback' in this case)
 * - Create Message (using 'IoTHubMessage_CreateFromByteArray')
 * - Set confirmation callback (using 'IoTHubClient_LL_SendEventAsync', 'SendConfirmationCallback' in this case)
 * - Do the work (using 'IoTHubClient_LL_DoWork')
 * 
 * LL API doesn't create Background thread.
 * Instead, you need to call a new API that explicitly sends and receives data from the IoT Hub.
 * (If you used LL API, then you have to keep using LL API. The non-LL API is also same.)
 * 
*/

/**
 * 
 *  --- ABOUT DEVICE TWIN ---
 * 
 * Define serializeToJson()
 * - Serialize to JSON
 * 
 * Define parseFromJson() (o)
 * - Parse From JSON
 * 
 * Define deviceTwinCallback() (o)
 * - It is called when desired properties are changed.
 * 
 * Define reportedStateCallback()
 * - It is called when reported properties are sent
 * 
 * Define iothub_device_twin_sample_run()
 * - Set IoTClient Handle
 * - Initialize platform (using 'IoTHub_init' or 'platform_init' / I don't know their difference)
 * - Allocate IoT Hub Client Handle (which is 'IoTHubClient_LL_CreateFromConnectionString')
 * - Set the reported properties
 * - Set Callback functions (reported state callback, device method callback, device twin callback)
 * - Do the work (using 'IoTHubClient_LL_DoWork')
 * 
*/

/**
 *  --- Example of GPIO ---
 * 
 * Example of GPIO config (o)
 * 1. Disable interrupt
 * 2. Set as output mode
 * 3. Bit mask of the pins that you want to set
 * 4. Disable pull-down mode
 * 5. Disable pull-up mode
 * 6. Configure GPIO with the given settings
 * 
 * Define void IRAM_ATTR gpio_isr_handler(void* arg)
 * 
 * Define void gpio_task_example(void* arg)
 * 
*/

#define GPIO_OUTPUT1 32
#define GPIO_OUTPUT2 33
#define GPIO_OUTPUT3 13
#define GPIO_OUTPUT4 27
#define GPIO_PIN_BITMASK ((1ULL<<GPIO_OUTPUT1)|(1ULL<<GPIO_OUTPUT2)|(1ULL<<GPIO_OUTPUT3)|(1ULL<<GPIO_OUTPUT4))

static char* serializeToJson(Message* param)
{
    char* result;

    JSON_Value* root_value = json_value_init_object();
    JSON_Object* root_object = json_value_get_object(root_value);

    if (param->state[0] == 1)
    {
        json_object_dotset_boolean(root_object, "outlet.1", true);
    }
    else
    {
        json_object_dotset_boolean(root_object, "outlet.1", false);
    }

    if (param->state[1] == 1)
    {
        json_object_dotset_boolean(root_object, "outlet.2", true);
    }
    else
    {
        json_object_dotset_boolean(root_object, "outlet.2", false);
    }

    if (param->state[2] == 1)
    {
        json_object_dotset_boolean(root_object, "outlet.3", true);
    }
    else
    {
        json_object_dotset_boolean(root_object, "outlet.3", false);
    }

    if (param->state[3] == 1)
    {
        json_object_dotset_boolean(root_object, "outlet.4", true);
    }
    else
    {
        json_object_dotset_boolean(root_object, "outlet.4", false);
    }
    json_object_set_value(root_object, "temperature", NULL);
    json_object_set_value(root_object, "humidity", NULL);
    
    result = json_serialize_to_string(root_value);
    json_value_free(root_value);

    return result;
}

static Message* parseFromJson(const char* json, DEVICE_TWIN_UPDATE_STATE update_state)
{
    JSON_Value* root_value = NULL;
    JSON_Object* root_object = NULL;
    Message* msg = malloc(sizeof(Message));

    if (msg == NULL)
    {
        printf("Failed to allocate memory to Message. \r\n");
    }
    else
    {
        root_value = json_parse_string(json);
        root_object = json_value_get_object(root_value);

        int state[4];
        if (update_state == DEVICE_TWIN_UPDATE_COMPLETE)
        {
            printf("first update\r\n");
            state[0] = json_object_dotget_boolean(root_object, "desired.outlet.1");
            state[1] = json_object_dotget_boolean(root_object, "desired.outlet.2");
            state[2] = json_object_dotget_boolean(root_object, "desired.outlet.3");
            state[3] = json_object_dotget_boolean(root_object, "desired.outlet.4");
        }
        else
        {
            printf("update\r\n");
            state[0] = json_object_dotget_boolean(root_object, "outlet.1");
            state[1] = json_object_dotget_boolean(root_object, "outlet.2");
            state[2] = json_object_dotget_boolean(root_object, "outlet.3");
            state[3] = json_object_dotget_boolean(root_object, "outlet.4");
        }

        for(int i = 0; i < sizeof(msg->state)/sizeof(int); i++)
        {
            msg->state[i] = state[i];
            printf("save index: %d value: %d\r\n",i,  msg->state[i]);
        }
        json_value_free(root_value);
    }
    return msg;    
}

static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    DATA* data = userContextCallback;
    if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
    {
        printf("Confirmation received with result = %s\r\n", ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
    }

    // printf("confirmationCounter : %d deviceTwinCounter : %d\r\n", confirmationCounter, deviceTwinCounter);
    // if (confirmationCounter == deviceTwinCounter)
    // {
    //     memset(data->m.output, -1, sizeof(data->m.output));
    //     IoTHubMessage_Destroy(data->event_i.messageHandle); ////////////////////////
    //     printf("destory\r\n");

    // }
}

static void reportedStateCallback(int status_code, void* userContextCallback) 
{
    printf("Send reported properties to IoT Hub. \r\n");
}

static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char* payload, size_t size, void* userContextCallback)
{
    DATA* data = userContextCallback;
    //Message* old_msg = &data->m;
    Message* new_msg = parseFromJson((const char*)payload, update_state);
    
    if (new_msg != NULL)
    {
        for (int i = 0; i < sizeof(new_msg->output)/sizeof(int); i++)
        {
            if(new_msg->state[i] != data->m.state[i])
            {
                printf("Desired outlet status changed.\r\n");
                if(new_msg->state[i]){
                    data->m.state[i] = 1;
                }else {
                    data->m.state[i] = 0;
                }
                data->m.output[i] = i;
            }
            else
            {
                data->m.output[i] = -1;
            }
            
        }

        free(new_msg);
    }
    else
    {
        printf("Parse from JSON failed \r\n");
    }

    if (iterator == 0) // boot
    {
        sprintf_s(msg_text, sizeof(msg_text), "{\"messageId\":%d, \"boot\": true}", iterator);
        char* reported_properties = serializeToJson(&data->m);
        if ((data->event_i.messageHandle
                    = IoTHubMessage_CreateFromByteArray((const unsigned char*)msg_text, strlen(msg_text))) == NULL)
        {
            printf("iothub message is null...\r\n");
        }
        else
        {
            if (IoTHubClient_LL_SendEventAsync(iothub_client_handle, data->event_i.messageHandle, sendConfirmationCallback, &data) != IOTHUB_CLIENT_OK)
            {
                printf("Failed to send a message to IOTHUB \r\n");
            }
            else
            {
                if(IoTHubClient_LL_SendReportedState(iothub_client_handle, (const unsigned char*)reported_properties, 
                                                strlen(reported_properties), reportedStateCallback, NULL)!=IOTHUB_CLIENT_OK){
                    printf("Failed to send a twin to IOTHUB \r\n");
                };
                printf("IoTHubClient_LL_SendEventAsync accepted boot message for transmission to IoT Hub.\r\n");

            }
            
        }
        iterator++;
    }
    else 
    {
        for (int i = 0; i < sizeof(data->m.output)/sizeof(int); i++)
        {
            if(data->m.output[i] != -1)
            {
                sprintf_s(msg_text, sizeof(msg_text), "{\"messageId\":%d, \"outlet\":%d, \"state\":", 
                        iterator, (data->m.output[i] + 1));
                if (data->m.state[i] == 1)
                {
                    strcat(msg_text, "true}");
                }
                else if (data->m.state[i] == 0)
                {
                    strcat(msg_text, "false}");
                }
            
                char* reported_properties = serializeToJson(&data->m);

                if ((data->event_i.messageHandle
                    = IoTHubMessage_CreateFromByteArray((const unsigned char*)msg_text, strlen(msg_text))) == NULL)
                {
                    printf("iothub message is null...\r\n");
                }
                else
                {
                    if (IoTHubClient_LL_SendEventAsync(iothub_client_handle, data->event_i.messageHandle, sendConfirmationCallback, &data) != IOTHUB_CLIENT_OK)
                    {
                        printf("Failed to send a message to IOTHUB \r\n");
                    }
                    else
                    {
                        IoTHubClient_LL_SendReportedState(iothub_client_handle, (const unsigned char*)reported_properties, 
                                                        strlen(reported_properties), reportedStateCallback, NULL);
                        printf("IoTHubClient_LL_SendEventAsync accepted message [%d] for transmission to IoT Hub.\r\n", (int)iterator);
                        
                        if (data->m.output[0] == 0 && data->m.state[0] == 1)
                        {
                            gpio_set_level(GPIO_OUTPUT1, 1);
                        }
                        else if (data->m.output[0] == 0 && data->m.state[0] == 0)
                        {
                            gpio_set_level(GPIO_OUTPUT1, 0);
                        }
                        if (data->m.output[1] == 1 && data->m.state[1] == 1)
                        {
                            gpio_set_level(GPIO_OUTPUT2, 1);
                        }
                        else if (data->m.output[1] == 1 && data->m.state[1] == 0)
                        {
                            gpio_set_level(GPIO_OUTPUT2, 0);
                        }
                        if (data->m.output[2] == 2 && data->m.state[2] == 1)
                        {
                            gpio_set_level(GPIO_OUTPUT3, 1);
                        }
                        else if (data->m.output[2] == 2 && data->m.state[2] == 0)
                        {
                            gpio_set_level(GPIO_OUTPUT3, 0);
                        }
                        if (data->m.output[3] == 3 && data->m.state[3] == 1)
                        {
                            gpio_set_level(GPIO_OUTPUT4, 1);
                        }
                        else if (data->m.output[3] == 3 && data->m.state[3] == 0)
                        {
                            gpio_set_level(GPIO_OUTPUT4, 0);
                        }
                    }
                    
                }
                
                iterator++;
                
            }
        }
    }
    
}

void device_twin_test_run()
{
    gpio_config_t io_config;
    
    io_config.intr_type = GPIO_PIN_INTR_DISABLE;
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = GPIO_PIN_BITMASK;
    io_config.pull_up_en = 0;
    io_config.pull_down_en = 0;
    gpio_config(&io_config);
    
    if (platform_init() != 0)
    {
        printf("Failed to initialize platform \r\n");
    }
    else
    {
        if ((iothub_client_handle = IoTHubClient_LL_CreateFromConnectionString(connection_string, MQTT_Protocol)) == NULL)
        {
            printf("IoTHub Client Handle is NULL");
        }
        else
        {
            
            memset(data.m.state, 0, sizeof(data.m.state));

            iterator = 0;

            IoTHubClient_LL_SetDeviceTwinCallback(iothub_client_handle, deviceTwinCallback, &data);
            
            while(1)
            {
                IoTHubClient_LL_DoWork(iothub_client_handle);
                ThreadAPI_Sleep(10);
                
                if (data.m.output[0] == 0 && data.m.state[0] == 1)
                {
                    gpio_set_level(GPIO_OUTPUT1, 1);
                }
                else if (data.m.output[0] == 0 && data.m.state[0] == 0)
                {
                    gpio_set_level(GPIO_OUTPUT1, 0);
                }
                if (data.m.output[1] == 1 && data.m.state[1] == 1)
                {
                    gpio_set_level(GPIO_OUTPUT2, 1);
                }
                else if (data.m.output[1] == 1 && data.m.state[1] == 0)
                {
                    gpio_set_level(GPIO_OUTPUT2, 0);
                }
                if (data.m.output[2] == 2 && data.m.state[2] == 1)
                {
                    gpio_set_level(GPIO_OUTPUT3, 1);
                }
                else if (data.m.output[2] == 2 && data.m.state[2] == 0)
                {
                    gpio_set_level(GPIO_OUTPUT3, 0);
                }
                if (data.m.output[3] == 3 && data.m.state[3] == 1)
                {
                    gpio_set_level(GPIO_OUTPUT4, 1);
                }
                else if (data.m.output[3] == 3 && data.m.state[3] == 0)
                {
                    gpio_set_level(GPIO_OUTPUT4, 0);
                }
            }
        }
    }
}