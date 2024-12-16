/*******************************************************************************
 * (c) Copyright 2012-2015 Microsemi SoC Products Group.  All rights reserved.
 *
 * This example program demonstrates the use of the SmartFusion2 eNVM driver by
 * programming various byte patterns into eNVM. This test program is intended to
 * be stepped through using a debugger to observe the changes of content of the
 * eNVM in address range 0x60001000 to 0x60001200 as the program executes. The
 * patterns written to NVM depend on the original content of the eNVM. Each
 * successive run of this program will produce an observable change in the
 * content of the eNVM.
 *
 * Please refer to file README.TXT in this project's folder for further details
 * about this example.
 *
 * SVN $Revision: 7790 $
 * SVN $Date: 2015-09-14 11:18:13 +0530 (Mon, 14 Sep 2015) $
 */

/*P
 *  eNVM_1 0x60040000 - 0x6007FFFF
    eNVM_0 0x60000000 - 0x6003FFFF
 * */
#include "drivers/mss_nvm/mss_nvm.h"
#include "drivers/mss_uart/mss_uart.h"
#include "drivers/mss_rtc/mss_rtc.h"
#include "drivers/mss_gpio/mss_gpio.h"
#include "CMSIS/system_m2sxxx.h"
#include "drivers/mss_timer/mss_timer.h"
#include "CMSIS/system_m2sxxx.h"
#include <stdio.h>


//#include "sm2_coreSpi.h"
//#include "core_pwm.h"


// RTC
#define RTC_PRESCALER    (32768u - 1u)
#define TICKS_SECOND                ( 2 )
#define HALF_A_SECOND               ( 32768 / TICKS_SECOND )
#define TICKS_MINUTE                ( 60 * TICKS_SECOND )


//Constants
#define LIMIT                   500//1968//Space => 256(2^10)/128 = 2048 - 64(not written first pages) = 1984 an the others do not work
#define VALUE                   0xAA//0xAA instead of 1 to see it clearly
#define NVM_PAGE_SIZE           128
//#define NVM_TEST_SIZE           LIMIT*128//5*128
//#define NVM_PAGE_SIZE_DOUBLE    NVM_PAGE_SIZE * 2       /* 256 bytes*/ Changed
#define BYTES_SIZE_260          (2 * NVM_PAGE_SIZE) + 4  /* 260 bytes */
#define BYTES_SIZE_258          (2 * NVM_PAGE_SIZE) + 2  /* 258 bytes */
#define BYTES_SIZE_224          (2 * NVM_PAGE_SIZE) - (NVM_PAGE_SIZE / 4)  /* 224 bytes */
#define BYTES_SIZE_192          (2 * NVM_PAGE_SIZE) - (NVM_PAGE_SIZE / 2)  /* 192 bytes */
//Addresses of the  eSRAM
#define NVM_ADDR                0x60002000u //Before address given in template, errors occur 0x60001000u
#define NVM_STOP_ADDR           0x6003FFFFu //From libero

/*
 * First 64 not written
 * From the start 2000 in dec=8192 => 64pag*128bit
 */
struct time {

    uint8_t subSecond;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t cycleCounter;

} rtcOverflow = { 0, 0, 0, 0, 0, 0 };

// Initialize the RTC for an ISR period of 0.5 second.
void init_RealTimeCounter (void) {
    MSS_RTC_init (MSS_RTC_BINARY_MODE, HALF_A_SECOND - 1u);
    MSS_RTC_start();
    MSS_RTC_set_binary_count_alarm (1, MSS_RTC_PERIODIC_ALARM);
    MSS_RTC_enable_irq();
}

// Called every 0.5 seconds, advances the RTC clock.
void RTC_Wakeup_IRQHandler (void) {

    MSS_RTC_clear_irq();
    rtcOverflow.cycleCounter++;

    // Update RTC time ------------------------------------
    uint8_t tmp;
    tmp = ++rtcOverflow.subSecond;
    if (tmp > TICKS_MINUTE - 1) {
        rtcOverflow.subSecond = 0;
        rtcOverflow.second = 0;
        rtcOverflow.minute++;

        if (rtcOverflow.minute > 59) {
            rtcOverflow.minute = 0;
            rtcOverflow.hour++;

            if (rtcOverflow.hour > 23) {
                rtcOverflow.hour = 0;
                rtcOverflow.day++;
            }
        }
    } else {
        rtcOverflow.second = tmp / 2;
    }
}

