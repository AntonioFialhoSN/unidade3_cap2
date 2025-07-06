#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Definições de pinos
#define LED_RED_ALIVE 13
#define LED_GREEN 11
#define LED_BLUE 12
#define BUZZER_PIN 21
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define JOYSTICK_SW_PIN 22
#define JOYSTICK_Y_ADC 0  // ADC0 (GPIO26)
#define JOYSTICK_X_ADC 1  // ADC1 (GPIO27)
#define MICROPHONE_ADC 2  // ADC2 (GPIO28)

// Configuração PWM para o buzzer
#define PWM_FREQ_HZ 1000
#define CLOCK_DIV 2.0f
#define PWM_WRAP (uint16_t)(125000000 / (PWM_FREQ_HZ * CLOCK_DIV))

// Mutex para acesso seguro à serial USB
SemaphoreHandle_t usb_mutex;

// Protótipos das tarefas
void self_test_task(void *param);
void alive_task(void *param);
void joystick_monitor_task(void *param);

// Funções auxiliares
void test_leds();
void test_buzzer();
void test_buttons();
void test_joystick_sw();
void test_adc_channels();
void setup_buzzer_pwm();
void buzzer_beep(uint16_t duration_ms);

int main() {
    stdio_init_all();
    sleep_ms(2000); // Espera para estabilizar a conexão USB

    // Inicializa o mutex para a serial USB
    usb_mutex = xSemaphoreCreateMutex();

    // Cria as tarefas
    xTaskCreate(self_test_task, "Self-Test", 512, NULL, 3, NULL);
    xTaskCreate(alive_task, "Alive Task", 256, NULL, 1, NULL);
    xTaskCreate(joystick_monitor_task, "Joystick Monitor", 512, NULL, 2, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();

    // Nunca deveria chegar aqui
    while (1) {
        tight_loop_contents();
    }
    return 0;
}

// Tarefa 1: Self-Test (executa uma vez e se auto-deleta)
void self_test_task(void *param) {
    if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
        printf("\n--- Iniciando Self-Test ---\n");
        xSemaphoreGive(usb_mutex);
    }

    // Testa LEDs RGB
    test_leds();
    
    // Testa Buzzer
    test_buzzer();
    
    // Testa botões
    test_buttons();
    
    // Testa joystick SW
    test_joystick_sw();
    
    // Testa canais ADC (joystick analógico e microfone)
    test_adc_channels();
    
    if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
        printf("\n--- Self-Test concluído com sucesso ---\n");
        xSemaphoreGive(usb_mutex);
    }
    
    // Auto-deleta a tarefa
    vTaskDelete(NULL);
}

