# Especificação para implementação – Carrinho controlado por joystick (ESP32, ESP-IDF v6.1)

Documento para um agente de código implementar o firmware dos dois ESP32 (controle e carrinho) em **C, com ESP-IDF v6.1** (sem Arduino). Origem: proposta de projeto da disciplina de Sistemas Embarcados (Grau B), versão de 12/11/2026: **1 joystick + 1 ponte H, 2 motores DC de tração, sem servo e sem botão de parada** (parar = joystick no neutro).

Itens marcados com **[CONFIRMAR]** são valores propostos (pinos, limiares) que dependem do hardware montado. Mantenha todos os valores numéricos em `config.h`.

---

## 0. Instruções para o agente (ler primeiro)

**Ambiente:** Windows, pasta do trabalho `C:\Unisinos\embarcados\trabalhoGB`. ESP-IDF v6.1 instalado no computador (usar o terminal "ESP-IDF PowerShell" ou carregar o ambiente com o `export.ps1` do IDF antes de `idf.py`). Alvo `esp32`. Antes de qualquer coisa, rodar `idf.py --version` e confirmar que é a v6.1; se for outra, **parar e avisar**, sem seguir com outra versão.

**Escopo da fase 1 (esta tarefa): somente o carrinho.**
1. Implementar `components/protocol`, `components/logic`, `test_apps/logic_host` e o projeto do **carrinho** (`car`), seguindo a ordem 1 a 3 da seção 14 e a recepção/validação do passo 5.
2. O projeto do **controle** fica só como esqueleto compilável (`app_main` que loga o MAC e nada mais). **Não implementar** o firmware do controle agora.
3. O controle ainda não existe, então o carrinho precisa de um **modo de bancada** (seção 15.4) para ser testado sozinho.

**Pastas existentes:** na raiz há duas pastas e um arquivo `.md` (esta especificação). Inspecione as duas pastas antes de criar qualquer coisa. Se uma já for projeto do carrinho e a outra do controle, use-as com esses papéis; se estiverem vazias ou com outros nomes, use-as como `car` e `controller` e **registre a escolha no README**. `components/` e `test_apps/` ficam na raiz do trabalho. **Não apague nem sobrescreva nada existente** sem avisar.

**Regras de trabalho:**
- Siga o RNF05 (seção 3): só componentes do ESP-IDF; validar cada API antes de usar. Tente primeiro `search_espressif_sources` do MCP `espressif-documentation`; se não estiver disponível, use a documentação oficial v6.1 e registre isso no README.
- **Não pergunte** sobre itens **[CONFIRMAR]**: use o valor padrão da seção 10 e da seção 5, e liste cada suposição na seção "Suposições" do README.
- Todo valor numérico fica em `config.h`; nada de "número mágico" no código.
- Se uma API da seção 12.2 não puder ser validada, **não improvise**: escolha a alternativa mais simples já documentada, comente no código `// TODO validar` e liste no README.
- Você não tem a placa. Você **pode** compilar e rodar os testes Unity; você **não pode** gravar nem testar o hardware. Não afirme que algo "funciona na bancada".

**Definição de pronto da fase 1:**
1. `idf.py build` do `car` termina **sem warnings** (o v6 trata warnings como erros).
2. `test_apps/logic_host` compila e todos os testes de 9.1 aplicáveis à lógica do carrinho passam (alvo linux; se não funcionar, deixar compilando para `esp32` e dizer isso).
3. Modo de bancada implementado e documentado (15.4).
4. `README.md` com: versão do IDF, ligações, como compilar/gravar (`idf.py -p COMx flash monitor`), checklist de bancada (T1, T2, T3, T5), seção "APIs validadas" e seção "Suposições".
5. Resumo final dizendo **o que foi feito, o que foi só compilado e não testado em hardware, e o que ficou pendente**.

---

## 1. Resumo do sistema

- **Controle:** ESP32 + 1 joystick analógico. Lê X e Y, normaliza, e envia o estado por ESP-NOW a 20 Hz (mesmo com o joystick parado: é o sinal de vida do enlace).
- **Carrinho:** ESP32 + ponte H (2 canais) + 2 motores DC de tração (um de cada lado) + sensor ultrassônico HC-SR04 frontal + divisor de tensão da bateria. Recebe comandos, decide localmente e atua. Devolve telemetria a 5 Hz.
- **Direção:** pela diferença de velocidade entre os motores (mistura estilo "arcade").
- **Parar:** joystick no neutro (zona morta), obstáculo à frente ou perda de comunicação. Não existe botão de parada.
- O carrinho **nunca** pode andar sozinho após reconexão ou reinício: voltar a andar exige o joystick passar pelo centro.

### 1.1 Mapa do joystick

| Eixo | Função |
| --- | --- |
| Y | Frente (+) e ré (−). Centro = parado |
| X | Direita (+) e esquerda (−) |
| Botão (SW) | Buzina: enquanto apertado, o buzzer do carrinho toca (bit `PROTO_BTN_HORN`) |

## 2. Requisitos (rastreabilidade com a proposta)

Numeração e texto seguem a proposta v2 (1 joystick + ponte H, de 08/11). **[CONFIRMAR]** se os IDs coincidem com os da versão entregue ao professor.

