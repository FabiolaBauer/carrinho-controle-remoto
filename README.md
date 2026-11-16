# Carrinho controlado por joystick (ESP32, ESP-IDF v6.1)

Firmware dos dois ESP32 do trabalho de Sistemas Embarcados (Grau B): **controle** (1 joystick) e **carrinho** (ponte H, 2 motores DC, HC-SR04, leitura de bateria). Comunicação por ESP-NOW. A referência técnica é o [ESPECIFICACAO_CODIGO.md](ESPECIFICACAO_CODIGO.md).

**Situação atual:**
- O **carrinho** está implementado e revisado. Ele compila, mas **ainda não foi testado no hardware**.
- O **controle** ainda é só um esqueleto que loga o MAC (fase 2, pendente).

---

## 1. Versão do ESP-IDF

**ESP-IDF v6.1**, linguagem C, alvo `esp32`, sem Arduino e sem componentes de terceiros.

Abra o VS Code a partir do *ESP-IDF PowerShell* (ou rode `. C:\esp\v6.1\esp-idf\export.ps1` antes) e confira a versão:

```powershell
python $env:IDF_PATH\tools\idf.py --version   # deve mostrar: ESP-IDF v6.1
```

> Nesta instalação, `idf.py --version` sozinho mostra `v1.0.3`. Esse número é a versão do lançador `idf.py.exe` do instalador, não a do IDF. Para ver a versão do IDF, use o comando acima ou abra `C:\esp\v6.1\esp-idf\version.txt`.

---

## 2. Estrutura

As pastas dos projetos já existiam com nomes em português e **não foram renomeadas**. Onde a especificação diz `car/`, leia `carrinho/`; onde diz `controller/`, leia `controle/`.

```
trabalhoGB/
├── components/
│   ├── protocol/              # pacotes, CRC-8, estados (C puro)
│   └── logic/                 # joystick, mixer, rampa, link, máquina de estados (C puro)
│       └── include/config.h   # TODOS os parâmetros e pinos
├── carrinho/                  # projeto IDF do carrinho (nome do projeto: car)
│   ├── sdkconfig.defaults
│   ├── sdkconfig.defaults.bench
│   └── main/  main.c (só E/S), motors.c, sonar.c, battery.c, bench.c, Kconfig.projbuild
├── controle/                  # projeto IDF do controle (nome do projeto: controller), fase 2
├── test_apps/logic_host/      # testes Unity da lógica (seção 9.1)
└── README.md
```

A lógica de decisão (validação de pacote, máquina de estados, mistura, rampa) fica em `components/logic`, em funções puras que recebem `now_ms`. O `main.c` do carrinho só faz E/S.

---

## 3. Ligações (seção 5) **[CONFIRMAR]**

Todos os pinos estão em `components/logic/include/config.h`.

### 3.1 Carrinho (ESP32 #2)

| Função | GPIO | Constante | Observação |
| --- | --- | --- | --- |
| Ponte H ENA (PWM motor esquerdo) | 25 | `CAR_PIN_MOTOR_LEFT_PWM` | Tirar o jumper de ENA do L298N |
| Ponte H IN1 / IN2 (esquerdo) | 26 / 27 | `CAR_PIN_MOTOR_LEFT_IN1/IN2` | Sentido |
| Ponte H ENB (PWM motor direito) | 13 | `CAR_PIN_MOTOR_RIGHT_PWM` | Tirar o jumper de ENB do L298N |
| Ponte H IN3 / IN4 (direito) | 32 / 33 | `CAR_PIN_MOTOR_RIGHT_IN3/IN4` | Sentido |
| HC-SR04 TRIG | 4 | `CAR_PIN_HCSR04_TRIG` | |
| HC-SR04 ECHO | 18 | `CAR_PIN_HCSR04_ECHO` | **Eco é 5 V: usar divisor resistivo** (máx. 3,3 V no ESP32) |
| Tensão da bateria | 34 | `CAR_PIN_BATT_ADC` | ADC1, via divisor (`BATT_DIV_RATIO_X1000`) |
| DEBUG_PIN (analisador lógico) | 23 | `CAR_PIN_DEBUG` | Ver seção 8 |

