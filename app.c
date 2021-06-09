/*******************************************************************************
  MPLAB Harmony Application Source File
  
  Company:
    Microchip Technology Inc.
  
  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It 
    implements the logic of the application's state machine and it may call 
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2013-2014 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END


// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "system_definitions.h"
#include "app.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

/* Recieve data buffer */
uint8_t receiveDataBuffer[64] APP_MAKE_BUFFER_DMA_READY;

/* Transmit data buffer */
uint8_t  transmitDataBuffer[64] APP_MAKE_BUFFER_DMA_READY;

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;
int dato1, dato2, respuesta;
unsigned char dato1Buffer[4]={0,0,0,0};
unsigned char dato2Buffer[4]={0,0,0,0};
unsigned char respuestaBuffer[4]={0,0,0,0};
int *dato2BufferPtr=NULL;
float *respuestafPtr=NULL;
float *datof1Ptr, *datof2Ptr, respuestaf;
float datof1, datof2, respuestaf;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

USB_DEVICE_HID_EVENT_RESPONSE APP_USBDeviceHIDEventHandler
(
    USB_DEVICE_HID_INDEX iHID,
    USB_DEVICE_HID_EVENT event,
    void * eventData,
    uintptr_t userData
)
{
    USB_DEVICE_HID_EVENT_DATA_REPORT_SENT * reportSent;
    USB_DEVICE_HID_EVENT_DATA_REPORT_RECEIVED * reportReceived;

    /* Check type of event */
    switch (event)
    {
        case USB_DEVICE_HID_EVENT_REPORT_SENT:

            /* The eventData parameter will be USB_DEVICE_HID_EVENT_REPORT_SENT
             * pointer type containing details about the report that was
             * sent. */
            reportSent = (USB_DEVICE_HID_EVENT_DATA_REPORT_SENT *) eventData;
            if(reportSent->handle == appData.txTransferHandle )
            {
                // Transfer progressed.
                appData.hidDataTransmitted = true;
            }
            
            break;

        case USB_DEVICE_HID_EVENT_REPORT_RECEIVED:

            /* The eventData parameter will be USB_DEVICE_HID_EVENT_REPORT_RECEIVED
             * pointer type containing details about the report that was
             * received. */

            reportReceived = (USB_DEVICE_HID_EVENT_DATA_REPORT_RECEIVED *) eventData;
            if(reportReceived->handle == appData.rxTransferHandle )
            {
                // Transfer progressed.
                appData.hidDataReceived = true;
            }
          
            break;

        case USB_DEVICE_HID_EVENT_SET_IDLE:

            /* For now we just accept this request as is. We acknowledge
             * this request using the USB_DEVICE_HID_ControlStatus()
             * function with a USB_DEVICE_CONTROL_STATUS_OK flag */

            USB_DEVICE_ControlStatus(appData.usbDevHandle, USB_DEVICE_CONTROL_STATUS_OK);

            /* Save Idle rate recieved from Host */
            appData.idleRate = ((USB_DEVICE_HID_EVENT_DATA_SET_IDLE*)eventData)->duration;
            break;

        case USB_DEVICE_HID_EVENT_GET_IDLE:

            /* Host is requesting for Idle rate. Now send the Idle rate */
            USB_DEVICE_ControlSend(appData.usbDevHandle, & (appData.idleRate),1);

            /* On successfully reciveing Idle rate, the Host would acknowledge back with a
               Zero Length packet. The HID function drvier returns an event
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT to the application upon
               receiving this Zero Length packet from Host.
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT event indicates this control transfer
               event is complete */

            break;
        default:
            // Nothing to do.
            break;
    }
    return USB_DEVICE_HID_EVENT_RESPONSE_NONE;
}