| ID | Requisito |
| --- | --- |
| RF01 | O controle envia o estado do joystick por ESP-NOW a 20 Hz, com número de sequência. |
| RF02 | Eixo Y define frente, ré e parada (centro); eixo X define direita/esquerda; o ESP32 do carrinho mistura os eixos e aplica PWM e sentido a cada motor. Zona morta de 10% nos eixos. |
| RF03 | Com o joystick no neutro (zona morta), os motores ficam parados. |
| RF04 | Com obstáculo a 15 cm ou menos, ou sensor sem leitura válida, o carrinho bloqueia o avanço. |
| RF05 | Sem pacote válido por mais de 300 ms, o carrinho para. |
| RF06 | Comandos fora de faixa, vencidos ou incompatíveis com o estado são rejeitados. |
| RF07 | Após uma falha, o carrinho só retoma com o joystick no centro. |
| RNF01 | Parada em 500 ms ou menos após a interrupção dos comandos. |
| RNF02 | Parada por obstáculo à velocidade máxima, com distância final de 8 cm ou mais (10 de 10 ensaios). |
| RNF03 | Nenhuma reinicialização do ESP32 em 20 partidas simultâneas dos motores. |
| RNF04 | Parada em 300 ms ou menos após a leitura do joystick entrar na zona morta (10 de 10 ensaios). |
| RNF05 | O sistema deve utilizar bibliotecas oficiais do ESP-IDF sempre que possível, evitando bibliotecas de terceiros, a menos que seja estritamente necessário. As assinaturas e APIs do ESP-IDF v6.1 devem ser validadas consultando o servidor MCP `espressif-documentation` (ferramenta `search_espressif_sources`). |

O RNF05 é de implementação (não consta na proposta entregue ao professor, que termina no RNF04).

## 3. Como atender o RNF05

1. **Sem dependências externas:** não usar Arduino, nem componentes do ESP Component Manager (`idf_component.yml`), nem bibliotecas de terceiros. Tudo o que o projeto precisa existe no ESP-IDF: Wi-Fi/ESP-NOW, ADC, LEDC, GPIO, `esp_timer`, FreeRTOS, log e Unity.
2. **Validar cada API antes de usar**, com a ferramenta `search_espressif_sources` do servidor MCP `espressif-documentation`. Se o MCP não estiver disponível na sessão do agente, usar a documentação oficial v6.1 em `docs.espressif.com/projects/esp-idf/en/v6.1/` e dizer isso no README.
3. **Registrar a validação** na seção "APIs validadas" do `README.md`: para cada API, assinatura confirmada, fonte consultada e data.
4. A validação já feita na elaboração desta especificação está na seção 12. **O MCP `espressif-documentation` não estava disponível naquela sessão**; a verificação foi feita nas páginas oficiais v6.1. Os itens "não verificados" precisam ser conferidos pelo agente.
5. Esta é uma versão **recente (v6.x)** e há quebras em relação ao v5.x e a muitos exemplos da internet (seção 12.3).

## 4. Estrutura do projeto

**ESP-IDF v6.1**, linguagem C, alvo `esp32`. Uma aplicação IDF por placa, compartilhando componentes. A lógica de decisão fica em componentes C puros (sem chamadas ao ESP-IDF), testáveis no PC.

```
trabalhoGB/
├── components/
│   ├── protocol/              # pacotes, CRC-8, enum de estados (C puro)
│   └── logic/                 # joystick, mixer, rampa, link, máquina de estados (C puro)
├── car/                       # projeto IDF do carrinho
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   └── main/ (CMakeLists.txt, Kconfig.projbuild, config.h, main.c)
├── controller/                # projeto IDF do controle
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   └── main/ (CMakeLists.txt, Kconfig.projbuild, config.h, main.c)
├── test_apps/logic_host/      # testes Unity da lógica (alvo linux, ver seção 9)
└── README.md                  # ligações, MACs, calibração, APIs validadas, suposições, como testar
```

Cada `CMakeLists.txt` de projeto usa `set(EXTRA_COMPONENT_DIRS ...)` apontando para `components/` antes de incluir `project.cmake`. Declarar **explicitamente** os componentes do ESP-IDF usados com `REQUIRES`/`PRIV_REQUIRES` (no v6.0 o componente `driver` deixou de puxar os `esp_driver_*`):

| Uso | Componente | Header |
| --- | --- | --- |
| ESP-NOW e Wi-Fi | `esp_wifi` | `esp_now.h`, `esp_wifi.h` **[validar o componente de `esp_now.h`]** |
| Rede/eventos | `esp_netif`, `esp_event` | `esp_netif.h`, `esp_event.h` |
| NVS (se usado pelo Wi-Fi) | `nvs_flash` | `nvs_flash.h` |
| ADC | `esp_adc` | `esp_adc/adc_oneshot.h`, `esp_adc/adc_cali.h`, `esp_adc/adc_cali_scheme.h` |
| PWM | `esp_driver_ledc` | `driver/ledc.h` |
| GPIO | `esp_driver_gpio` | `driver/gpio.h` |
| Tempo | `esp_timer` | `esp_timer.h` |
| Tarefas, filas, semáforos | `freertos` | `freertos/FreeRTOS.h`, `freertos/task.h`, `freertos/queue.h`, `freertos/semphr.h` |
| Log | `log` | `esp_log.h` |

**Regras de build do v6:** avisos do compilador são erros por padrão; o código deve compilar **sem nenhum warning**. A libc padrão é **Picolibc**: não imprimir `float` com `printf`/`ESP_LOGx` (usar inteiros). `idf.py flash -a` força a gravação completa em placa nova.

**Regra de projeto:** toda decisão (mistura, zona morta, rampa, validação de pacote, transição de estado) fica em `components/logic` como funções puras que recebem `now_ms` por parâmetro. O `main.c` só faz E/S (ADC, GPIO, PWM, ESP-NOW, log).

API sugerida para `logic` (ajustável, mantendo a lógica sem dependência do IDF):

```c
// joystick.h
int8_t joystick_normalize(int raw, int center, int raw_min, int raw_max, bool invert); // -100..100
int8_t joystick_deadzone(int8_t v, uint8_t deadzone_pct);                               // reescala fora da zona
// mixer.h
typedef struct { int8_t left, right; } motor_cmd_t;
motor_cmd_t mixer_mix(int8_t x, int8_t y);                                              // clamp +-100
// ramp.h  (estado por motor)
int8_t ramp_step(ramp_t *r, int8_t target, uint32_t now_ms);                            // freio imediato, rampa na subida
// link.h
link_verdict_t link_check_cmd(link_rx_t *rx, const uint8_t *data, size_t len, uint32_t now_ms);
// state_machine.h
void car_sm_step(car_sm_t *sm, const car_inputs_t *in, car_outputs_t *out);             // seção 8.3
```

