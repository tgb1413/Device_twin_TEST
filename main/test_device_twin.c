#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothub_message.h"
#include "iothub_client_ll.h"
#include "iothubtransportmqtt.h"


#define EXAMPLE_IOTHUB_CONNECTION_STRING CONFIG_IOTHUB_CONNECTION_STRING

typedef struct EVENT_INSTANCE_TAG {
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId;
} EVENT_INSTANCE;

typedef struct MESSAGE_TAG
{
    int outlet;
    bool state;
}Message;

const char* connection_string = EXAMPLE_IOTHUB_CONNECTION_STRING;
static int iterator; 
static int callbackCounter;
static char msg_text[1024];

/**
 * 
 *  --- ABOUT MQTT ---
 * 
 * Define typedef struct EVENT_INSTANCE
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
 * Define serializeToJson()
 * - Serialize to JSON
 * 
 * Define parseFromJson()
 * - Parse From JSON
 * 
 * Define deviceTwinCallback()
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
static char* serializeToJson(Message* param)
{

}

static Message* parseFromJson(const char* json, DEVICE_TWIN_UPDATE_STATE update_state)
{

}

static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char* payload, size_t size, void* userContextCallback)
{
    Message* old_msg = userContextCallback;
    Message* new_msg = parseFromJson((const char*)payload, update_state);
}

static void reportedStateCallback() 
{
    printf("Send reported properties to IoT Hub");
}

void device_twin_test_run()
{
    IOTHUB_CLIENT_LL_HANDLE iothub_client_handle;

    EVENT_INSTANCE message;

    callbackCounter = 0;
    
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
            Message cmsg;
            // Set the reported properties

            IoTHubClient_LL_SetDeviceTwinCallback(iothub_client_handle, deviceTwinCallback, &cmsg);
            // IoTHubClient_LL_SetMessageCallback(iothub_client_handle, receiveMessageCallback, NULL);


        }
    }
}