### 3.2 Controle (ESP32 #1), fase 2

| Função | GPIO | Constante | Observação |
| --- | --- | --- | --- |
| Joystick X (VRx) | 34 | `CTRL_PIN_JOYSTICK_X` | ADC1, 3,3 V |
| Joystick Y (VRy) | 35 | `CTRL_PIN_JOYSTICK_Y` | ADC1 |
| LED de estado | 2 | `CTRL_PIN_LED` | LED da placa |

O botão (SW) do joystick não é usado. Só o ADC1 é usado, porque o ADC2 é compartilhado com o Wi-Fi.

### 3.3 Alimentação

- Ponte H e motores direto na bateria.
- Os dois ESP32 e o HC-SR04 num trilho de 5 V por step-down.
- **GND comum** e capacitores de desacoplamento.
- Recomendado: resistores de 10 kΩ para GND em IN1 a IN4, ENA e ENB, para os motores não se mexerem durante o boot.
- Ponte TB6612: mesmo mapa (PWMA/AIN1/AIN2, PWMB/BIN1/BIN2), com STBY em nível alto.

Para inverter sentido ou eixo sem mexer nos fios, use `INVERT_LEFT` e `INVERT_RIGHT` (carrinho) ou `INVERT_X` e `INVERT_Y` (controle) no `config.h`.

---

## 4. Como compilar e gravar o carrinho

No *ESP-IDF PowerShell*:

```powershell
cd C:\Unisinos\embarcados\trabalhoGB\carrinho

# Modo normal (com ESP-NOW): pasta build\
idf.py set-target esp32      # só na primeira vez
idf.py build
idf.py -p COMx flash monitor # troque COMx pela porta da placa

# Modo de bancada (sem rádio): pasta e sdkconfig próprios, sem precisar apagar nada
idf.py -B build_bench -D SDKCONFIG=build_bench/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.bench" build
idf.py -B build_bench -D SDKCONFIG=build_bench/sdkconfig -p COMx flash monitor
```

- Placa nova: use `idf.py -p COMx flash -a` para forçar a gravação completa.
- Para sair do monitor: `Ctrl+]`.
- Alternativa da seção 15.2: `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.bench" build` na pasta padrão. Nesse caso, **apague o `sdkconfig`** ao trocar entre os modos.

`build/`, `build_*/` e `sdkconfig` estão no `.gitignore`. Só os `sdkconfig.defaults*` são versionados.

---

## 5. MACs das placas

Os valores de `CONTROLLER_MAC` e `CAR_MAC` em `components/logic/include/config.h` são **placeholders**. O carrinho só aceita pacotes cujo MAC de origem seja `CONTROLLER_MAC`.

1. Grave o **carrinho** (modo normal) e abra o monitor. No boot aparece:
   `MAC do Carrinho: xx:xx:xx:xx:xx:xx`
2. Grave o **controle** e abra o monitor. Aparece:
   `MAC do Controle: xx:xx:xx:xx:xx:xx`
3. Preencha o `config.h` (os bytes em hexadecimal, na mesma ordem):
   ```c
   #define CONTROLLER_MAC { 0x.., 0x.., 0x.., 0x.., 0x.., 0x.. }
   #define CAR_MAC        { 0x.., 0x.., 0x.., 0x.., 0x.., 0x.. }
   ```
4. Recompile e grave **as duas** placas de novo.

Os dois firmwares usam o canal Wi-Fi `CHANNEL` (1) do `config.h`.

---

## 6. Modo de bancada (`CONFIG_CAR_BENCH_MODE`, seção 15.4)

Serve para testar o carrinho sem o controle.

> ⚠️ **Na primeira vez, rode a sequência com o carrinho levantado (rodas fora do chão).**