## 5. Hardware e pinagem

### 5.1 Controle (ESP32 #1) **[CONFIRMAR]**

| Função | GPIO | Observação |
| --- | --- | --- |
| Joystick eixo X (VRx) | 34 | **ADC1**, 3,3 V. Obter unidade e canal com `adc_oneshot_io_to_channel()`; não fixar número de canal no código. |
| Joystick eixo Y (VRy) | 35 | ADC1. |
| LED de estado | 2 | LED onboard. |

O botão SW do joystick vai no **GPIO 32** (entrada com pull-up interno, ativo em 0). Usar **só ADC1** nos eixos: o ADC2 é compartilhado com o Wi-Fi e a leitura pode retornar `ESP_ERR_TIMEOUT`.

### 5.2 Carrinho (ESP32 #2) **[CONFIRMAR]**

| Função | GPIO | Observação |
| --- | --- | --- |
| Ponte H ENA (PWM motor esquerdo) | 25 | Remover o jumper de ENA/ENB do L298N para controlar por PWM. |
| Ponte H IN1 / IN2 (esquerdo) | 26 / 27 | Sentido. |
| Ponte H ENB (PWM motor direito) | 13 | |
| Ponte H IN3 / IN4 (direito) | 32 / 33 | Sentido. |
| HC-SR04 TRIG | 4 | |
| HC-SR04 ECHO | 18 | **5 V: passar por divisor resistivo** antes do ESP32 (3,3 V máx.). |
| Tensão da bateria (ADC) | 34 | ADC1, via divisor resistivo. |
| Buzzer (buzina) | 19 | PWM do LEDC (`LEDC_TIMER_1`, canal 2), tom de 2 kHz. Buzzer magnético: ligar por transistor. |

Alimentação (README e desafio D1): ponte H e motores direto na bateria; os dois ESP32 e o sensor em trilho de 5 V por step-down; GND comum; capacitores de desacoplamento. Recomendado: resistores de 10 kΩ para GND em IN1 a IN4, ENA e ENB, para os motores não se moverem durante o boot. Se a ponte for TB6612, usar o mesmo mapa (PWMA/AIN1/AIN2, PWMB/BIN1/BIN2) e ligar STBY em nível alto.

Polaridade e orientação ajustáveis em `config.h`: `INVERT_X`, `INVERT_Y` (controle) e `INVERT_LEFT`, `INVERT_RIGHT` (carrinho).

## 6. Comunicação (ESP-NOW)

**Inicialização Wi-Fi + ESP-NOW (ordem):** a documentação v6.1 só fixa que o Wi-Fi deve estar iniciado antes de `esp_now_init()`. A sequência abaixo vem do uso comum e **precisa ser validada** (seção 12):

1. `nvs_flash_init()` (ou `esp_wifi_set_storage(WIFI_STORAGE_RAM)` para não depender de NVS)
2. `esp_netif_init()` e `esp_event_loop_create_default()`
3. `esp_wifi_init(&cfg)` com `WIFI_INIT_CONFIG_DEFAULT()` (chamar duas vezes devolve `ESP_ERR_INVALID_STATE` no v6)
4. `esp_wifi_set_mode(WIFI_MODE_STA)` e `esp_wifi_start()` (sem `esp_wifi_connect()`)
5. `esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE)`, **somente depois de `esp_wifi_start()`**. O canal não é salvo na NVS.
6. `esp_now_init()`, registrar os callbacks, `esp_now_add_peer()` com `channel = 0` (canal atual), `ifidx = WIFI_IF_STA`, `encrypt = false`.

**Callbacks (assinaturas do v6.1, diferentes das de muitos exemplos):**

```c
void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int data_len);   // info->src_addr, info->des_addr, info->rx_ctrl
void on_send(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);      // 'status' será removido: preferir tx_info->tx_status
```

- Os dois callbacks rodam na **task de Wi-Fi de alta prioridade**: não fazer trabalho longo. Copiar o pacote para uma **fila FreeRTOS** e processar em outra tarefa. O ponteiro `info` só vale dentro do callback.
- Não enviar o próximo pacote antes de o callback de envio anterior ter ocorrido. Com 20 Hz e 5 Hz isso não é problema.
- O callback de envio só registra falhas, sem alterar estados nem bloquear.

**Endereços:** MACs em `config.h` (`CONTROLLER_MAC`, `CAR_MAC`). Cada firmware imprime o próprio MAC no boot (`esp_wifi_get_mac(WIFI_IF_STA, mac)`). O carrinho **só aceita** pacotes cujo `info->src_addr` seja `CONTROLLER_MAC`.

**Pacotes** (`components/protocol/include/protocol.h`, `__attribute__((packed))`, little-endian, bem abaixo de `ESP_NOW_MAX_DATA_LEN` = 250):

```c
#define PROTO_MAGIC 0xA5
typedef enum { PKT_CMD = 1, PKT_TELEM = 2 } pkt_type_t;
typedef enum { ST_INIT = 0, ST_PARADO, ST_OPERANDO, ST_OBSTACULO, ST_FALHA_COM } car_state_t;

typedef struct __attribute__((packed)) {   // controle -> carrinho, 20 Hz
  uint8_t  magic;   // PROTO_MAGIC
  uint8_t  type;    // PKT_CMD
  uint16_t seq;     // incrementa a cada pacote, com wraparound
  int8_t   x;       // -100..100 (+ = direita)
  int8_t   y;       // -100..100 (+ = frente)
  uint8_t  buttons; // bit 0 = PROTO_BTN_HORN (buzina); demais bits = 0
  uint8_t  crc;     // CRC-8 de todos os bytes anteriores
} cmd_packet_t;

typedef struct __attribute__((packed)) {   // carrinho -> controle, 5 Hz
  uint8_t  magic;
  uint8_t  type;          // PKT_TELEM
  uint16_t seq;
  uint8_t  state;         // car_state_t
  uint8_t  reserved;      // 0
  uint16_t distance_cm;   // 0xFFFF = leitura inválida
  uint16_t battery_mv;    // informativo, sem ação automática
  uint16_t rejected_cnt;  // total de comandos/pacotes rejeitados
  uint8_t  crc;
} telem_packet_t;
```