void APP_USBDeviceEventHandler(USB_DEVICE_EVENT event, void * eventData, uintptr_t context)
{
    switch(event)
    {
        case USB_DEVICE_EVENT_RESET:
        case USB_DEVICE_EVENT_DECONFIGURED:

            /* Host has de configured the device or a bus reset has happened.
             * Device layer is going to de-initialize all function drivers.
             * Hence close handles to all function drivers (Only if they are
             * opened previously. */

            BSP_LEDOn  (APP_USB_LED_1);
            BSP_LEDOn  (APP_USB_LED_2);
            BSP_LEDOff (APP_USB_LED_3);
            appData.deviceConfigured = false;
            appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            break;

        case USB_DEVICE_EVENT_CONFIGURED:
            /* Set the flag indicating device is configured. */
            appData.deviceConfigured = true;

            /* Save the other details for later use. */
            appData.configurationValue = ((USB_DEVICE_EVENT_DATA_CONFIGURED*)eventData)->configurationValue;

            /* Register application HID event handler */
            USB_DEVICE_HID_EventHandlerSet(USB_DEVICE_HID_INDEX_0, APP_USBDeviceHIDEventHandler, (uintptr_t)&appData);

            /* Update the LEDs */
            BSP_LEDOff (APP_USB_LED_1);
            BSP_LEDOff (APP_USB_LED_2);
            BSP_LEDOn  (APP_USB_LED_3);

            break;

        case USB_DEVICE_EVENT_SUSPENDED:

            /* Switch on green and orange, switch off red */
            BSP_LEDOff (APP_USB_LED_1);
            BSP_LEDOn  (APP_USB_LED_2);
            BSP_LEDOn  (APP_USB_LED_3);
            break;

        case USB_DEVICE_EVENT_POWER_DETECTED:

            /* VBUS was detected. We can attach the device */

            USB_DEVICE_Attach (appData.usbDevHandle);
            break;

        case USB_DEVICE_EVENT_POWER_REMOVED:

            /* VBUS is not available */
            USB_DEVICE_Detach(appData.usbDevHandle);
            break;

        /* These events are not used in this demo */
        case USB_DEVICE_EVENT_RESUMED:
        case USB_DEVICE_EVENT_ERROR:
        default:
            break;
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary local functions.
*/


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;
    
    appData.usbDevHandle = USB_DEVICE_HANDLE_INVALID;
    appData.deviceConfigured = false;
    appData.txTransferHandle = USB_DEVICE_HID_TRANSFER_HANDLE_INVALID;
    appData.rxTransferHandle = USB_DEVICE_HID_TRANSFER_HANDLE_INVALID;
    appData.hidDataReceived = false;
    appData.hidDataTransmitted = true;
    appData.receiveDataBuffer = &receiveDataBuffer[0];
    appData.transmitDataBuffer = &transmitDataBuffer[0];
}


/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

float matrixA[8][8], matrixB[8][8], matrixC[8][8];
int rowA = 0, rowB = 0, rowC = 0;
int i, j, k;
int *intPtr;

void APP_Tasks (void )
{
    /* Check if device is configured.  See if it is configured with correct
     * configuration value  */

    switch(appData.state)
    {
        case APP_STATE_INIT:

            /* Open the device layer */
            appData.usbDevHandle = USB_DEVICE_Open( USB_DEVICE_INDEX_0, DRV_IO_INTENT_READWRITE );

            if(appData.usbDevHandle != USB_DEVICE_HANDLE_INVALID)
            {
                /* Register a callback with device layer to get event notification (for end point 0) */
                USB_DEVICE_EventHandlerSet(appData.usbDevHandle, APP_USBDeviceEventHandler, 0);

                appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            }
            else
            {
                /* The Device Layer is not ready to be opened. We should try
                 * again later. */
            }

            break;

        case APP_STATE_WAIT_FOR_CONFIGURATION:

            if(appData.deviceConfigured == true)
            {
                /* Device is ready to run the main task */
                appData.hidDataReceived = false;
                appData.hidDataTransmitted = true;
                appData.state = APP_STATE_MAIN_TASK;

                /* Place a new read request. */
                USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                        &appData.rxTransferHandle, appData.receiveDataBuffer, 64);
            }
            break;

        case APP_STATE_MAIN_TASK:

            if(!appData.deviceConfigured)
            {
                /* Device is not configured */
                appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            }
            else if( appData.hidDataReceived )
            {
                /* Look at the data the host sent, to see what
                 * kind of application specific command it sent. */

                switch(appData.receiveDataBuffer[0])
                {
                    case 0x01:
                                if (appData.receiveDataBuffer[1]==1)
                                    BSP_LEDOn(APP_USB_LED_1);
                                else
                                    BSP_LEDOff(APP_USB_LED_1);
                                appData.hidDataReceived = false;
                                USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                            &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                                appData.state = APP_STATE_MAIN_TASK;
                                break;
                    case 0x02:
                                if (appData.receiveDataBuffer[1]==1)
                                    BSP_LEDOn(APP_USB_LED_2);
                                else
                                    BSP_LEDOff(APP_USB_LED_2);
                                appData.hidDataReceived = false;
                                USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                            &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                                appData.state = APP_STATE_MAIN_TASK;
                                break;
                    case 0x03:
                                if (appData.receiveDataBuffer[1]==1)
                                    BSP_LEDOn(APP_USB_LED_3);
                                else
                                    BSP_LEDOff(APP_USB_LED_3);
                                
                        /* Toggle on board LED1 to LED2. */
                        //BSP_LEDToggle( APP_USB_LED_1 );
                        //BSP_LEDToggle( APP_USB_LED_2 );
                        //BSP_LEDToggle( APP_USB_LED_3 );

                        appData.hidDataReceived = false;

                        /* Place a new read request. */
                        USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                        
                        appData.state = APP_STATE_MAIN_TASK;

                        break;
                    case 0x80:

                        /* Toggle on board LED3. */
                        BSP_LEDToggle( APP_USB_LED_3 );

                        appData.hidDataReceived = false;

                        /* Place a new read request. */
                        USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );

                        appData.state = APP_STATE_MAIN_TASK;        
                        
                        break;

                    case 0x81:

                        if(appData.hidDataTransmitted)
                        {
                            /* Echo back to the host PC the command we are fulfilling in
                             * the first byte.  In this case, the Get Push-button State
                             * command. */

                            appData.transmitDataBuffer[0] = 0x81;
                            
                            appData.transmitDataBuffer[1] = 0x00;
                            if( BSP_SwitchStateGet(APP_USB_SWITCH_1) == BSP_SWITCH_STATE_RELEASED ) {
                                appData.transmitDataBuffer[1] = 0x01;   //0b00000001
                            }
                            if( BSP_SwitchStateGet(APP_USB_SWITCH_2) == BSP_SWITCH_STATE_RELEASED ) {
                                appData.transmitDataBuffer[1] |= 0x02;  //0b00000010
                            }
                            if( BSP_SwitchStateGet(APP_USB_SWITCH_3) == BSP_SWITCH_STATE_RELEASED ) {
                                appData.transmitDataBuffer[1] |= 0x04;  //0b00000100
                            }
                            
                            appData.hidDataTransmitted = false;

                            /* Prepare the USB module to send the data packet to the host */
                            USB_DEVICE_HID_ReportSend (USB_DEVICE_HID_INDEX_0,
                                    &appData.txTransferHandle, appData.transmitDataBuffer, 64 );    //Cargar un EP_IN con los datos que la computadora va a leer

                            appData.hidDataReceived = false;

                            /* Place a new read request. */
                            USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                    &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );     //Re-armar el EP_OUT
                        }
                        
                        appData.state = APP_STATE_MAIN_TASK;        

                        break;
                        
                    case 0x82:
                        if (appData.hidDataTransmitted) {   //Validar que el EP_IN puede recibir informaci?n que eventualmente ser? le?da por la computadora
                            appData.transmitDataBuffer[0]=0x82;
                            appData.transmitDataBuffer[1]='A';
                            appData.transmitDataBuffer[2]='0';
                            appData.transmitDataBuffer[3]='1';
                            appData.transmitDataBuffer[4]='1';
                            appData.transmitDataBuffer[5]='9';
                            appData.transmitDataBuffer[6]='7';
                            appData.transmitDataBuffer[7]='0';
                            appData.transmitDataBuffer[8]='4';
                            appData.transmitDataBuffer[9]='4';
                            appData.transmitDataBuffer[10]=' ';
                            appData.transmitDataBuffer[11]='A';
                            appData.transmitDataBuffer[12]='0';
                            appData.transmitDataBuffer[13]='1';
                            appData.transmitDataBuffer[14]='6';
                            appData.transmitDataBuffer[15]='3';
                            appData.transmitDataBuffer[16]='5';
                            appData.transmitDataBuffer[17]='3';
                            appData.transmitDataBuffer[18]='4';
                            appData.transmitDataBuffer[19]='6';
                            appData.transmitDataBuffer[20]=0;
                            appData.hidDataTransmitted=false;
                            USB_DEVICE_HID_ReportSend (USB_DEVICE_HID_INDEX_0,
                                    &appData.txTransferHandle, appData.transmitDataBuffer, 64 );    //Cargar un EP_IN con los datos que la computadora va a leer
                            appData.hidDataReceived = false;
                            USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                    &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );     //Re-armar el EP_OUT
                        }
                        appData.state = APP_STATE_MAIN_TASK;        
                        break;

                    case 0x83:
                        if (appData.hidDataTransmitted) {   //Validar que el EP_IN puede recibir informaci?n que eventualmente ser? le?da por la computadora
                            appData.transmitDataBuffer[0]=0x83;
//                            appData.transmitDataBuffer[1]=(dato2 & 0x0FF);      //b7-b0
//                            appData.transmitDataBuffer[2]=((dato2>>8) & 0x0FF); //b15-b8
//                            appData.transmitDataBuffer[3]=((dato2>>16) & 0x0FF);//b23-b16
//                            appData.transmitDataBuffer[4]=((dato2>>24) & 0x0FF);//b31-b24
                            dato2BufferPtr=(int *)&(dato2Buffer[0]);
                            *dato2BufferPtr=131;
                            appData.transmitDataBuffer[1]=dato2Buffer[0];//b7-b0
                            appData.transmitDataBuffer[2]=dato2Buffer[1];//b15-b8
                            appData.transmitDataBuffer[3]=dato2Buffer[2];//b23-b16
                            appData.transmitDataBuffer[4]=dato2Buffer[3];//b31-b24
                            appData.transmitDataBuffer[5]=0;
                            appData.hidDataTransmitted=false;
                            USB_DEVICE_HID_ReportSend (USB_DEVICE_HID_INDEX_0,
                                    &appData.txTransferHandle, appData.transmitDataBuffer, 64 );    //Cargar un EP_IN con los datos que la computadora va a leer
                            appData.hidDataReceived = false;
                            USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                    &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );     //Re-armar el EP_OUT
                        }
                        appData.state = APP_STATE_MAIN_TASK;        
                        break;
                    case 0x10:  //Suma [0] -->0x10
                                //     [1] -->dato1[b7-b0]
                                //     [2] -->dato1[b15-b8]
                                //     [3] -->dato1[b23-b16]
                                //     [4] -->dato1[b31-b24]
                                //     [5] -->dato2[b7-b0]
                                //     [6] -->dato2[b15-b8]
                                //     [7] -->dato2[b23-b16]
                                //     [8] -->dato2[b31-b24]
                        if (appData.hidDataTransmitted) {   //Validar que el EP_IN puede recibir informaci?n que eventualmente ser? le?da por la computadora
                            appData.transmitDataBuffer[0]=0x10;
                            //----Obtener los operandos que han sido recibidos en el EP_OUT
                            dato1 = 0;
                            dato1 |= appData.receiveDataBuffer[4];  //b31-b24
                            dato1 <<= 8;
                            dato1 |= appData.receiveDataBuffer[3];  //b23-b16
                            dato1 <<= 8;
                            dato1 |= appData.receiveDataBuffer[2];  //b15-b8
                            dato1 <<= 8;
                            dato1 |= appData.receiveDataBuffer[1];  //b7-b0
                            dato2 = 0;
                            dato2 |= appData.receiveDataBuffer[8];  //b31-b24
                            dato2 <<= 8;
                            dato2 |= appData.receiveDataBuffer[7];  //b23-b16
                            dato2 <<= 8;
                            dato2 |= appData.receiveDataBuffer[6];  //b15-b8
                            dato2 <<= 8;
                            dato2 |= appData.receiveDataBuffer[5];  //b7-b0
                            respuesta = dato1 + dato2;
                            //----
                            appData.transmitDataBuffer[1]=(respuesta & 0x0FF);      //b7-b0
                            appData.transmitDataBuffer[2]=((respuesta>>8) & 0x0FF); //b15-b8
                            appData.transmitDataBuffer[3]=((respuesta>>16) & 0x0FF);//b23-b16
                            appData.transmitDataBuffer[4]=((respuesta>>24) & 0x0FF);//b31-b24

                            USB_DEVICE_HID_ReportSend (USB_DEVICE_HID_INDEX_0,
                                    &appData.txTransferHandle, appData.transmitDataBuffer, 64 );    //Cargar un EP_IN con los datos que la computadora va a leer
                            appData.hidDataReceived = false;
                            USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                    &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );     //Re-armar el EP_OUT
                        }
                        appData.state = APP_STATE_MAIN_TASK;        
                        break;
                    case 0x11:  //Resta
                        break;
                    case 0x12:  //Multiplicaci?n
                        break;
                    case 0x13:  //Divisi?n
                        break;
                    case 0x20:  //Suma [0] -->0x10
                                //     [1] -->dato1[b7-b0]
                                //     [2] -->dato1[b15-b8]
                                //     [3] -->dato1[b23-b16]
                                //     [4] -->dato1[b31-b24]
                                //     [5] -->dato2[b7-b0]
                                //     [6] -->dato2[b15-b8]
                                //     [7] -->dato2[b23-b16]
                                //     [8] -->dato2[b31-b24]
                        if (appData.hidDataTransmitted) {   //Validar que el EP_IN puede recibir informaci?n que eventualmente ser? le?da por la computadora
                            appData.transmitDataBuffer[0]=0x20;
                            //----Obtener los operandos que han sido recibidos en el EP_OUT
                            dato1Buffer[0]=appData.receiveDataBuffer[1];
                            dato1Buffer[1]=appData.receiveDataBuffer[2];
                            dato1Buffer[2]=appData.receiveDataBuffer[3];
                            dato1Buffer[3]=appData.receiveDataBuffer[4];
                            datof1Ptr=(float *)&dato1Buffer[0];
                            
                            dato2Buffer[0]=appData.receiveDataBuffer[5];
                            dato2Buffer[1]=appData.receiveDataBuffer[6];
                            dato2Buffer[2]=appData.receiveDataBuffer[7];
                            dato2Buffer[3]=appData.receiveDataBuffer[8];
                            datof2Ptr=(float *)&dato2Buffer[0];
                            
                            datof1=*datof1Ptr;
                            datof2=*datof2Ptr;
                            respuestaf = datof1 + datof2;
                            respuestafPtr = (float *)&respuestaBuffer[0];
                            *respuestafPtr = respuestaf;
                           //----
                            appData.transmitDataBuffer[1]=respuestaBuffer[0];      //b7-b0
                            appData.transmitDataBuffer[2]=respuestaBuffer[1];      //b15-b8
                            appData.transmitDataBuffer[3]=respuestaBuffer[2];      //b23-b16
                            appData.transmitDataBuffer[4]=respuestaBuffer[3];      //b31-b24

                            USB_DEVICE_HID_ReportSend (USB_DEVICE_HID_INDEX_0,
                                    &appData.txTransferHandle, appData.transmitDataBuffer, 64 );    //Cargar un EP_IN con los datos que la computadora va a leer
                            appData.hidDataReceived = false;
                            USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                    &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );     //Re-armar el EP_OUT
                        }
                        appData.state = APP_STATE_MAIN_TASK;        
                        break;
                        
                    case 0x60:
                        rowA = appData.receiveDataBuffer[1];

                        for(j = 0; j < 8; j++){
                            //matrixA[rowA][j] = (appData.receiveDataBuffer[2 + j*4]) | (appData.receiveDataBuffer[3 + j*4] << 8) | (appData.receiveDataBuffer[4 + j*4] << 16) | (appData.receiveDataBuffer[5 + j*4] << 24);
                            memcpy(&matrixA[rowA][j],(float*)&appData.receiveDataBuffer[2 + j*4],4);
                            //intPtr = (int*)&appData.receiveDataBuffer[2 + j * 4];
                            //matrixA[rowA][j] = *intPtr;
                        }

                        appData.hidDataReceived = false;
                        USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                    &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                            
                        appData.state = APP_STATE_MAIN_TASK;
                        break;
                        
                    case 0x61:
                        rowB = appData.receiveDataBuffer[1];

                        for(j = 0; j < 8; j++){
                            //matrixB[rowB][j] = (appData.receiveDataBuffer[2 + j*4] << 24) | (appData.receiveDataBuffer[3 + j*4] << 16) | (appData.receiveDataBuffer[4 + j*4] << 8) | (appData.receiveDataBuffer[5 + j*4]);
                            memcpy(&matrixB[rowB][j],(float*)&appData.receiveDataBuffer[2 + j*4],4);
                            //intPtr = (int*)&appData.receiveDataBuffer[2 + j * 4];
                            //matrixB[rowB][j] = *intPtr;
                        }

                        appData.hidDataReceived = false;
                        USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                    &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                            
                        appData.state = APP_STATE_MAIN_TASK;
                        break;
                        
                    case 0x62:
                        
                        for (i = 0; i < 8; i++) {
                            for (j = 0; j < 8; j++) {
                                matrixC[i][j] = 0;
                                for (k = 0; k < 8; k++) {
                                    matrixC[i][j] += matrixA[i][k] * matrixB[k][j];
                                }
                            }
                        }

                        appData.hidDataReceived = false;
                        USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                    &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                            
                        appData.state = APP_STATE_MAIN_TASK;
                        
                        break;
                        
                    case 0x63:
                        if (appData.hidDataTransmitted) {
                            appData.transmitDataBuffer[0]= appData.receiveDataBuffer[0];
                            appData.transmitDataBuffer[1]= appData.receiveDataBuffer[1];
                            
                            rowC = appData.receiveDataBuffer[1];
                            
                                    BSP_LEDOn(APP_USB_LED_1);
                                    BSP_LEDOff(APP_USB_LED_2);
                                    BSP_LEDOff(APP_USB_LED_3);
                            
                            int j;
                            
                            for(j = 0; j < 8; j++){
                                /*
                                appData.transmitDataBuffer[2 + j*4] = (uint8_t)(matrixA[rowA][j] & 0xFF);
                                appData.transmitDataBuffer[3 + j*4] = (uint8_t)((matrixA[rowA][j] >> 8) & 0xFF);
                                appData.transmitDataBuffer[4 + j*4] = (uint8_t)((matrixA[rowA][j] >> 16) & 0xFF);
                                appData.transmitDataBuffer[5 + j*4] = (uint8_t)((matrixA[rowA][j] >> 24) & 0xFF);
                                */
                                memcpy(&appData.transmitDataBuffer[2 + j*4],(uint8_t*)(&matrixC[rowC][j]),4);
                                //intPtr = (int*)&transmitDataBuffer[3 + j * 4];
                                //*intPtr = matrixA[rowA][j];
                            }
                            
                            appData.hidDataTransmitted=false;

                            USB_DEVICE_HID_ReportSend (USB_DEVICE_HID_INDEX_0,
                                    &appData.txTransferHandle, appData.transmitDataBuffer, 64 );
                            
                                    BSP_LEDOff(APP_USB_LED_1);
                                    BSP_LEDOn(APP_USB_LED_2);
                                    BSP_LEDOff(APP_USB_LED_3);
                            
                            appData.hidDataReceived = false;
                            
                            USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                        &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                            
                                    BSP_LEDOn(APP_USB_LED_1);
                                    BSP_LEDOn(APP_USB_LED_2);
                                    BSP_LEDOn(APP_USB_LED_3);

                        }
                        appData.state = APP_STATE_MAIN_TASK;
                        break;
                        
                    case 0x70: //encriptar y regresar encriptado
                      
                    case 0x71: //desencriptar y regresar desencriptado
                        if (appData.hidDataTransmitted) {
                            appData.transmitDataBuffer[0]= appData.receiveDataBuffer[0];
                            appData.transmitDataBuffer[1]= appData.receiveDataBuffer[1]; //byte de la izquierda de la clave
                            appData.transmitDataBuffer[2]= appData.receiveDataBuffer[2]; //byte de la derecha de la clave
                            
                            appData.transmitDataBuffer[3]= appData.receiveDataBuffer[3] ^ appData.transmitDataBuffer[1];
                            appData.transmitDataBuffer[4]= appData.receiveDataBuffer[4] ^ appData.transmitDataBuffer[2];

                                    BSP_LEDOn(APP_USB_LED_1);
                                    BSP_LEDOff(APP_USB_LED_2);
                                    BSP_LEDOff(APP_USB_LED_3);
                            
                            
                            appData.hidDataTransmitted=false;

                            USB_DEVICE_HID_ReportSend (USB_DEVICE_HID_INDEX_0,
                                    &appData.txTransferHandle, appData.transmitDataBuffer, 64 );
                            
                                    BSP_LEDOff(APP_USB_LED_1);
                                    BSP_LEDOn(APP_USB_LED_2);
                                    BSP_LEDOff(APP_USB_LED_3);
                            
                            appData.hidDataReceived = false;
                            
                            USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                        &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                            
                                    BSP_LEDOn(APP_USB_LED_1);
                                    BSP_LEDOn(APP_USB_LED_2);
                                    BSP_LEDOn(APP_USB_LED_3);

                        }
                        appData.state = APP_STATE_MAIN_TASK;
                        break;
                                        
                    default:

                        appData.hidDataReceived = false;

                        /* Place a new read request. */
                        USB_DEVICE_HID_ReportReceive (USB_DEVICE_HID_INDEX_0,
                                &appData.rxTransferHandle, appData.receiveDataBuffer, 64 );
                        
                        appData.state = APP_STATE_MAIN_TASK;        

                        break;
                }
            }
        case APP_STATE_ERROR:
            break;
        default:
            break;
    }
}
 

/*******************************************************************************
 End of File
 */

