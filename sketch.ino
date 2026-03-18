

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Definição dos pinos dos Leds e Botões
#define PIN_LED_VERMELHO 19
#define PIN_LED_VERDE 18
#define PIN_LED_CHEAT 4
#define PIN_BTN_CHEAT 8
#define PIN_BTN_FAIL 9

/* Variáveis definidas como volatile para informar ao compilador 
   pode ser alterado fora do fluxo normal do programa*/
volatile int fuel_flow = 100; // Fluxo de combustível
volatile bool cheatMode = false; // Estado do botão para ativar o cheat
volatile bool forceFail = false; // Estado do botão para ativar a falha no cheat

// Handles para gerenciamento do de recursos e comunicação entre as tarefas
hw_timer_t *timer = NULL;
TaskHandle_t xSensorTaskHandle = NULL; 
TaskHandle_t xActuatorTaskHandle = NULL;
hw_timer_t *timerAtuador = NULL;
SemaphoreHandle_t xTimerSemaphore = NULL; // Semáforo para precisão temporal

/* Debounce em software para tratar das interrupções
   do botão que ativa o cheat*/
volatile uint32_t lastDebounceTime = 0;
const uint32_t debounceDelay = 200;

void IRAM_ATTR isrCheat(){
 uint32_t currentTime = millis();
    if (currentTime - lastDebounceTime > debounceDelay) {
        cheatMode = !cheatMode; 
        lastDebounceTime = currentTime;
    }
}

// Funções 'isr' - eventos externos (botões)
void IRAM_ATTR isrFail(){
  forceFail = !digitalRead(PIN_BTN_FAIL);
}

/* Gatilho de precisão do sistema.
   Essa função notifica a tarefa do sensor para fazer
   a verificação do combustível.*/
void IRAM_ATTR onTimer() {
  vTaskNotifyGiveFromISR(xSensorTaskHandle, NULL);
}

// Interrupção para controle do fluxo de combustível
void IRAM_ATTR onTimerAtuador() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Libera o semáforo para destravar a task do atuador
  xSemaphoreGiveFromISR(xTimerSemaphore, &xHigherPriorityTaskWoken);
  
  if(xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

// Tarefa dedicada ao sensor fiscal da FIA.
void vSensorTask(void *param){
  uint32_t ulNotificationValue;
  for(;;){
    /* Bloqueio não ocioso a tarefa fica suspensa até a notificação
       timer "void IRAM_ATTR onTimer()"*/
    ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if(ulNotificationValue > 0){
      if(fuel_flow > 100){
        // Sheat detectado
        Serial.println("Infração detectada!");
        digitalWrite(PIN_LED_VERMELHO, HIGH);
        digitalWrite(PIN_LED_VERDE, LOW);
      }else{
        // Dentro do parâmetro
        Serial.println("Dentro do parâmetro!");
        digitalWrite(PIN_LED_VERDE, HIGH);
        digitalWrite(PIN_LED_VERMELHO, LOW);
      }
      // Notifica o Atuador que a leitura acabou de ser feita
      xTaskNotifyGive(xActuatorTaskHandle);
    }
  }
}

// Tarefa dedicada ao cheat.
void vActuatorTask(void *pvParameters) {
    for (;;) {
        /* Espera o sinal do Sensor (Janela de leitura fechada)
           Aqui ele aguarda a notificação que a tarefa do sensor
           acabou de fazer a leitura.*/
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (cheatMode) {
            digitalWrite(PIN_LED_CHEAT, HIGH);
            // Injeta excesso após a leitura do sensor
            fuel_flow = 120;
            Serial.print("Fluxo_Real:"); Serial.println(fuel_flow);
            /* Cálculo de Tempo:
               O ciclo é de 30ms nesse caso o cheat injeta por apenas 25ms.
               Se 'forceFail' estiver ativo, o cheat se 'atrasa' e passa
               30ms injetando. */

               // Muda o valor do waitTime conforme o estado do cheat (ativado ou não)
            int waitTime = forceFail ? 30 : 25;
            //vTaskDelay(pdMS_TO_TICKS(waitTime));
            timerRestart(timerAtuador); 
            timerAlarm(timerAtuador, waitTime * 1000, false, 0);
            xSemaphoreTake(xTimerSemaphore, portMAX_DELAY);
            // Recua o fluxo para o limite legal
            fuel_flow = 100;
            Serial.print("Fluxo_Real:"); Serial.println(fuel_flow);
        } else {
            digitalWrite(PIN_LED_CHEAT, LOW);
            fuel_flow = 100; // Mantém legal
            Serial.print("Fluxo_Real:"); Serial.println(fuel_flow);
        }
    }
}


void setup(){
  Serial.begin(115200);
  
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_LED_CHEAT, OUTPUT);
  pinMode(PIN_BTN_CHEAT, INPUT_PULLUP);
  pinMode(PIN_BTN_FAIL, INPUT_PULLUP);

  xTimerSemaphore = xSemaphoreCreateBinary();
  timerAtuador = timerBegin(1000000); 
  timerAttachInterrupt(timerAtuador, &onTimerAtuador);

  // Associações das interrupções de hardware dos botões
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_CHEAT), isrCheat, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_FAIL), isrFail, CHANGE);

  // Atributos da tarefa do sensor FIA
  xTaskCreatePinnedToCore(
    vSensorTask,      // Function to be performed the task is called 
  "Sensor_Task",    // Name of the task in text
    4096,          // Stack size (Memory size assigned to the task)
    NULL,         // Pointer that will be used as the parameter for the task being created
    5,           // Task Priority 
    &xSensorTaskHandle, // The task handler
    0          //xCoreID (Core 0)
    );

  // Atributos da tarefa do atuador (cheat)
  xTaskCreatePinnedToCore(
    vActuatorTask,      // Function to be performed the task is called 
  "Actuator_Task",    // Name of the task in text
    4096,          // Stack size (Memory size assigned to the task)
    NULL,         // Pointer that will be used as the parameter for the task being created
    4,           // Task Priority 
    &xActuatorTaskHandle, // The task handler
    0          //xCoreID (Core 0)
    );


  // Configuração do alarme para garantir a amostragem do sensor FIA:

  // Create and start timer (num, divider, countUp)
  timer = timerBegin(1000000);
  // Provide ISR to timer (timer, function, edge)
  timerAttachInterrupt(timer, &onTimer);
  // At what count should ISR trigger (timer, count, autoreload)
  timerAlarm(timer, 30*1000, true, 0);

  Serial.println("Fiscalização da FIA operando a 30ms...");
}

void loop(){

  vTaskDelete(NULL);
}