**CRC-8:** polinômio 0x07, valor inicial 0x00, sem reflexão. `x` e `y` chegam normalizados, com zona morta aplicada, inteiros em −100..100. A mistura e o PWM são feitos **no carrinho**.

## 7. Firmware do controle (fase 2)

1. **Calibração no boot:** com o joystick em repouso, média de ~500 ms de leituras (por exemplo 25 amostras a cada 20 ms) para obter o centro de cada eixo. Se o centro ficar fora de 1500–2600 (12 bits), logar aviso e continuar. Extremos: 0 e o máximo do ADC, com escalas separadas para o lado positivo e o negativo, para que o centro físico vire 0.
2. **ADC (oneshot):** `adc_oneshot_new_unit()` → `adc_oneshot_config_channel()` com `atten = ADC_ATTEN_DB_12` e `bitwidth = ADC_BITWIDTH_DEFAULT` → `adc_oneshot_read()`. Média curta de 4 amostras. Tratar `ESP_ERR_TIMEOUT` repetindo a leitura.
3. **Normalização:** `v` em −100..100 por eixo; aplicar `INVERT_*`.
4. **Zona morta de 10%:** se `|v| < 10` então `v = 0`; senão reescalar `(|v| − 10) × 100 / 90` mantendo o sinal, para não haver salto na saída da zona morta.
5. **Envio a cada 50 ms** (20 Hz), sempre, mesmo com x = y = 0, por um `esp_timer` periódico (`esp_timer_create` + `esp_timer_start_periodic(timer, 50000)`, despacho `ESP_TIMER_TASK`). Incrementar `seq`. O callback do timer é curto e não bloqueia.
6. **Telemetria recebida:** o callback copia o pacote para uma fila; uma tarefa de UI valida o CRC e faz o log com `ESP_LOGI`, por exemplo `estado=OPERANDO dist=42cm bat=7430mV rej=0`. Sem telemetria por mais de 1 s, logar `SEM TELEMETRIA` uma vez por segundo.
7. **LED (GPIO 2), pela tarefa de UI:** PARADO aceso fixo; OPERANDO pisca lento (1 Hz); OBSTACULO pisca rápido (4 Hz); FALHA_COM duplo piscar a cada 1 s; sem telemetria, apagado.
8. **Teste T4 (opção Kconfig `FW_TEST_T4`):** quando ligada, 10 s após o boot o controle envia **uma vez** a sequência de pacotes inválidos (sequência antiga, `x = 120`, CRC errado, 3 s de frente máxima, inversão direta de `y = +100` para `y = −100`) e volta ao funcionamento normal. Fora do build de teste esse código não existe (`sdkconfig.defaults.t4` adicional em `SDKCONFIG_DEFAULTS`).

## 8. Firmware do carrinho

### 8.1 Tarefas e temporização

Tudo não bloqueante, sem `vTaskDelay` dentro da lógica de controle.

| O quê | Período | Onde |
| --- | --- | --- |
| Laço de controle (fila de comandos, estado, mistura, rampa, motores) | 10 ms | `esp_timer` periódico (`ESP_TIMER_TASK`) |
| Medição do HC-SR04 | 60 ms | **tarefa FreeRTOS separada**; resultado em fila de tamanho 1 (`xQueueOverwrite`) lida pelo laço de controle |
| Telemetria | 200 ms | contador dentro do laço de controle |
| Leitura da bateria | 200 ms | média de 8 amostras; `adc_cali_raw_to_voltage()` e `BATT_DIV_RATIO_X1000` **[CONFIRMAR]** |

Todo o estado do carrinho é lido e escrito **apenas** pelo laço de controle (sem mutex). Callback ESP-NOW e tarefa do sensor só colocam dados em filas.

**HC-SR04 sem `esp_rom_delay_us`:** essa função está na lista de APIs internas e instáveis do v6.1 e não deve ser usada (RNF05). O pulso de TRIG de 10 µs é feito com `gpio_set_level(TRIG, 1)`, espera ativa lendo `esp_timer_get_time()` até passarem ≥ 10 µs, e `gpio_set_level(TRIG, 0)`. A largura do eco é medida por **interrupção de GPIO** no ECHO (`GPIO_INTR_ANYEDGE`, `gpio_install_isr_service()`, `gpio_isr_handler_add()`): na borda de subida guarda `esp_timer_get_time()`; na de descida calcula a largura e libera um semáforo binário com `xSemaphoreGiveFromISR()`. A tarefa do sensor espera o semáforo com timeout de 30 ms. Distância em cm = `largura_us / 58`. **Sem eco = livre** (decisão do grupo, 19/11): se o ECHO subiu mas o timeout estourou, ou o pulso passou de 400 cm, a leitura é válida com `SENSOR_NO_ECHO_FAR_CM` (400 cm). Se o ECHO nem subiu (sensor desligado ou ECHO solto), a leitura é inválida. Se o ECHO ainda está alto na hora do disparo (pulso longo do ciclo anterior), não dispara e conta como livre por até `SENSOR_ECHO_BUSY_MAX_CYCLES` (5) ciclos; acima disso é ECHO preso, inválido.

### 8.2 Validação de pacote (RF06)

O callback ESP-NOW só copia `{origem, dados, tamanho, rx_ms}` para uma fila (profundidade 8; se cheia, descarta e conta). O laço de controle avalia, nesta ordem; cada falha incrementa `rejected_cnt`:

