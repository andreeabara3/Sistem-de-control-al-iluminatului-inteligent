#include <stdio.h>
#include <stdint.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_types.h"
#include "xparameters.h"   // adresele prifericelor AXI GPIO, XADC
#include "xadcps.h"        // drivere pentru XADC
#include "xadcps_hw.h"
#include <sys/unistd.h>
#include <unistd.h>
#include <xil_types.h>
#include <xstatus.h>
#include "xgpio.h"
#include "xil_exception.h"

#define XADC_DEVICE_ID 0
#define LSB_BITS_NOT_USED 4   // ignoram 4 LSB, raman 12

// masti de biti pentru canalele auxiliare
#define XADCPS_SEQ_CH_VAUX14 (1 << 30)
#define XADCPS_SEQ_CH_VAUX7 (1 << 23)
#define XADCPS_SEQ_CH_VAUX15 (1 << 31)
#define XADCPS_SEQ_CH_VAUX6 (1 << 22)

static XAdcPs_Config *xadc_cfg;   // pointer la configuratia XADC
static XAdcPs xadc_ctl;   // structura de control XADC 

XGpio rgb;
XGpio keypad;

static void rgb_set (u8 r, u8 g, u8 b){
    u32 val = 0;
    if (r) val |= 1u << 0;   
    if (g) val |= 1u << 1;   
    if (b) val |= 1u << 2;   

    XGpio_DiscreteWrite(&rgb, 1, val);
}

 
static void kp_set_col(u8 col)
{
    u32 col_val;

    switch (col) {
        case 0: col_val = 0x0E; break; // 1110
        case 1: col_val = 0x0D; break; // 1101
        case 2: col_val = 0x0B; break; // 1011
        case 3: col_val = 0x07; break; // 0111
        default: col_val = 0x0F;  
    }

    XGpio_DiscreteWrite(&keypad, 1, col_val);
    usleep(200);                              
}

static u8 kp_read_rows(void)
{
    u32 val = XGpio_DiscreteRead(&keypad, 2);
    val &= 0x0F;  
    return (u8)val;
}

static int scan_keypad(void)
{
    for (int col = 0; col < 4; col++) {

        kp_set_col(col);          
        u8 rows = kp_read_rows(); 

        if (rows != 0xF) {     

            for (int row = 0; row < 4; row++) {
                if ((rows & (1u << row)) == 0) {   
                                                   
                    return row * 4 + col; 
                }
            }
        }
    }
    return -1; 
}

static int key_index_to_digit(int keyIndex)
{
    switch (keyIndex) {
    case 0:  return 1;    
    case 1:  return 2;
    case 2:  return 3;
    case 4:  return 4;
    case 5:  return 5;
    case 6:  return 6;
    case 8:  return 7;
    case 12: return 0;
    default: return -1;
    }
}

typedef enum {
    SCENE_EMPTY = 0,
    SCENE_LEVEL,
    SCENE_RELAX,
    SCENE_PARTY,
    SCENE_LED_BREATH,
    SCENE_AUTO_OFF
} SceneType;

static SceneType current_scene;
static unsigned int manual_duty = 0; 

static int party_index = 0;  
static int contor  = 0; 

static unsigned int breath_duty = 0;  
static int breath_dir = 1;   
static int breath_color_index = 0; 

static unsigned int led_breath_duty = 0;   
static int led_breath_dir = 1;

static unsigned int last_duty = 0;           
static unsigned int auto_off_current = 0;   
static int auto_off_ticks_left = 0;   


#define LOOP_DELAY_US 50000                 
#define AUTO_OFF_SECONDS 10
#define AUTO_OFF_TICKS (AUTO_OFF_SECONDS * (1000000 / LOOP_DELAY_US))  //  cati pasi in 10 secunde

static void party_step(void)
{
    const int STEP_TICKS = 6; 
    contor++;
    if (contor < STEP_TICKS)
        return;

    contor = 0;

    switch (party_index) {
    case 0: rgb_set(1,0,0); break; 
    case 1: rgb_set(0,1,0); break; 
    case 2: rgb_set(0,0,1); break; 
    case 3: rgb_set(1,1,0); break; 
    case 4: rgb_set(1,0,1); break; 
    case 5: rgb_set(0,1,1); break; 
    case 6: rgb_set(1,1,1); break; 
    }

    party_index = (party_index + 1) % 7;  
}

