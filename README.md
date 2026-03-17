# Projeto Acadêmico: F1 Fuel Flow Gate

## Simulação de Sistemas de Tempo Real utilizando FreeRTOS e ESP32

 Sobre o Projeto
Este projeto foi desenvolvido como requisito para a disciplina de Sistemas em Tempo Real (STR). O objetivo principal é demonstrar a aplicação prática de conceitos de Sistemas Operacionais de Tempo Real (RTOS) na resolução de problemas de sincronização, manipulação de sinais e concorrência.
O cenário simula um sistema de injeção de combustível de um carro de Fórmula 1, inspirado em polêmicas reais de "burlas" no regulamento:

    A Regra (Sensor da FIA): O regulamento estipula um limite de fluxo de combustível de 100 kg/h. A fiscalização (FIA) realiza amostragens rigorosas a cada 30ms.
    A Burla (Atuador/Cheat): O sistema injeta um excesso de combustível (120 kg/h) exatamente nos "intervalos cegos" do sensor, reduzindo o fluxo milissegundos antes da próxima leitura da FIA para mascarar a infração.

O sistema foi implementado na plataforma ESP32, utilizando a arquitetura do FreeRTOS para garantir o determinismo e a precisão temporal de hardware necessários para o sucesso da burla.

--------------------------------------------------------------------------------
 Arquitetura de Software e Conceitos de FreeRTOS Aplicados
A solução arquitetural do firmware utiliza técnicas avançadas do FreeRTOS para garantir o cumprimento dos prazos (deadlines) rigorosos da aplicação:
1. Escalonamento e Prioridade de Tarefas (Tasks)
O projeto foi dividido em duas tarefas principais independentes, ambas alocadas no Core 0 para garantir a compatibilidade com arquiteturas single-core (como o ESP32-C3):

    vSensorTask (Prioridade 5): Tarefa crítica de alta prioridade que representa a fiscalização da FIA. Ela aguarda a notificação de um Timer de Hardware configurado para estourar a exatos 30ms (2000 vezes por minuto).
    vActuatorTask (Prioridade 4): Tarefa responsável pelo controle do fluxo de combustível. Opera com prioridade ligeiramente inferior para não preempitar a fiscalização oficial. Utiliza bloqueio não-ocioso para liberar a CPU enquanto a injeção extra ocorre.

2. Sincronização e Comunicação Interprocessos (IPC)
Foram empregadas duas técnicas distintas para a sincronização leve e eficiente:

    Task Notifications: Utilizadas para a comunicação entre a Rotina de Interrupção do sensor e a vSensorTask (vTaskNotifyGiveFromISR e ulTaskNotifyTake), garantindo o destravamento imediato da tarefa. O sensor também utiliza notificações para avisar o atuador do momento exato em que a "janela cega" de leitura se inicia.
    Semáforos Binários (xSemaphoreCreateBinary): Aplicados para gerenciar o Timer Dinâmico do Atuador. Um semáforo é liberado pela ISR do atuador (xSemaphoreGiveFromISR) e consumido pela task do atuador (xSemaphoreTake), acionando a redução do fluxo de combustível. Adicionalmente, o código implementa a diretiva portYIELD_FROM_ISR() para forçar a troca de contexto instantânea caso a tarefa destravada tenha maior prioridade.

3. Tratamento de Interrupções (ISRs) e Concorrência
As Rotinas de Serviço de Interrupção (ISRs) foram projetadas seguindo a regra fundamental do FreeRTOS: Processamento Deferido. As funções onTimer, onTimerAtuador, isrCheat e isrFail são extremamente curtas, apenas sinalizando eventos ou alternando flags. O processamento pesado é delegado para o contexto das tasks.
Além disso:

    Variáveis volatile: Atributos compartilhados entre ISRs e Tasks (como fuel_flow, cheatMode e forceFail) receberam a diretiva volatile para evitar otimizações do compilador e garantir a integridade da leitura na memória.
    Debounce em Software: A interrupção do botão que ativa o modo "Cheat" (isrCheat) conta com uma rotina de debouncing baseada na função millis() com um limiar de 200ms, mitigando ruídos mecânicos típicos de hardware físico.


--------------------------------------------------------------------------------
 Hardware e Pinout (ESP32)
O projeto mapeia os seguintes periféricos de entrada e saída:
Componente
	
Pino (GPIO)
	
Função / Comportamento
	
Configuração
LED Verde
	
18
	
Status OK: Acende quando o fluxo detectado pela FIA é ≤ 100 kg/h.
	
OUTPUT
LED Vermelho
	
19
	
Status Violação: Acende quando a FIA detecta fluxo > 100 kg/h.
	
OUTPUT
LED Azul/Amarelo
	
4
	
Indicador de Cheat: Acende quando a burla está ativa no sistema.
	
OUTPUT
Botão 1 (Cheat)
	
8
	
Ativa/Desativa o sistema de burla no fluxo de combustível.
	
INPUT_PULLUP com ISR (FALLING)
Botão 2 (Fail)
	
9
	
Força uma falha de sincronização no sistema do atuador.
	
INPUT_PULLUP com ISR (CHANGE)

--------------------------------------------------------------------------------
 Lógica de Funcionamento e Modos de Operação

    Modo Legal (Normal): O fluxo de combustível se mantém constante em 100 kg/h. O sensor da FIA realiza leituras a cada 30ms e valida o status, mantendo o LED Verde aceso.
    Modo Cheat (Burla Ativa): Ao pressionar o Botão Cheat, o atuador passa a elevar o fluxo para 120 kg/h imediatamente após cada leitura da FIA. O atuador arma um Hardware Timer (timerAtuador) para interromper a injeção extra (aos 25ms), retornando o fluxo para 100 kg/h estritamente antes do próximo ciclo de 30ms da FIA. A infração passa despercebida.
    Modo Force Fail (Falha Imposta): Ao acionar o botão de falha, o tempo de injeção extra do atuador é prolongado para 30ms. Isso causa um atraso proposital (uma "condição de corrida"), fazendo com que a janela do atuador se sobreponha à leitura da FIA. O sensor da FIA capta o valor de 120 kg/h em memória, detecta a infração, e aciona o LED Vermelho no painel.


--------------------------------------------------------------------------------
 Como Executar a Simulação

    O projeto pode ser executado diretamente na plataforma Wokwi Simulator ou compilado via Arduino IDE (com suporte ao núcleo do ESP32 versão 3.x).
    Na Arduino IDE, selecione a placa "ESP32C3 Dev Module" (ou equivalente compatível).
    O monitor serial deve ser configurado para o Baud Rate de 115200.
    Monitore a saída ou utilize o Serial Plotter para observar os gráficos de variação da variável fuel_flow interagindo com as deadlines de leitura. O código otimiza o uso da memória ao deletar a tarefa ociosa loop na inicialização (vTaskDelete(NULL)).


--------------------------------------------------------------------------------