1. Origem diferente de `CONTROLLER_MAC`, tamanho ≠ `sizeof(cmd_packet_t)`, `magic` ou `type` errados, CRC errado → descartar. **Não** renova o temporizador de enlace.
2. Sequência antiga ou repetida: aceitar só se `(int16_t)(seq − last_seq) > 0`; o primeiro pacote após boot sempre é aceito. Descartar, sem renovar o enlace.
3. `x` ou `y` fora de −100..100, ou bit de `buttons` fora de `PROTO_BTN_MASK` → descartar, sem renovar o enlace.
4. Pacote válido: atualizar `last_seq` e `last_rx_ms`. A **regra de comando por estado** (8.3) decide se o comando é usado ou rejeitado (também conta em `rejected_cnt`, mas o enlace segue vivo).

### 8.3 Máquina de estados

Estados: `ST_INIT, ST_PARADO, ST_OPERANDO, ST_OBSTACULO, ST_FALHA_COM`.

Sinais usados:
- `link_ok`: `now − last_rx_ms ≤ 300 ms`.
- `centered`: último comando válido com x = 0 e y = 0.
- `obstacle`: leitura válida ≤ 15 cm, **ou** sensor inválido. Sai com histerese: leitura válida > 20 cm **e** 3 leituras válidas seguidas.
- Sensor inválido: 3 ciclos seguidos sem resposta do sensor (ECHO não sobe ou preso) ou fora de faixa (< 2 cm ou > 400 cm). Volta a válido com 3 leituras válidas seguidas.

| De | Para | Condição |
| --- | --- | --- |
| (boot) | INIT | Inicializa GPIO, LEDC, ADC, Wi-Fi/ESP-NOW, tarefa do sensor. Motores em 0. |
| INIT | PARADO | Autoteste OK (periféricos inicializados, ADC da bateria lido). Começa **não armado**. |
| qualquer ≠ INIT | FALHA_COM | `!link_ok` (inclui não ter recebido pacote 300 ms após o INIT). |
| FALHA_COM | PARADO | `link_ok` e `centered`. Rearma. |
| PARADO | (armar) | Pacote válido com neutro marca `armed = true`. |
| PARADO | OPERANDO | Armado e comando válido ≠ neutro. |
| OPERANDO | PARADO | `centered`. |
| PARADO ou OPERANDO | OBSTACULO | `obstacle` verdadeiro. |
| OBSTACULO | PARADO | `obstacle` falso **e** `centered`. |

Comportamento por estado:

| Estado | Saída dos motores | Comandos rejeitados |
| --- | --- | --- |
| INIT | Parados (freio). | Todos. |
| PARADO | Parados. | Sequência antiga, valor fora de faixa. Comando ≠ neutro com `armed = false` é ignorado (e conta). |
| OPERANDO | Mistura normal com rampa. | Sequência antiga, valor fora de faixa. Inversão frente/ré sem passar por zero (ver rampa). |
| OBSTACULO | Entra parando imediatamente. Depois, com `y = min(y, 0)`: ré e giro livres, avanço bloqueado. | Todo avanço (conta em `rejected_cnt`). |
| FALHA_COM | Parados (freio). | Todos, até `link_ok` e `centered`. |

### 8.4 Mistura, rampa e motores

- **Mistura:** `left = clamp(y + x, −100, 100)`, `right = clamp(y − x, −100, 100)`.
- **Parada é imediata:** neutro, entrada em OBSTACULO, entrada em FALHA_COM e INIT zeram a saída na hora (sem rampa) e acionam freio, para atender RNF01, RNF02 e RNF04.
- **Rampa só na subida:** aumento de módulo limitado a `RAMP_UP_PCT_PER_S = 200` (0→100% em 0,5 s), calculada pelo tempo decorrido. Diminuição de módulo é imediata.
- **Intertravamento de inversão:** se o sinal do alvo de um motor é oposto ao da saída atual, a saída vai a 0, fica `REVERSE_DWELL_MS = 50` e só então sobe pela rampa no novo sentido.
- **PWM mínimo:** módulo 1..100% é mapeado para `PWM_MIN_PCT..100%` (padrão 60, ajustar na bancada). 0 continua 0.
- **PWM com LEDC:** `ledc_timer_config()` com `LEDC_TIMER_0`, `freq_hz = 5000`, `duty_resolution = LEDC_TIMER_8_BIT`; dois canais (`ledc_channel_config()`, um por ENA/ENB) no mesmo timer. Duty 0..255 com `ledc_set_duty()` seguido de `ledc_update_duty()`. Modo de velocidade e `clk_cfg`: **[validar]** (seção 12.2).
- **Sentido (GPIO):** + → IN1=H, IN2=L; − → IN1=L, IN2=H.
- **Freio:** IN1 = IN2 = HIGH com EN em duty 255. Função única `motors_brake()`.
- Aplicar `INVERT_LEFT` / `INVERT_RIGHT` na camada de saída.

### 8.5 Telemetria (carrinho → controle)

A cada 200 ms enviar `telem_packet_t` ao `CONTROLLER_MAC` com `esp_now_send()` (peer registrado no boot). `distance_cm = 0xFFFF` quando o sensor está inválido. Falha de envio não pode travar nem alterar o estado.

### 8.6 Registro para medição dos testes

`ESP_LOGI` com timestamp em ms: `EVT link_lost`, `EVT center_detected`, `EVT obstacle_enter dist=<cm>`, `EVT motors_zero`, `EVT state <de>-><para>`. O intervalo `center_detected → motors_zero` mede o RNF04 e `link_lost → motors_zero` o RNF01. Alternar também `DEBUG_PIN` (**[CONFIRMAR]** GPIO 23) quando os motores chegam a zero, para analisador lógico. O log da UART é lento: eventos vão para uma fila e são impressos fora do laço de controle.

## 9. Testes

### 9.1 Unitários da lógica (`test_apps/logic_host`)

Aplicação de teste com o componente `unity` do ESP-IDF, compilando só `main`, `logic`, `protocol` e `unity`.

