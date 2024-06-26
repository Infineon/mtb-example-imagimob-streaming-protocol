/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the Imagimob Streaming Protocol
*              Example for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "stdlib.h"
#include "config.h"
#include "audio.h"
#ifdef IM_ENABLE_IMU
  #include "imu.h"
#endif
#include "protocol.h"


/*******************************************************************************
* Global Variables
********************************************************************************/
volatile bool pdm_pcm_flag;
volatile bool imu_flag;


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  This is the main function. It sets up either the PDM or IMU based on the
*  config.h file. main continuously checks flags, signaling that data is ready
*  to be streamed over UART or USB and initiates the transfer.
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

#ifdef COMPONENT_USBD_BASE
    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    printf("\x1b[2J\x1b[;H");

    printf("*********** "
           "PSoC 6 MCU: Imagimob Streaming Protocol"
           "*********** \r\n\n");
#endif

    /* Initialize the User LED */
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* Initialize the streaming interface */
    streaming_init();

    /* Initialize protocol (start timer) */
    protocol_init();

    /* Initialize PDM transmit buffers */
    uint8_t transmit_pdm[2 * FRAME_SIZE] = {0};
    int16_t *pdm_raw_data = (int16_t *) transmit_pdm;

    /* Configure PDM, PDM clocks, and PDM event */
    result = pdm_init();

#ifdef IM_ENABLE_IMU
    /* Initialize IMU transmit buffers */
    uint8_t transmit_imu[4 * IMU_AXIS] = {0};
    float *imu_raw_data = (float*) transmit_imu;

    /* Start the imu and timer */
    result = imu_init();
#endif

    /* Initialization failed */
    if (CY_RSLT_SUCCESS != result)
    {
        /* Reset the system on sensor fail */
        NVIC_SystemReset();
    }

    for (;;)
    {
        /* Handle incoming characters */
        protocol_repl();

        /* Transmit IMU data and PDM data */
#if IM_ENABLE_IMU
        if (true == imu_flag)
        {
            imu_flag = false;
            /* Store IMU data */
            imu_get_data(imu_raw_data);
            /* Transmit data */
            protocol_send(PROTOCOL_IMU_CHANNEL, transmit_imu, sizeof(transmit_imu));
        }
#endif
        if (true == pdm_pcm_flag)
        {
            pdm_pcm_flag = false;
            /* Store PDM data */
            pdm_preprocessing_feed(pdm_raw_data);
            /* Transmit data */
            protocol_send(PROTOCOL_AUDIO_CHANNEL, transmit_pdm, sizeof(transmit_pdm));
        }
    }
}

/* [] END OF FILE */