- **Rádio:** o Wi-Fi e o ESP-NOW não são iniciados, e o `CONTROLLER_MAC` não é exigido. No binário da bancada não há nenhum símbolo `esp_wifi_*`, `esp_now_*` ou `esp_netif_*`, conferido no `build_bench/car.map`. O componente `esp_wifi` ainda é compilado como biblioteca, porque o `PRIV_REQUIRES` não pode depender do Kconfig, mas não é linkado.
- **Mesmo caminho do uso real:** uma tarefa injeta `cmd_packet_t` válidos (20 Hz, `seq` crescente, CRC correto) na **mesma fila** do callback ESP-NOW. Eles passam pela mesma validação (`link_check_cmd`), pelo mesmo timeout de enlace, pela máquina de estados, pela mistura, pela rampa e pelos motores.
- **Início em FALHA_COM:** a sequência começa 3 s depois do boot. Por isso o log mostra `EVT link_lost` e `PARADO->FALHA_COM` logo no início. É o comportamento esperado de "boot sem controle". O passo 1 (zero) faz o carrinho voltar a PARADO.
- **Sequência:** são 9 passos, em laço:

| Passo | Duração | x | y | Esperado |
| --- | --- | --- | --- | --- |
| 1 | 2 s | 0 | 0 | Parado (arma) |
| 2 | 3 s | 0 | +40 | Os dois motores para a frente |
| 3 | 2 s | 0 | 0 | Parada imediata |
| 4 | 3 s | 0 | −40 | Os dois para trás |
| 5 | 2 s | 0 | 0 | Parada imediata |
| 6 | 3 s | +40 | +60 | Esquerdo mais rápido que o direito |
| 7 | 2 s | 0 | 0 | Parada imediata |
| 8 | 3 s | +60 | 0 | Esquerdo para frente, direito para trás |
| 9 | 2 s | 0 | 0 | Parada imediata |

- **Obstáculo:** um anteparo a 25 cm ou menos bloqueia os passos 2 e 6. A ré (passo 4) e o giro (passo 8) continuam liberados.
- **Telemetria:** é só logada (`TELEM: estado=... dist=...cm bat=...mV rej=...`).

---

## 7. Ajuste de `PWM_MIN_PCT`

Um comando de 1 a 100% vira PWM de `PWM_MIN_PCT` a `MAX_PWM_PCT`; 0 continua 0. O padrão é 30 **[CONFIRMAR]**. Para ajustar:

1. Use o modo de bancada com as rodas fora do chão.
2. Baixe `PWM_MIN_PCT` no `config.h` (por exemplo, 20) e recompile.
3. No passo 2 (y = +40), as rodas devem girar logo no começo da rampa, sem zumbido parado.
4. Ache o menor valor em que **as duas** rodas começam a girar e some uns 5 pontos de margem.
5. Repita com o carrinho no chão, porque o atrito é maior.

Se as rodas zumbirem sem girar, o valor está baixo demais.

Para limitar a velocidade máxima (útil no T2), use `MAX_PWM_PCT`: 100% do joystick vira exatamente `MAX_PWM_PCT`.

---

## 8. Registro para medição (seção 8.6)

O carrinho loga eventos com timestamp em ms, por uma fila. O log é impresso fora do laço de controle.

| Evento | Quando |
| --- | --- |
| `EVT link_lost` | O enlace caiu (300 ms sem pacote válido) |
| `EVT center_detected` | O comando passou de "fora do centro" para "centro" (só na transição) |
| `EVT obstacle_enter dist=<cm>` | Entrada em OBSTACULO |
| `EVT motors_zero` | Os motores chegaram a zero (só na borda) |
| `EVT state <de>-><para>` | Mudança de estado |

- **RNF04 (T6):** intervalo de `center_detected` até `motors_zero`.
- **RNF01 (T1):** intervalo de `link_lost` até `motors_zero`.
- **DEBUG_PIN (GPIO 23):** muda de nível **uma vez**, na mesma borda do `EVT motors_zero`. Fica em 0 com os motores girando e vai a 1 quando eles chegam a zero (`DEBUG_PIN_LEVEL_MOVING` e `DEBUG_PIN_LEVEL_STOPPED`). Com um analisador lógico, meça desse pino até a borda do sinal de EN da ponte H.