- **No PC (alvo linux):** `idf.py --preview set-target linux`, com `set(COMPONENTS main logic protocol unity)`; depois `idf.py build` e executar o `.elf`. A documentação marca esse recurso como **experimental/preview**; exige `libbsd` e `libbsd-dev`.
- **No ESP32:** o mesmo app com `set-target esp32` e `idf.py flash monitor` (plano B).

Obrigatórios:
- Normalização e zona morta: centro → 0; ±9% → 0; ±10% → 0; ±100% → ±100; reescala contínua na borda.
- Mistura: (y=100, x=0) → (100, 100); (y=0, x=100) → (100, −100); (y=100, x=100) → clamp em (100, 0); (y=−50, x=0) → (−50, −50).
- Rampa: subida limitada pelo tempo; descida imediata; troca de sinal passa por 0 e espera `REVERSE_DWELL_MS`.
- CRC-8: vetores conhecidos e pacote com 1 bit trocado rejeitado.
- Sequência: crescente aceita; repetida e antiga rejeitadas; wraparound 65535→0 aceito; primeiro pacote aceito.
- Validação: `x = 120` e `y = −120` rejeitados.
- Máquina de estados: todas as transições da tabela 8.3; FALHA_COM aos 300 ms sem pacote e **não** aos 299 ms; saída de FALHA_COM e de OBSTACULO só com `centered`; avanço bloqueado em OBSTACULO e ré liberada; histerese 15/20 cm; sensor inválido após 3 ciclos e recuperado após 3 leituras válidas; boot sem controle vai a FALHA_COM; boot com joystick fora do centro só arma depois do neutro.

### 9.2 Aceitação em bancada

| Teste | Requisito | Procedimento resumido | Critério |
| --- | --- | --- | --- |
| T1 | RF05, RF07, RNF01 | Carrinho em movimento, desligar o controle, cronometrar até parar; religar e verificar retomada. 10 repetições. | Parada ≤ 500 ms; só retoma com o joystick no centro. |
| T2 | RF04, RNF02 | Aproximar de um anteparo à velocidade máxima e medir a distância final. 10 repetições. | ≥ 8 cm em 10 de 10. |
| T3 | RNF03 | 20 partidas simultâneas dos motores. | Nenhuma reinicialização do ESP32. |
| T4 | RF06 | Build do controle com `FW_TEST_T4`. | Todos rejeitados e contados em `rejected_cnt`. |
| T5 | RF04 (sensor inválido) | Desconectar o sensor com o carrinho em avanço. | Avanço bloqueado em ≤ 500 ms; recupera ao reconectar (e joystick no centro). |
| T6 | RF03, RNF04 | Em movimento, soltar o joystick e medir da entrada na zona morta até parar. 10 repetições. | ≤ 300 ms em 10 de 10. |

Observação para o T2: a distância final depende da velocidade máxima e do período do sensor (60 ms); `OBSTACLE_ENTER_CM` (15), `OBSTACLE_EXIT_CM` (20) e `MAX_PWM_PCT` ficam em `config.h` para ajuste depois da primeira medição.

## 10. Parâmetros de `config.h` (valores padrão)

```c
// comuns
#define CHANNEL                 1
#define CMD_PERIOD_MS           50     // 20 Hz
#define TELEM_PERIOD_MS         200    // 5 Hz
#define LINK_TIMEOUT_MS         300
#define DEADZONE_PCT            10
// carrinho
#define CONTROL_PERIOD_MS       10
#define SENSOR_PERIOD_MS        60
#define SENSOR_ECHO_TIMEOUT_MS  30
#define OBSTACLE_ENTER_CM       15
#define OBSTACLE_EXIT_CM        20
#define SENSOR_VALID_MIN_CM     2
#define SENSOR_VALID_MAX_CM     400
#define SENSOR_FAIL_CYCLES      3
#define SENSOR_RECOVER_CYCLES   3
#define PWM_FREQ_HZ             5000
#define PWM_MIN_PCT             60
#define MAX_PWM_PCT             100
#define RAMP_UP_PCT_PER_S       200
#define REVERSE_DWELL_MS        50
#define BATT_DIV_RATIO_X1000    3000   // inteiro (3,000), CONFIRMAR após medir o divisor
```

## 11. Restrições e armadilhas

- **Só ADC1** em qualquer leitura analógica (o ADC2 é compartilhado com o Wi-Fi).
- Evitar pinos de strapping (GPIO 0, 2, 5, 12, 15) para saídas de potência no carrinho.
- **Sem eco = livre** (decidido pelo grupo em 19/11, no lugar de "inválido" da proposta): em espaço aberto, sem nada a menos de ~4 m, o carrinho continua andando. Continua **sensor inválido** (bloqueia o avanço, RF04) quando o ECHO nem sobe após o disparo ou fica preso em nível alto (seção 8.1).
- Nada de bloqueio, alocação dinâmica ou `printf` pesado no laço de controle, nos callbacks de ESP-NOW nem na ISR de GPIO.
- Sem `float` em log (Picolibc). Usar `PRIu32` e afins.
- Em qualquer reinício do carrinho, motores começam parados e o estado começa em INIT.
- `ESP_ERROR_CHECK` nos inits; em operação, tratar erros de envio e leitura sem abortar.

## 12. APIs validadas (RNF05)

Verificação feita em 09/11/2026 nas páginas oficiais do ESP-IDF **v6.1** (`docs.espressif.com/projects/esp-idf/en/v6.1/`) e nos guias de migração 5.5→6.0→6.1. O MCP `espressif-documentation` não estava disponível naquela sessão. O agente deve repetir a validação com `search_espressif_sources` e registrar o resultado no README.

### 12.1 Confirmadas na documentação v6.1