void erase_nvm(mss_uart_instance_t * this_uart){
    uint8_t pattern[NVM_PAGE_SIZE] = {[0 ... (NVM_PAGE_SIZE-1)] = 0x00};
    RTC_TIMESTAMP(&g_mss_uart0);
    for (uint32_t i = 0;i<LIMIT;i=i+1)
    {
        NVM_write(NVM_ADDR + (i*NVM_PAGE_SIZE), pattern, sizeof(pattern), NVM_DO_NOT_LOCK_PAGE);
    }
    RTC_TIMESTAMP(&g_mss_uart0);
    MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"\n\r**********************************************************************\n\r");
}

void write_nvm(mss_uart_instance_t * this_uart, uint8_t num_x){
    uint8_t pattern[NVM_PAGE_SIZE] = {[0 ... (NVM_PAGE_SIZE-1)] = num_x};
    RTC_TIMESTAMP(&g_mss_uart0);
    for (uint32_t i = 0;i<LIMIT;i=i+1)
    {
        NVM_write(NVM_ADDR + (i*NVM_PAGE_SIZE), pattern, sizeof(pattern), NVM_DO_NOT_LOCK_PAGE);
    }
    RTC_TIMESTAMP(&g_mss_uart0);
    MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"\n\r**********************************************************************\n\r");
}

uint32_t read_nvm(mss_uart_instance_t * this_uart, uint8_t num_x, uint32_t faults){
    uint8_t msg[120];
    uint8_t memo[128];
    RTC_TIMESTAMP(&g_mss_uart0);
    for (uint32_t i = 0;i<LIMIT;i=i+1)
    {
        memcpy(memo, NVM_ADDR + (i*NVM_PAGE_SIZE), NVM_PAGE_SIZE);
        for (uint8_t j = 0; j < NVM_PAGE_SIZE; j++ )
            {
                if (memo[j] != num_x) {
                    faults ++;
                    sprintf(msg, "\n\r --ERROR-- at address %#X ==> %zu is not %u", NVM_ADDR + (i*NVM_PAGE_SIZE)+j, memo[j], num_x);
                    MSS_UART_polled_tx_string(&g_mss_uart0, msg);}
            }
        sprintf(msg, "\n\r %#X ==> %zu ", NVM_ADDR + (i*NVM_PAGE_SIZE), i);
                    MSS_UART_polled_tx_string(&g_mss_uart0, msg);
    }
    RTC_TIMESTAMP(&g_mss_uart0);
    return faults;
}

void RTC_TIMESTAMP(mss_uart_instance_t * this_uart);
void erase_nvm(mss_uart_instance_t * this_uart);
void write_nvm(mss_uart_instance_t * this_uart, uint8_t num_x);
uint32_t read_nvm(mss_uart_instance_t * this_uart, uint8_t num_x,uint32_t faults);