---

## 9. Testes unitários (seção 9.1)

São 19 testes em `test_apps/logic_host/main/test_main.c`:
- normalização e zona morta;
- mistura;
- rampa;
- CRC-8;
- sequência e validação de pacote;
- todas as transições da tabela 8.3;
- controle reiniciado depois de perder o enlace (T1);
- boot sem controle e Wi-Fi lento;
- histerese 25/30 cm;
- sensor inválido e recuperação.

### 9.1 Rodando no PC (foi o que rodou de fato)

O alvo linux do IDF **não funciona neste Windows**. O comando `idf.py --preview set-target linux` falha no CMake com `ld.lld: error: unable to find library -ladvapi32`: o alvo linux é experimental e feito para hosts Linux ou macOS.

Os mesmos testes foram compilados de forma **nativa** com o `gcc` do MinGW e o Unity que vem no IDF. O `test_main.c` tem `main()` próprio fora do ESP32:

```bash
# Git Bash, na raiz do trabalho
U=$IDF_PATH/components/unity/unity/src
gcc -std=c11 -Wall -Wextra -Werror -I$U -Icomponents/logic/include -Icomponents/protocol/include \
    $U/unity.c test_apps/logic_host/main/test_main.c components/logic/*.c components/protocol/crc8.c \
    -o test_apps/logic_host/host_build/test_logic.exe
./test_apps/logic_host/host_build/test_logic.exe
```

Resultado em 15/11/2026: **`19 Tests 0 Failures 0 Ignored OK`**, com gcc 16.1.0 (MinGW-W64).

Também conferi que os testes pegam o bug do item 1: com o `link.c` antigo, `test_link_controller_restart` e `test_sm_controller_restart` falham.

### 9.2 No ESP32 (plano B)

