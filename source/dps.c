/******************************************************************************
* File Name:   dps.c
*
* Description: This file implements the interface with the Pressure sensor, as
*              a timer to feed the pre-processor at 50Hz.
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
#include "config.h"
#include "xensiv_dps3xx_mtb.h"
#include "dps.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define DPS_SCAN_RATE         50
#define DPS_TIMER_FREQUENCY   100000
#define DPS_TIMER_PERIOD      (DPS_TIMER_FREQUENCY/DPS_SCAN_RATE)
#define DPS_TIMER_PRIORITY    6
#ifdef IM_XSS_DPS368
#define DPS368_ADDRESS (XENSIV_DPS3XX_I2C_ADDR_ALT)
#else
#define DPS368_ADDRESS (XENSIV_DPS3XX_I2C_ADDR_DEFAULT)
#endif

/*******************************************************************************
* Global Variables
*******************************************************************************/
xensiv_dps3xx_t pressure_sensor;
/* timer used for getting data */
cyhal_timer_t dps_timer;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void dps_timer_intr_handler(void* callback_arg, cyhal_timer_event_t event);
cy_rslt_t dps_timer_init(void);


/*******************************************************************************
* Function Name: dps_init
********************************************************************************
* Summary:
*    A function used to initialize the DPS368 Pressure sensor. Starts a timer 
*    that triggers an interrupt at 50Hz.
*
* Parameters:
*   None
*
* Return:
*     The status of the initialization.
*
*
*******************************************************************************/
cy_rslt_t dps_init(void)
{
    cy_rslt_t result;
    xensiv_dps3xx_config_t config;
    /* Initialize pressure sensor */
    result = xensiv_dps3xx_mtb_init_i2c(&pressure_sensor, &i2c, DPS368_ADDRESS);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    result = xensiv_dps3xx_get_config(&pressure_sensor, &config);
    config.pressure_oversample = XENSIV_DPS3XX_OVERSAMPLE_16;
    config.pressure_rate = XENSIV_DPS3XX_RATE_16;
    result = xensiv_dps3xx_set_config(&pressure_sensor, &config);
    result = xensiv_dps3xx_get_config(&pressure_sensor, &config);
    config.temperature_oversample = XENSIV_DPS3XX_OVERSAMPLE_16;
    config.temperature_rate = XENSIV_DPS3XX_RATE_16;
    result = xensiv_dps3xx_set_config(&pressure_sensor, &config);

    dps_flag = false;
    /* Timer for data collection */
    result = dps_timer_init();
    if(CY_RSLT_SUCCESS != result)
    {
        return result;
    }
    return CY_RSLT_SUCCESS;
}


/*******************************************************************************
* Function Name: dps_timer_init
********************************************************************************
* Summary:
*   Sets up an interrupt that triggers at the desired frequency.
*
* Returns:
*   The status of the initialization.
*
*
*******************************************************************************/
cy_rslt_t dps_timer_init(void)
{
    cy_rslt_t rslt;
    const cyhal_timer_cfg_t timer_cfg =
    {
        .compare_value = 0,                 /* Timer compare value, not used */
        .period = DPS_TIMER_PERIOD,         /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = false,                /* Don't use compare mode */
        .is_continuous = true,              /* Run the timer indefinitely */
        .value = 0                          /* Initial value of counter */
    };

    /* Initialize the timer object. Does not use pin output ('pin' is NC) and
    * does not use a pre-configured clock source ('clk' is NULL). */
    rslt = cyhal_timer_init(&dps_timer, NC, NULL);
    if (CY_RSLT_SUCCESS != rslt)
    {
        return rslt;
    }

    /* Apply timer configuration such as period, count direction, run mode, etc. */
    rslt = cyhal_timer_configure(&dps_timer, &timer_cfg);
    if (CY_RSLT_SUCCESS != rslt)
    {
        return rslt;
    }

    /* Set the frequency of timer to 100KHz */
    rslt = cyhal_timer_set_frequency(&dps_timer, DPS_TIMER_FREQUENCY);
    if (CY_RSLT_SUCCESS != rslt)
    {
        return rslt;
    }

    /* Assign the ISR to execute on timer interrupt */
    cyhal_timer_register_callback(&dps_timer, dps_timer_intr_handler, NULL);

    /* Set the event on which timer interrupt occurs and enable it */
    cyhal_timer_enable_event(&dps_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT, DPS_TIMER_PRIORITY, true);
    
    /* Start the timer with the configured settings */
    rslt = cyhal_timer_start(&dps_timer);
    if (CY_RSLT_SUCCESS != rslt)
    {
        return rslt;
    }

    return CY_RSLT_SUCCESS;
}


/*******************************************************************************
* Function Name: dps_timer_intr_handler
********************************************************************************
* Summary:
*   Interrupt handler for timer. Interrupt handler will get called at 50Hz and
*   sets a flag that can be checked in main.
*
* Parameters:
*     callback_arg: not used
*     event: not used
*
*
*******************************************************************************/
void dps_timer_intr_handler(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;

    dps_flag = true;
}


/*******************************************************************************
* Function Name: dps_get_data
********************************************************************************
* Summary:
*   Reads data from the Pressure sensor and stores it in a buffer.
*
* Parameters:
*     dps_data: Stores Pressure sensor data
*
*
*******************************************************************************/
cy_rslt_t dps_get_data(float *dps_data)
{
    cy_rslt_t result;
    float pressure;
    float temperature;
    result = xensiv_dps3xx_read(&pressure_sensor, &pressure, &temperature);
    if (CY_RSLT_SUCCESS == result)
    {
        dps_data[0] = pressure;
        dps_data[1] = temperature;
    }
    return result;
}