// Tarefa 2: Alive Task (pisca LED vermelho continuamente)
void alive_task(void *param) {
    gpio_init(LED_RED_ALIVE);
    gpio_set_dir(LED_RED_ALIVE, GPIO_OUT);
    
    while (1) {
        gpio_put(LED_RED_ALIVE, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_put(LED_RED_ALIVE, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Tarefa 3: Monitor de Joystick e Alarme
void joystick_monitor_task(void *param) {
    // Inicializa ADCs para joystick
    adc_init();
    adc_gpio_init(26); // ADC0 (Joystick Y)
    adc_gpio_init(27); // ADC1 (Joystick X)
    
    // Configura o buzzer
    setup_buzzer_pwm();
    bool buzzer_active = false;
    
    while (1) {
        // Lê os valores do joystick
        adc_select_input(JOYSTICK_Y_ADC);
        float y_voltage = adc_read() * 3.3f / 4096;
        adc_select_input(JOYSTICK_X_ADC);
        float x_voltage = adc_read() * 3.3f / 4096;
        
        // Verifica se deve ativar o alarme
        bool alarm_condition = (x_voltage > 3.00f) || (y_voltage > 3.00f);
        
        if (alarm_condition && !buzzer_active) {
            pwm_set_gpio_level(BUZZER_PIN, PWM_WRAP / 2); // Liga buzzer (50% duty cycle)
            buzzer_active = true;
        } else if (!alarm_condition && buzzer_active) {
            pwm_set_gpio_level(BUZZER_PIN, 0); // Desliga buzzer
            buzzer_active = false;
        }
        
        // Imprime os valores (protegido por mutex)
        if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
            printf("Joystick - X: %.2fV, Y: %.2fV\n", x_voltage, y_voltage);
            xSemaphoreGive(usb_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Funções auxiliares para o Self-Test
void test_leds() {
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    
    if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
        printf("Testando LEDs...\n");
        xSemaphoreGive(usb_mutex);
    }
    
    // Testa LED verde
    gpio_put(LED_GREEN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_put(LED_GREEN, 0);
    
    // Testa LED azul
    gpio_put(LED_BLUE, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_put(LED_BLUE, 0);
    
    vTaskDelay(pdMS_TO_TICKS(500));
}

void test_buzzer() {
    setup_buzzer_pwm();
    
    if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
        printf("Testando Buzzer...\n");
        xSemaphoreGive(usb_mutex);
    }
    
    // Toca um beep de teste
    buzzer_beep(200);
    vTaskDelay(pdMS_TO_TICKS(500));
}

void test_buttons() {
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    
    if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
        printf("Testando botões...\n");
        printf("Pressione os botões A e B...\n");
        xSemaphoreGive(usb_mutex);
    }
    
    // Aguarda um pouco para dar tempo de pressionar os botões
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start_time < 3000) {
        bool a_pressed = !gpio_get(BUTTON_A_PIN);
        bool b_pressed = !gpio_get(BUTTON_B_PIN);
        
        if (a_pressed || b_pressed) {
            if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
                printf("Botão %s pressionado\n", a_pressed ? "A" : "B");
                xSemaphoreGive(usb_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(200)); // Debounce
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void test_joystick_sw() {
    gpio_init(JOYSTICK_SW_PIN);
    gpio_set_dir(JOYSTICK_SW_PIN, GPIO_IN);
    gpio_pull_up(JOYSTICK_SW_PIN);
    
    if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
        printf("Testando botão do joystick...\n");
        printf("Pressione o botão do joystick...\n");
        xSemaphoreGive(usb_mutex);
    }
    
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start_time < 3000) {
        if (!gpio_get(JOYSTICK_SW_PIN)) {
            if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
                printf("Botão do joystick pressionado\n");
                xSemaphoreGive(usb_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(200)); // Debounce
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void test_adc_channels() {
    adc_init();
    adc_gpio_init(26); // ADC0 (Joystick Y)
    adc_gpio_init(27); // ADC1 (Joystick X)
    adc_gpio_init(28); // ADC2 (Microfone)
    
    if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
        printf("Testando canais ADC...\n");
        xSemaphoreGive(usb_mutex);
    }
    
    // Lê cada canal algumas vezes e mostra os valores
    for (int i = 0; i < 5; i++) {
        adc_select_input(JOYSTICK_Y_ADC);
        uint16_t y_val = adc_read();
        float y_voltage = y_val * 3.3f / 4096;
        
        adc_select_input(JOYSTICK_X_ADC);
        uint16_t x_val = adc_read();
        float x_voltage = x_val * 3.3f / 4096;
        
        adc_select_input(MICROPHONE_ADC);
        uint16_t mic_val = adc_read();
        float mic_voltage = mic_val * 3.3f / 4096;
        
        if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100))) {
            printf("ADC - Joystick X: %.2fV, Y: %.2fV, Microfone: %.2fV\n", 
                  x_voltage, y_voltage, mic_voltage);
            xSemaphoreGive(usb_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void setup_buzzer_pwm() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, CLOCK_DIV);
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(slice, &cfg, true);
    pwm_set_gpio_level(BUZZER_PIN, 0); // Começa desligado
}

void buzzer_beep(uint16_t duration_ms) {
    pwm_set_gpio_level(BUZZER_PIN, PWM_WRAP / 2); // 50% duty cycle
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    pwm_set_gpio_level(BUZZER_PIN, 0);
}