```powershell
cd C:\Unisinos\embarcados\trabalhoGB\test_apps\logic_host
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

O build para `esp32` **compila sem warnings**, mas **não foi gravado nem executado** numa placa.

---

## 10. Aceitação em bancada (seção 9.2): procedimento

Nenhum destes testes foi executado ainda: dependem do hardware montado. Os testes T1 e T4 precisam do controle (fase 2). Em todos, deixe o monitor serial aberto no carrinho (`idf.py -p COMx monitor`) e anote os timestamps dos eventos.

| Teste | Requisito | Procedimento | Critério |
| --- | --- | --- | --- |
| **T1** | RF05, RF07, RNF01 | Carrinho andando para a frente. Desligue o controle (tire a alimentação). Meça de `EVT link_lost` até `EVT motors_zero` (ou o DEBUG_PIN). Religue o controle **com o joystick à frente**: o carrinho deve continuar parado (FALHA_COM). Solte o joystick no centro: vai a PARADO; daí em diante volta a andar. Repita 10 vezes. | Parada ≤ 500 ms do último pacote; só retoma depois do centro. |
| **T2** | RF04, RNF02 | Velocidade máxima em direção a um anteparo. Meça a distância final com régua. Repita 10 vezes. Se falhar, ajuste `MAX_PWM_PCT` ou `OBSTACLE_ENTER_CM`. | ≥ 8 cm em 10 de 10. |
| **T3** | RNF03 | 20 partidas simultâneas dos dois motores, do zero à frente máxima. Pode usar o modo de bancada. Procure `rst:` ou `Guru Meditation` no monitor. | Nenhuma reinicialização. |
| **T4** | RF06 | Grave o controle com `FW_TEST_T4` (fase 2). 10 s depois do boot ele envia a sequência inválida. Compare o `rej=` da telemetria antes e depois. | Todos os pacotes rejeitados e contados em `rejected_cnt`; o carrinho não se mexe por causa deles. |
| **T5** | RF04 (sensor inválido) | Carrinho avançando. Desconecte o ECHO (ou o sensor). Observe `EVT state OPERANDO->OBSTACULO`. Reconecte, solte o joystick no centro e confira a volta a PARADO. | Avanço bloqueado em ≤ 500 ms (3 leituras × 60 ms + latência); recupera ao reconectar com o joystick no centro. |
| **T6** | RF03, RNF04 | Em movimento, solte o joystick. Meça de `EVT center_detected` até `EVT motors_zero`. Repita 10 vezes. | ≤ 300 ms em 10 de 10. |

> Sem eco conta como **sensor inválido** e bloqueia o avanço. Em espaço aberto, sem nada a menos de uns 4 m, o carrinho pode ficar bloqueado. Faça os ensaios com um anteparo ao alcance.

---

## 11. Checklist (só o que foi realmente feito)

- [x] `idf.py build` do `carrinho` em modo normal, **sem warnings** de compilação (15/11/2026).
- [x] `idf.py build` do `carrinho` em modo de bancada, **sem warnings** de compilação (15/11/2026). O CMake mostra 5 avisos de dependência privada entre `esp_wifi` e `wpa_supplicant`, que são de dentro do próprio IDF, não do nosso código.
- [x] Testes da lógica compilados nativamente (gcc) e **executados: 19/19 passaram**.
- [x] `test_apps/logic_host` compila para `esp32` sem warnings.
- [ ] `test_apps/logic_host` no alvo linux (não funciona no Windows, ver 9.1).
- [ ] `test_apps/logic_host` executado num ESP32.
- [ ] Gravação e teste do carrinho no hardware (T1 a T6).
- [ ] Firmware do controle (fase 2).

---

## 12. APIs validadas (RNF05)

**Fonte:** o MCP `espressif-documentation` (`search_espressif_sources`) **não estava disponível** na sessão do agente. A validação foi feita nos **headers e fontes do ESP-IDF v6.1 instalado** (`C:\esp\v6.1\esp-idf`, `version.txt` = v6.1), que são a origem da documentação oficial. A ordem de inicialização foi conferida no exemplo oficial `examples/wifi/espnow`.

**Data:** 15/11/2026.

| API | Assinatura / fato confirmado | Fonte (em `components/`) |
| --- | --- | --- |
| `esp_now_init` | `esp_err_t esp_now_init(void)` | `esp_wifi/include/esp_now.h` |
| `esp_now_register_recv_cb` | `esp_err_t (esp_now_recv_cb_t cb)`; `void (*)(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len)` | `esp_wifi/include/esp_now.h` |
| `esp_now_register_send_cb` | `esp_err_t (esp_now_send_cb_t cb)`; `void (*)(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)` | `esp_wifi/include/esp_now.h` |
| `esp_now_send_info_t` | `typedef wifi_tx_info_t esp_now_send_info_t`; campo `wifi_tx_status_t tx_status` (`WIFI_SEND_SUCCESS`) | `esp_now.h`, `esp_wifi/include/esp_wifi_types_generic.h` |
| `esp_now_send` | `esp_err_t (const uint8_t *peer_addr, const uint8_t *data, size_t len)` | `esp_wifi/include/esp_now.h` |
| `esp_now_add_peer` | `esp_err_t (const esp_now_peer_info_t *peer)`; `ESP_NOW_ETH_ALEN` | `esp_wifi/include/esp_now.h` |
| Componente de `esp_now.h` | **`esp_wifi`** | `esp_wifi/include/esp_now.h` |
| `esp_wifi_init` | `esp_err_t (const wifi_init_config_t *config)` | `esp_wifi/include/esp_wifi.h` |
| `esp_wifi_set_storage` | `esp_err_t (wifi_storage_t storage)` | `esp_wifi/include/esp_wifi.h` |
| `esp_wifi_set_channel` | `esp_err_t (uint8_t primary, wifi_second_chan_t second)` | `esp_wifi/include/esp_wifi.h` |
| `esp_wifi_get_mac` | `esp_err_t (wifi_interface_t ifx, uint8_t mac[6])` | `esp_wifi/include/esp_wifi.h` |
| Ordem de inicialização | `nvs_flash_init` → `esp_netif_init` → `esp_event_loop_create_default` → `esp_wifi_init` → `esp_wifi_set_storage(RAM)` → `esp_wifi_set_mode` → `esp_wifi_start` → `esp_wifi_set_channel` → `esp_now_init` | `examples/wifi/espnow/main/espnow_example_main.c` |
| `MACSTR`, `MAC2STR` | Macros de formatação | `esp_hw_support/include/esp_mac.h` |
| `ledc_timer_config_t` | Campos `speed_mode`, `duty_resolution`, `timer_num`, `freq_hz`, `clk_cfg`, `deconfigure` | `esp_driver_ledc/include/driver/ledc.h` |
| `ledc_channel_config_t` | Campos `gpio_num`, `speed_mode`, `channel`, `timer_sel`, `duty`, `hpoint`, `sleep_mode`, `flags.output_invert`, `deconfigure`. **`intr_type` está `__attribute__((deprecated))`** e foi removido do código. | `esp_driver_ledc/include/driver/ledc.h` |
| `LEDC_AUTO_CLK` | **Existe** para o esp32 (`soc_periph_ledc_clk_src_legacy_t`, valor 0) | `soc/esp32/include/soc/clk_tree_defs.h` |
| `LEDC_SLEEP_MODE_NO_ALIVE_NO_PD` | Existe (modo padrão) | `esp_driver_ledc/include/driver/ledc.h` |
| `ledc_set_duty`, `ledc_update_duty` | `esp_err_t (ledc_mode_t, ledc_channel_t, uint32_t duty)`; `esp_err_t (ledc_mode_t, ledc_channel_t)`. Não são thread-safe: só o laço de controle chama. | `esp_driver_ledc/include/driver/ledc.h` |
| `gpio_set_level`, `gpio_get_level` | `esp_err_t (gpio_num_t, uint32_t)`; `int (gpio_num_t)` | `esp_driver_gpio/include/driver/gpio.h` |
| `gpio_install_isr_service`, `gpio_isr_handler_add` | `esp_err_t (int intr_alloc_flags)`; `esp_err_t (gpio_num_t, gpio_isr_t, void *)` | `esp_driver_gpio/include/driver/gpio.h` |
| `adc_oneshot_new_unit`, `adc_oneshot_config_channel`, `adc_oneshot_read`, `adc_oneshot_io_to_channel` | `esp_err_t (const adc_oneshot_unit_init_cfg_t *, adc_oneshot_unit_handle_t *)`; `esp_err_t (handle, adc_channel_t, const adc_oneshot_chan_cfg_t *)`; `esp_err_t (handle, adc_channel_t, int *out_raw)`; `esp_err_t (int io_num, adc_unit_t *const, adc_channel_t *const)` | `esp_adc/include/esp_adc/adc_oneshot.h` |
| `adc_oneshot_read` não bloqueia | Usa `adc_lock_try_acquire` e retorna `ESP_ERR_TIMEOUT` se o ADC estiver ocupado; por isso pode ficar no laço de controle | `esp_adc/adc_oneshot.c` |
| `adc_cali_create_scheme_line_fitting` | `esp_err_t (const adc_cali_line_fitting_config_t *, adc_cali_handle_t *)`; `ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED` = 1 no esp32 | `esp_adc/include/esp_adc/adc_cali_scheme.h`, `esp_adc/esp32/include/adc_cali_schemes.h` |
| `adc_cali_raw_to_voltage` | `esp_err_t (adc_cali_handle_t, int raw, int *voltage)` | `esp_adc/include/esp_adc/adc_cali.h` |
| `esp_timer_create`, `esp_timer_start_periodic`, `esp_timer_get_time` | `esp_err_t (const esp_timer_create_args_t *, esp_timer_handle_t *)`; `esp_err_t (handle, uint64_t period)`; `int64_t (void)`; campo `skip_unhandled_events` | `esp_timer/include/esp_timer.h` |
| `xTaskDelayUntil` | Existe (substitui `vTaskDelayUntil`, removido no v6) | `freertos/FreeRTOS-Kernel/include/freertos/task.h` |
| Alvo linux | `idf.py --preview set-target linux` falha no Windows (`-ladvapi32`) | Teste local, 15/11/2026 |

---

## 13. Suposições e decisões

### 13.1 Valores [CONFIRMAR] usados (padrões das seções 5 e 10)

| Item | Valor usado | Onde |
| --- | --- | --- |
| Pinos do carrinho | Tabela 3.1 | `config.h` |
| Pinos do controle | X = 34, Y = 35, LED = 2 | `config.h` |
| `DEBUG_PIN` | GPIO 23 | `CAR_PIN_DEBUG` |
| `PWM_MIN_PCT` | 30 | `config.h` (ajustar, seção 7) |
| `MAX_PWM_PCT` | 100 | `config.h` (ajustar no T2) |
| `BATT_DIV_RATIO_X1000` | 3000 (divisor 3:1) | `config.h` (medir o divisor real) |
| `OBSTACLE_ENTER_CM` / `OBSTACLE_EXIT_CM` | 25 / 30 | `config.h` |
| `CONTROLLER_MAC` / `CAR_MAC` | Placeholders | `config.h` (seção 5) |
| `INVERT_*` | Todos `false` | `config.h` |
| Canal Wi-Fi | 1 | `CHANNEL` |
| Sem eco do HC-SR04 | Conta como **sensor inválido** (bloqueia o avanço), como na proposta | `state_machine.c` |
| Segundo motor "extra" | Não usado | — |
| IDs RF/RNF | Seguem a proposta v2 de 08/11 | Seção 2 da especificação |

### 13.2 Decisões tomadas na revisão

- **Limite de 300 ms:** a especificação se contradiz. A seção 8.3 diz `link_ok` = `now − last_rx ≤ 300 ms`, mas o teste obrigatório da 9.1 pede "FALHA_COM aos 300 ms e **não** aos 299 ms". Foi seguida a 9.1, que é o lado mais seguro: o enlace cai ao **completar** 300 ms sem pacote válido. Para voltar à leitura da 8.3, troque `<` por `<=` em `link_is_alive()` e ajuste os testes.
- **Controle reiniciado:** quando o enlace cai, o próximo pacote íntegro (origem, tamanho, magic, tipo e CRC corretos) é aceito como "primeiro pacote", qualquer que seja o `seq`. Sair de FALHA_COM continua exigindo o joystick no centro (RF07).
- **Boot sem controle:** `boot_time_ms` é gravado antes de o Wi-Fi subir. Se nenhum pacote chegar em 300 ms, contando o tempo de subida do Wi-Fi, o carrinho entra em FALHA_COM. É o comportamento da seção 8.3 ("boot sem controle vai a FALHA_COM") e tem teste (`test_sm_slow_wifi_boot`).
- **Telemetria:** não é enviada antes do callback do envio anterior (seção 6). Se o callback não tiver chegado, aquele período é pulado. `rejected_cnt` = rejeições do enlace + rejeições por estado + pacotes perdidos por fila cheia, este último com contador próprio, escrito só pelo callback.
- **Leitura da bateria:** fica no laço de controle (a cada 200 ms, 8 amostras), porque `adc_oneshot_read` não bloqueia (seção 12).
- **Sensor:** período fixo de 60 ms com `xTaskDelayUntil`. Antes eram 60 ms mais o tempo de espera do eco.
- **Tabela da bancada:** os valores de `x` e `y` dos 9 passos ficam na tabela de `bench.c`, porque são os dados do próprio ensaio da seção 15.4. As durações estão em `config.h`.