int main(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    init_RealTimeCounter();



    //MSS_UART_init( mss_uart_instance_t* this_uart, uint32_t baud_rate, uint8_t line_config );
    MSS_UART_init ( &g_mss_uart0, MSS_UART_115200_BAUD,
                        MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);

    const uint8_t pattern_ones[NVM_PAGE_SIZE] = {[0 ... (NVM_PAGE_SIZE-1)] = VALUE};//0xAA so to be different than 0
    const uint8_t pattern_zeros[NVM_PAGE_SIZE] = {[0 ... (NVM_PAGE_SIZE-1)] = 0};
    //const uint8_t pattern_zeros_all[NVM_TEST_SIZE] = {[0 ... (NVM_PAGE_SIZE-1)] = 0};
    //const uint8_t pattern_ones_all[NVM_TEST_SIZE] = {[0 ... (NVM_PAGE_SIZE-1)] = 255};
    //const uint8_t pattern_a[] = { 0x41u, 0xABu, 0xCDu, 0xEFu, 0xAAu, 0xBBu };
    volatile nvm_status_t envm_status_returned = 0;



    uint32_t faults = 0;
    uint8_t msg[120];

    //UART and flow
    size_t rx_size;
    uint8_t rx_buff[1];
    uint8_t exit_prg = 1;
    uint8_t xxx = 1;
    SYSREG->WDOG_CR = 0;
    //uint8_t * p_nvm = (uint8_t *)NVM_ADDR;

    char *filename = "test.txt";

    // remaining pages from 0x60002000 to 0x6003fff
    //int total_pages_to_write = 1984;


    MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"\n\r**********************************************************************\n\r");
    MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"******************** SmartFusion2 eNVM TESTS ************************\n\r");
    MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"**********************************************************************\n\r");

    //Initialization
    MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"Initializing memory...\n\r");
    RTC_TIMESTAMP(&g_mss_uart0);
    //MSS_RTC_start();
    for (uint32_t i = 0;i<LIMIT;i=i+1)
    {
            envm_status_returned = 0;
            uint8_t * data_to_write_nvm = (uint8_t*) malloc(NVM_PAGE_SIZE * sizeof(uint8_t));
            uint8_t char_written = 0x00;
            envm_status_returned = NVM_unlock(NVM_ADDR + (i*NVM_PAGE_SIZE), NVM_PAGE_SIZE);
            memset(data_to_write_nvm, char_written, NVM_PAGE_SIZE);
            //NVM_write(NVM_ADDR + (i*NVM_PAGE_SIZE), pattern_zeros, sizeof(pattern_zeros), NVM_DO_NOT_LOCK_PAGE);
            envm_status_returned = NVM_write(NVM_ADDR + (i*NVM_PAGE_SIZE), data_to_write_nvm, NVM_PAGE_SIZE, NVM_DO_NOT_LOCK_PAGE);
            free(data_to_write_nvm);
            //envm_status_returned = NVM_unlock(NVM_ADDR + (i*NVM_PAGE_SIZE), envm_status_returned);
            sprintf(msg, "\n\r %#X ==> %zu ", NVM_ADDR + (i*NVM_PAGE_SIZE), i);
            MSS_UART_polled_tx_string(&g_mss_uart0, msg);


    }
    RTC_TIMESTAMP(&g_mss_uart0);

    //Injecting faults
    //NVM_write(NVM_ADDR, pattern_a, sizeof(pattern_a), NVM_DO_NOT_LOCK_PAGE);

    //Chequear
    /*for (uint8_t i = 0;i<LIMIT;i=i+1)
    {
        memcpy(memo, NVM_ADDR + (i*NVM_PAGE_SIZE), NVM_PAGE_SIZE);
        for (uint8_t j = 0; j < NVM_PAGE_SIZE; j++ )
            {
                if (memo[j] != 0xFF) {faults ++;}
            }
    }*/

    while(xxx==1)
    {
        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"\n\r**********************************************************************\n\r");
        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"SELECT :\n\r");
        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"1.Erase all \n\r");
        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"2.Read 0 \n\r");
        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"3.Read Value \n\r");
        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"4.Write 0 \n\r");
        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"5.Write Value \n\r");
        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"6.EXIT \n\r");
        //exit_prg=1;
        while(exit_prg != 0) {  // Inner loop for the iterations
            rx_size = MSS_UART_get_rx(&g_mss_uart0, rx_buff, sizeof(rx_buff));
            if(rx_size > 0){
                switch (rx_buff[0]) {
                    case '1':
                        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"Erasing memory...\n\r");
                        erase_nvm(&g_mss_uart0);
                        break;
                    case '2':
                        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"Reading 0...\n\r");
                        faults = read_nvm(&g_mss_uart0, 0x00,faults);
                        sprintf(msg, "\n\r Faults: %zu", faults);
                        MSS_UART_polled_tx_string(&g_mss_uart0, msg);
                        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"\n\r**********************************************************************\n\r");
                        break;
                    case '3':
                        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"Reading Value...\n\r");
                        faults = read_nvm(&g_mss_uart0, 0xFF,faults);
                        sprintf(msg, "\n\r Faults: %zu", faults);
                        MSS_UART_polled_tx_string(&g_mss_uart0, msg);
                        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"\n\r**********************************************************************\n\r");
                        break;
                    case '4':
                        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"Writting 0...\n\r");
                        write_nvm(&g_mss_uart0, 0x00);
                        break;
                    case '5':
                        MSS_UART_polled_tx_string(&g_mss_uart0,(const uint8_t*)"Writting Value...\n\r");
                        write_nvm(&g_mss_uart0, 0xFF);
                        break;
                    case '6':
                        exit_prg = 0;
                    default:
                        break;
                    }
            }
        }
        xxx=0;
    }
}

void RTC_TIMESTAMP(mss_uart_instance_t * this_uart)
{
    uint8_t msg[100] = "";

    // RTC Timestamp
    sprintf (msg,"\n\r[RTC-Time]\t%u:%02u:%02u:%02u (Day, hour, minute, second)",
            rtcOverflow.day, rtcOverflow.hour,
            rtcOverflow.minute, rtcOverflow.second);
    MSS_UART_polled_tx_string (&g_mss_uart0, msg);
}