static void breath_step(void)
{
    const unsigned int MAX_BREATH = 3500;
    const unsigned int MIN_BREATH = 300;

    if (breath_dir > 0) {
        if (breath_duty + 40 < MAX_BREATH) 
            breath_duty += 40;  
        else {
            breath_duty = MAX_BREATH;  
            breath_dir = -1; 

            breath_color_index = (breath_color_index + 1) % 7; 
        }
    } else {
        if (breath_duty > MIN_BREATH + 40)  
            breath_duty -= 40;  
        else {
            breath_duty = MIN_BREATH;  
            breath_dir = 1;  

            breath_color_index = (breath_color_index + 1) % 7;  
        }
    }

    switch (breath_color_index) {
    case 0: rgb_set(1,0,0); break; // rosu
    case 1: rgb_set(0,1,0); break; // verde
    case 2: rgb_set(0,0,1); break; // albastru
    case 3: rgb_set(1,1,0); break; // galben
    case 4: rgb_set(1,0,1); break; // mov
    case 5: rgb_set(0,1,1); break; // turcoaz
    case 6: rgb_set(1,1,1); break; // alb
    default: rgb_set(0,0,0); break;
    }
}

static unsigned int led_breath_step(void)
{
    const unsigned int MAX = 4095;   
    const unsigned int STEP = 20;    

    if (led_breath_dir > 0) {
        if (led_breath_duty + STEP < MAX) {
            led_breath_duty += STEP;
        } else {
            led_breath_duty = MAX;
            led_breath_dir = -1;
        }
    } else {
        if (led_breath_duty > STEP) {
            led_breath_duty -= STEP;
        } else {
            led_breath_duty = 0;
            led_breath_dir = 1;
        }
    }

    return led_breath_duty;
}


static void apply_scene_digit(int digit)
{
    const unsigned int MAX = 4095u;

    switch (digit) {
    case 0: 
        current_scene = SCENE_EMPTY;
        manual_duty = 0;
        rgb_set(0,0,0);
        break;

    case 1: 
        current_scene = SCENE_LEVEL;
        manual_duty = (MAX * 10) / 100;
        rgb_set(0,0,0);
        break;

    case 2: 
        current_scene = SCENE_LEVEL;
        manual_duty = (MAX * 55) / 100;
        rgb_set(0,0,0);
        break;

    case 3: 
        current_scene = SCENE_LEVEL;
        manual_duty = (MAX * 90) / 100;
        rgb_set(0,0,0);
        break;

    case 4: 
        current_scene = SCENE_RELAX;
        manual_duty = 0;
        breath_duty = 300;
        breath_dir = 1;
        breath_color_index = 0;
        break;

    case 5: 
        current_scene = SCENE_PARTY;
        manual_duty = 0;
        party_index = 0;
        contor  = 0;
        break;

    case 7: 
        current_scene = SCENE_LED_BREATH;
        led_breath_duty = 0;  
        led_breath_dir = 1;    
        rgb_set(0,0,0);
        break;

    default:
        break;
    }
}



static void start_auto_off(unsigned int current_duty)
{
    current_scene = SCENE_AUTO_OFF;
    auto_off_current = current_duty;
    auto_off_ticks_left = AUTO_OFF_TICKS;

    rgb_set(0,0,0);
}

static unsigned int auto_off_step(void)
{
    if (auto_off_ticks_left <= 0) {
        current_scene = SCENE_EMPTY;
        manual_duty = 0;
        rgb_set(0,0,0);
        return 0; 
    }

    unsigned int step = (auto_off_current + (unsigned int)auto_off_ticks_left - 1) / (unsigned int)auto_off_ticks_left;

    if (step < 1) step = 1;

    if (auto_off_current > step) auto_off_current -= step;
    else auto_off_current = 0;

    auto_off_ticks_left--; 

    if (auto_off_current == 0 || auto_off_ticks_left == 0) {
        current_scene = SCENE_EMPTY;
        manual_duty = 0;
        rgb_set(0,0,0);
    }

    return auto_off_current;
}