| API | Assinatura / fato confirmado |
| --- | --- |
| `esp_now_init` | `esp_err_t esp_now_init(void)` |
| `esp_now_register_recv_cb` | callback `void (const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len)`; `esp_now_recv_info_t` tem `src_addr`, `des_addr`, `rx_ctrl` |
| `esp_now_register_send_cb` | callback `void (const esp_now_send_info_t *tx_info, esp_now_send_status_t status)`; `esp_now_send_info_t` é `wifi_tx_info_t`; `status` "será removido", usar `tx_info->tx_status` |
| `esp_now_add_peer`, `esp_now_peer_info_t` | `peer_addr[ESP_NOW_ETH_ALEN]`, `lmk`, `channel` (0 = canal atual), `ifidx`, `encrypt`, `priv` |
| `esp_now_send` | `esp_err_t (const uint8_t *peer_addr, const uint8_t *data, size_t len)`; peer precisa ter sido adicionado; `ESP_NOW_MAX_DATA_LEN` = 250 (v1) |
| Contexto dos callbacks | Task de Wi-Fi de alta prioridade; máximo de 20 peers |
| `esp_wifi_init`, `esp_wifi_set_storage`, `esp_wifi_set_mode`, `esp_wifi_start` | Assinaturas confirmadas; segunda chamada de `esp_wifi_init` retorna `ESP_ERR_INVALID_STATE` (v6.0) |
| `esp_wifi_set_channel` | `esp_err_t (uint8_t primary, wifi_second_chan_t second)`; chamar após `esp_wifi_start()`; não salva na NVS |
| `esp_wifi_get_mac` | `esp_err_t (wifi_interface_t ifx, uint8_t mac[6])` |
| ADC oneshot | `adc_oneshot_new_unit`, `adc_oneshot_config_channel`, `adc_oneshot_read` (pode dar `ESP_ERR_TIMEOUT`; não usar em ISR), `adc_oneshot_io_to_channel`; `adc_oneshot_unit_init_cfg_t`, `adc_oneshot_chan_cfg_t` (`atten`, `bitwidth`); `ADC_ATTEN_DB_12`, `ADC_BITWIDTH_DEFAULT`; `esp_adc/adc_oneshot.h`, componente `esp_adc` |
| ADC calibração | `esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t handle, int raw, int *voltage)`; `adc_cali_line_fitting_config_t`; `adc_cali_create_scheme_line_fitting`; `esp_adc/adc_cali.h` |
| LEDC | `ledc_timer_config`, `ledc_channel_config`, `ledc_set_duty`, `ledc_update_duty`, `ledc_stop`; campos de `ledc_timer_config_t` (`speed_mode`, `duty_resolution`, `timer_num`, `freq_hz`, `clk_cfg`) e `ledc_channel_config_t` (`gpio_num`, `speed_mode`, `channel`, `timer_sel`, `duty`, `hpoint`); `intr_type` deprecated; componente `esp_driver_ledc` |
| GPIO | `gpio_config`, `gpio_set_level`, `gpio_get_level`, `gpio_install_isr_service(int)`, `gpio_isr_handler_add(gpio_num_t, gpio_isr_t, void*)`; `GPIO_INTR_ANYEDGE`; componente `esp_driver_gpio` |
| `esp_timer` | `esp_timer_get_time()` (µs, usável em ISR), `esp_timer_create`, `esp_timer_start_periodic(timer, period_us)`, `esp_timer_stop`, `esp_timer_delete`; `esp_timer_create_args_t`; `ESP_TIMER_TASK` / `ESP_TIMER_ISR` |
| Alvo linux | Experimental; `unity` e `freertos` têm simulação; `esp_timer` só mock |

### 12.2 Não verificadas (o agente deve validar com o MCP antes de usar)

- Sequência exata de inicialização do Wi-Fi para uso só com ESP-NOW (NVS, `esp_netif_init`, `esp_event_loop_create_default`, `esp_wifi_set_storage`); conferir também o exemplo oficial de ESP-NOW do v6.1.
- Componente e header de `esp_now.h` e os tipos `wifi_second_chan_t` e `wifi_tx_info_t` (campo `tx_status`).
- Enumeradores do LEDC: `LEDC_LOW_SPEED_MODE` no ESP32 (a página confirma que o modo de alta velocidade só existe no ESP32) e `clk_cfg` (`LEDC_AUTO_CLK` **não aparece** na página lida).
- Largura de bits do ADC no ESP32 com `ADC_BITWIDTH_DEFAULT` (esperado 12 bits) e o mapeamento GPIO34/35 → canais ADC1 (usar `adc_oneshot_io_to_channel()`).
- Assinatura formal de `adc_cali_create_scheme_line_fitting` e `adc_cali_delete_scheme_line_fitting`.
- `MACSTR`/`MAC2STR` (`esp_mac.h`), chamadas do FreeRTOS (`xQueueCreate`, `xQueueOverwrite`, `xSemaphoreCreateBinary`, `xSemaphoreGiveFromISR`, `xTaskCreate`), macros do Unity, `Kconfig.projbuild`, `SDKCONFIG_DEFAULTS` e `EXTRA_COMPONENT_DIRS`.

### 12.3 O que mudou no v6.x e quebra código antigo

| Mudança | Efeito neste projeto |
| --- | --- |
| Callback de envio do ESP-NOW recebe `const esp_now_send_info_t *tx_info` em vez do MAC | Exemplos antigos não compilam |
| `driver` não depende mais dos `esp_driver_*` (v6.0) | Declarar `esp_driver_gpio` e `esp_driver_ledc` no CMake |
| Drivers legados removidos (`driver/adc.h`, `driver/timer.h`, `driver/mcpwm.h`, `driver/rmt.h` etc.) | Usar `esp_adc` e os drivers novos |
| FreeRTOS: `vTaskDelayUntil` removido (usar `xTaskDelayUntil`); headers públicos não incluem FreeRTOS implicitamente | Incluir `freertos/*.h` explicitamente |
| Picolibc como libc padrão | Não imprimir `float` |
| Avisos do compilador viram erros | Código sem warnings |
| `esp_rom_delay_us` consta como API interna e instável | Não usar |

## 13. Fora de escopo