int main()
{
    XGpio inputPWM, inputSw;

    int switch_data = 0;

    init_platform();

    s32 status;  // stocheaza starea apelurilor functiilor
    
    XGpio_Initialize(&inputPWM, XPAR_AXI_GPIO_DUTY_BASEADDR);
    XGpio_Initialize(&inputSw, XPAR_AXI_GPIO_SW_BASEADDR);
    XGpio_Initialize(&keypad,   XPAR_AXI_GPIO_KEYPAD_BASEADDR);
    XGpio_Initialize(&rgb,  XPAR_AXI_GPIO_RGB_BASEADDR);

    XGpio_SetDataDirection(&inputPWM, 1, 0x0000);
    XGpio_SetDataDirection(&inputSw, 1, 0xF);
    XGpio_SetDataDirection(&keypad,   1, 0x0000); 
    XGpio_SetDataDirection(&keypad,   2, 0x000F); 
    XGpio_SetDataDirection(&rgb,  1, 0x0000);

    rgb_set(0,0,0);

    // cauta configuratia XADC dupa ID
    xadc_cfg = XAdcPs_LookupConfig(XADC_DEVICE_ID);
    if (NULL == xadc_cfg){
        print("Configuratia a crapat\n\r");
        return XST_FAILURE;
    }

    // initializeaza XADC cu configuratia gasita
    status = XAdcPs_CfgInitialize(&xadc_ctl, xadc_cfg, xadc_cfg -> BaseAddress);

    if (status != XST_SUCCESS) {
        print("Statusul a crapat\n\r");
        return XST_FAILURE;
    }

    print("Configuratia a mers bine, incepe procesarea\n\r");

    // seteaza XADC in modul de conversie continua
    XAdcPs_SetSequencerMode(&xadc_ctl, XADCPS_SEQ_MODE_CONTINPASS);
    u32 ChannelMask = XADCPS_SEQ_CH_VAUX14 | XADCPS_SEQ_CH_VAUX7 | XADCPS_SEQ_CH_VAUX15 | XADCPS_SEQ_CH_VAUX6;
    
    // activeaza canalele din XADC
    XAdcPs_SetSeqChEnables(&xadc_ctl, ChannelMask);

    while(1){
        switch_data = XGpio_DiscreteRead(&inputSw, 1);
        u16 Vaux14Data = XAdcPs_GetAdcData(&xadc_ctl, 30);

        Vaux14Data >>= LSB_BITS_NOT_USED;
        Vaux14Data &= 0x0FFF;

        unsigned int duty_value = 0;
        unsigned int sw0 = switch_data & 0x1;

        int keyIndex_global = scan_keypad();

        if (keyIndex_global == 13) {   // F
            start_auto_off(last_duty);
            usleep(150000);         
        }

        if (current_scene == SCENE_AUTO_OFF) {
            duty_value = auto_off_step();
            XGpio_DiscreteWrite(&inputPWM, 1, duty_value);
            last_duty = duty_value;
            usleep(LOOP_DELAY_US);
            continue; 
        }


       if (sw0 == 0) {

             rgb_set(0,0,0);
             duty_value = 4095 - Vaux14Data;

        } else if (sw0 == 1){
            int keyIndex = scan_keypad();
            int digit    = key_index_to_digit(keyIndex);

            if (digit >= 0) {
                apply_scene_digit(digit);
            }

            switch (current_scene) {
            case SCENE_EMPTY:
                duty_value = 0;
                break;

            case SCENE_LEVEL:
                duty_value = manual_duty;
                break;

            case SCENE_RELAX:
                duty_value = manual_duty; 
                breath_step();
                break;

            case SCENE_PARTY:
                duty_value = manual_duty; 
                party_step();             
                break;

            case SCENE_LED_BREATH:
                duty_value = led_breath_step();
                break;

            case SCENE_AUTO_OFF:
                duty_value = auto_off_current;
                break;
            }
        }

        XGpio_DiscreteWrite(&inputPWM, 1, duty_value);
        last_duty = duty_value;      
        usleep(LOOP_DELAY_US);

    }
    

    cleanup_platform();
    return 0;
}