- Servo de direção, segundo joystick e botão de parada (descartados nesta versão).
- Reação automática a bateria baixa (só leitura e telemetria).
- Segundo motor "extra" mencionado no início do projeto: **não usado** nesta versão **[CONFIRMAR]**.
- Criptografia do enlace, pareamento dinâmico, app de celular.
- Arduino, PlatformIO e bibliotecas de terceiros.

## 14. Ordem de implementação e entregáveis

Implementar nesta ordem, cada etapa compilando e testada antes da próxima:

1. `components/protocol` e `components/logic` com os testes da seção 9.1 (sem hardware).
2. **Carrinho sem rádio:** motores, mistura, rampa e freio, com comandos fixos no código (modo de bancada, 15.4).
3. **Carrinho com sensor:** HC-SR04, histerese e estado OBSTACULO (T2).
4. **Controle mínimo + ESP-NOW:** só o envio de x e y a 20 Hz com CRC.
5. **Carrinho recebendo:** validação de pacote, FALHA_COM e rearme (T1, T4).
6. **Controle completo:** calibração, zona morta, LED e telemetria.

Entregáveis:
1. Dois projetos IDF (`controller` e `car`) que compilam com ESP-IDF v6.1 para o alvo `esp32`, **sem warnings**.
2. `test_apps/logic_host` com os testes da seção 9.1 passando (alvo linux ou, se necessário, no ESP32).
3. `README.md` com: versão do ESP-IDF, tabela de ligações, como obter os MAC, calibração do joystick, ajuste de `PWM_MIN_PCT`, como executar T1 a T6, a seção **"APIs validadas"** (RNF05) e a seção **"Suposições"**.
4. Código comentado em português, com os IDs RF/RNF citados onde cada requisito é atendido.

## 15. Anexo: esqueletos de build, Kconfig e modo de bancada

Os trechos abaixo são ponto de partida. Os nomes de componentes e opções de Kconfig seguem o uso comum do ESP-IDF e **devem ser validados** (seção 12.2); se algum não existir no v6.1, ajustar e registrar no README.

### 15.1 CMake

`car/CMakeLists.txt` (o mesmo vale para `controller/`, trocando o nome do projeto):
```cmake
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(car)
```
Se `car/` não estiver diretamente dentro da raiz do trabalho, ajustar o caminho de `components`.

`components/logic/CMakeLists.txt` (C puro, sem dependência do IDF; o mesmo vale para `protocol`):
```cmake
idf_component_register(SRCS "joystick.c" "mixer.c" "ramp.c" "link.c" "state_machine.c"
                       INCLUDE_DIRS "include"
                       REQUIRES protocol)
```
`car/main/CMakeLists.txt`:
```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES logic protocol
                       PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_adc
                                     esp_driver_ledc esp_driver_gpio esp_timer freertos log)
```

### 15.2 `sdkconfig.defaults` do carrinho (mínimo)

```
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_FREERTOS_HZ=1000
```
Não definir nenhuma opção que o agente não tenha validado. `sdkconfig.defaults.bench` (modo de bancada) acrescenta apenas `CONFIG_CAR_BENCH_MODE=y`. Para compilar em modo de bancada: `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.bench" build` (apagar o `sdkconfig` gerado ao alternar entre os dois modos).

### 15.3 `car/main/Kconfig.projbuild`

```
menu "Carrinho"
    config CAR_BENCH_MODE
        bool "Modo de bancada (sem controle, comandos simulados)"
        default n
endmenu
```

### 15.4 Modo de bancada do carrinho (`CONFIG_CAR_BENCH_MODE`)

Serve para testar o carrinho sozinho nas etapas 2 e 3 da seção 14, **sem ESP-NOW e sem controle**. Regras:
- O Wi-Fi/ESP-NOW **não é iniciado** (nem o peer), e o `CONTROLLER_MAC` não é exigido.
- Uma tarefa de simulação **injeta `cmd_packet_t` válidos na mesma fila** que o callback ESP-NOW usaria, a 20 Hz, com `seq` crescente e CRC correto. Assim, validação, máquina de estados, mistura e motores rodam pelo **mesmo caminho** do uso real.
- A sequência, em laço, começa 3 s após o boot (para dar tempo de posicionar o carrinho) e repete:

| Passo | Duração | `x` | `y` | Efeito esperado |
| --- | --- | --- | --- | --- |
| 1. Zero (arma) | 2 s | 0 | 0 | Parado |
| 2. Frente suave | 3 s | 0 | +40 | Os dois motores para a frente |
| 3. Zero | 2 s | 0 | 0 | Parada imediata |
| 4. Ré suave | 3 s | 0 | −40 | Os dois motores para trás |
| 5. Zero | 2 s | 0 | 0 | Parada imediata |
| 6. Curva à direita | 3 s | +40 | +60 | Motor esquerdo mais rápido que o direito |
| 7. Zero | 2 s | 0 | 0 | Parada imediata |
| 8. Giro no lugar | 3 s | +60 | 0 | Esquerdo para frente, direito para trás |
| 9. Zero | 2 s | 0 | 0 | Parada imediata |

- O HC-SR04 e a máquina de estados funcionam normalmente: um obstáculo a 15 cm ou menos deve colocar o carrinho em OBSTACULO e bloquear os passos 2 e 6 (a ré do passo 4 e o giro do passo 8 continuam liberados).
- Cada passo e cada transição de estado são logados (`EVT bench_step <n>`, `EVT state <de>-><para>`).
- A telemetria é **só logada** na serial (não enviada).
- Esse código existe **apenas** com `CONFIG_CAR_BENCH_MODE=y`; no build normal ele não é compilado.
- **Aviso no README:** rodar a sequência com as rodas **fora do chão** (carrinho levantado) na primeira vez.

### 15.5 Esqueleto do controle (fase 1)

`controller/main/main.c`: `app_main` que inicializa Wi-Fi em modo STA, imprime o MAC com `esp_wifi_get_mac()` e `ESP_LOGI`, e entra em `vTaskDelay` infinito. Serve só para obter o MAC e confirmar que o ambiente compila; o firmware real é a fase 2.
