# Carrinho controlado por joystick (ESP32, ESP-IDF v6.1)

Firmware dos dois ESP32 do trabalho de Sistemas Embarcados (Grau B): **controle** (1 joystick) e **carrinho** (ponte H, 2 motores DC, HC-SR04, leitura de bateria). Comunicação por ESP-NOW. A referência técnica é o [ESPECIFICACAO_CODIGO.md](ESPECIFICACAO_CODIGO.md).

**Situação atual:** o **carrinho** e o **controle** estão implementados e compilam, mas **nenhum dos dois foi testado no hardware**. Ver o checklist na seção 12.

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
├── controle/                  # projeto IDF do controle (nome do projeto: controller)
│   ├── sdkconfig.defaults
│   ├── sdkconfig.defaults.t4  # build do teste T4
│   └── main/  main.c (boot + timer de envio), joystick_adc.c, radio.c, ui.c, t4.c, Kconfig.projbuild
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
| Buzzer (buzina) | 19 | `CAR_PIN_BUZZER` | Tom de 2 kHz por PWM (`CAR_BUZZER_FREQ_HZ`). Ver ligação abaixo |
| LEDs de movimento | 21 | `CAR_PIN_MOVE_LED` | Acesos enquanto algum motor gira (frente, ré ou giro). Ver ligação abaixo |

**Buzina:** enquanto o botão SW do joystick está apertado, o controle manda o bit `PROTO_BTN_HORN` no `cmd_packet_t` e o carrinho toca o buzzer. Toca em qualquer estado e desliga sozinha se o enlace cair. O log do carrinho mostra `EVT horn_on` / `EVT horn_off`.

Ligação do buzzer passivo:
- **Piezo** (disco ou cápsula fina, sem polaridade): GPIO 19 → resistor de 100 Ω → buzzer → GND. Pode ser ligado em qualquer sentido.
- **Magnético** (cápsula preta com bobina, uns 16 a 42 Ω): **não ligue direto no GPIO**, a corrente passa do limite do pino. Use um transistor NPN (BC547, 2N2222): GPIO 19 → resistor de 1 kΩ → base; emissor no GND; coletor → buzzer → 3V3 ou 5 V. Coloque um diodo (1N4148) em paralelo com o buzzer, catodo no lado do +. Se o buzzer não tiver marcação, qualquer sentido funciona.
- **Módulo de 3 pinos** (VCC, GND, I/O): VCC em 3V3, GND no GND e I/O no GPIO 19. O módulo já tem o transistor.

**LEDs de movimento:** acendem quando a saída de algum motor é diferente de zero (o mesmo momento em que o `DEBUG_PIN` vai para "girando") e apagam no `EVT motors_zero`. Ligação de cada LED: GPIO 21 → resistor de 220 Ω → anodo (perna longa) → catodo (perna curta) → GND. Vários LEDs vão em paralelo, cada um com seu resistor. O GPIO aguenta cerca de **2 LEDs**; para mais, use um transistor NPN (GPIO 21 → 1 kΩ → base, emissor no GND, coletor nos catodos, e os anodos com resistor no 3V3 ou 5 V).

### 3.2 Controle (ESP32 #1)

| Pino do joystick | Liga em | Constante | Observação |
| --- | --- | --- | --- |
| VRx (eixo X) | GPIO 34 | `CTRL_PIN_JOYSTICK_X` | ADC1. GPIO 34 e 35 são só entrada, sem pull interno |
| VRy (eixo Y) | GPIO 35 | `CTRL_PIN_JOYSTICK_Y` | ADC1 |
| +5V / VCC | **3V3** do ESP32 | | **Não ligue em 5 V**: a saída do potenciômetro iria a 5 V no pino do ADC |
| GND | GND | | |
| SW (botão) | GPIO 32 | `CTRL_PIN_JOY_BUTTON` | Buzina. Pull-up interno; o botão fecha para GND |
| LED de estado | GPIO 2 | `CTRL_PIN_LED` | LED da própria placa |

Só o ADC1 é usado, porque o ADC2 é compartilhado com o Wi-Fi. O firmware confere no boot que os dois pinos estão no ADC1 e para com erro se não estiverem.

### 3.3 Alimentação

- Ponte H e motores direto na bateria.
- Os dois ESP32 e o HC-SR04 num trilho de 5 V por step-down.
- **GND comum** e capacitores de desacoplamento.
- Recomendado: resistores de 10 kΩ para GND em IN1 a IN4, ENA e ENB, para os motores não se mexerem durante o boot.
- Ponte TB6612: mesmo mapa (PWMA/AIN1/AIN2, PWMB/BIN1/BIN2), com STBY em nível alto.

Para inverter sentido ou eixo sem mexer nos fios, use `INVERT_LEFT` e `INVERT_RIGHT` (carrinho) ou `INVERT_X` e `INVERT_Y` (controle) no `config.h`.

---

## 4. Como compilar e gravar

### 4.1 Carrinho

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

### 4.2 Controle

```powershell
cd C:\Unisinos\embarcados\trabalhoGB\controle

# Modo normal: pasta build\
idf.py set-target esp32      # só na primeira vez
idf.py build
idf.py -p COMy flash monitor

# Build do teste T4: pasta e sdkconfig próprios
idf.py -B build_t4 -D SDKCONFIG=build_t4/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.t4" build
idf.py -B build_t4 -D SDKCONFIG=build_t4/sdkconfig -p COMy flash monitor

# Build de diagnóstico do joystick: pasta e sdkconfig próprios
idf.py -B build_joy -D SDKCONFIG=build_joy/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.joyled" build
idf.py -B build_joy -D SDKCONFIG=build_joy/sdkconfig -p COMy flash monitor
```

- **Build de diagnóstico do joystick** (`CONFIG_CTRL_LED_JOYSTICK=y`): o LED (GPIO 2) acende quando o x ou o y enviados são diferentes de 0, ou seja, com o joystick fora da zona morta, e o log mostra `joystick: x=... y=...` a cada 500 ms. Nesse build o LED **não** mostra o estado do carrinho. Com o joystick solto, o LED tem que ficar apagado e o log tem que mostrar `x=0 y=0`.

### 4.3 Gravando as duas placas

Cada placa aparece numa porta COM diferente. Veja no Gerenciador de Dispositivos, ou ligue uma de cada vez.

1. Ligue só o **carrinho** no USB e anote a porta (`COMx`). Grave pela seção 4.1.
2. Ligue só o **controle** e anote a porta (`COMy`). Grave pela seção 4.2.
3. Para ver as duas ao mesmo tempo, abra dois terminais do ESP-IDF, um com `idf.py -p COMx monitor` na pasta `carrinho` e outro com `idf.py -p COMy monitor` na pasta `controle`.

Na primeira vez, sem o `config_macs.h`, os MACs ficam zerados (o build avisa com um `#warning`). Siga a seção 5 e grave as duas de novo.

`build/`, `build_*/` e `sdkconfig` estão no `.gitignore`. Só os `sdkconfig.defaults*` são versionados.

---

## 5. MACs das placas

Os MACs `CONTROLLER_MAC` e `CAR_MAC` ficam em `components/logic/include/config_macs.h`. Esse arquivo é local e está no `.gitignore`, então os MACs reais não vão para o repositório. O `config.h` inclui esse arquivo se ele existir; se não existir, usa MACs zerados e o build mostra um aviso. O carrinho só aceita pacotes cujo MAC de origem seja `CONTROLLER_MAC`.

0. Crie o arquivo local a partir do modelo versionado:
   ```sh
   cp components/logic/include/config_macs.example.h components/logic/include/config_macs.h
   ```
1. Grave o **carrinho** (modo normal) e abra o monitor. No boot aparece:
   `car_main: MAC do Carrinho: xx:xx:xx:xx:xx:xx`
2. Grave o **controle** e abra o monitor. Aparece:
   `radio: MAC do Controle: xx:xx:xx:xx:xx:xx`
3. Preencha o `config_macs.h` (os bytes em hexadecimal, na mesma ordem):
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

- **Obstáculo:** um anteparo a 15 cm ou menos bloqueia os passos 2 e 6. A ré (passo 4) e o giro (passo 8) continuam liberados.
- **Telemetria:** é só logada (`TELEM: estado=... dist=...cm bat=...mV rej=...`).

---

## 7. Ajuste de `PWM_MIN_PCT`

Um comando de 1 a 100% vira PWM de `PWM_MIN_PCT` a `MAX_PWM_PCT`; 0 continua 0. O padrão é 60. Para ajustar:

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

## 9. Controle: calibração, envio, telemetria e LED (seção 7)

### 9.1 Calibração do joystick

No boot, logo depois de subir o rádio, o controle mede o centro dos dois eixos: 25 leituras a cada 20 ms, cerca de 0,5 s, cada leitura com média de 4 amostras.

- **Deixe o joystick solto até aparecer `Centro calibrado: X=... Y=...`.** Se ele estiver inclinado durante a calibração, essa posição vira o "centro".
- Centro fora de **1500 a 2600** gera o aviso `Centro do eixo ... fora do esperado`. O firmware continua, mas confira a ligação do joystick (VCC em 3V3, VRx e VRy nos pinos certos).
- Os extremos são 0 e 4095 (12 bits), com escalas separadas para cada lado do centro. Assim o centro físico vira 0 mesmo que não esteja no meio da faixa.
- Depois vem a zona morta, com reescala contínua: **10% no eixo Y** (`DEADZONE_PCT`) e **25% no eixo X** (`DEADZONE_X_PCT`). A zona maior no X deixa andar reto mesmo com o joystick um pouco torto. Se ainda virar sem querer, aumente `DEADZONE_X_PCT`; se demorar demais para começar a virar, diminua.
- Para inverter um eixo, use `INVERT_X` ou `INVERT_Y` no `config.h`. O eixo Y para a frente deve dar valor **positivo**, e o eixo X para a direita também.

### 9.2 Envio

- Um `esp_timer` de 50 ms (20 Hz) envia **sempre**, mesmo com x = y = 0: é o sinal de vida do enlace. A cada pacote, `seq` aumenta e o CRC-8 é recalculado.
- Se o callback do envio anterior ainda não chegou, o ciclo é **pulado e contado** (`pulados=`). Depois de 4 ciclos seguidos sem callback, a trava é liberada (`destravados=`), para o controle nunca parar de enviar.
- Se a leitura do ADC falhar, mesmo depois das repetições em `ESP_ERR_TIMEOUT`, o pacote sai com x = y = 0 (neutro) e a falha é contada (`adc_falhas=`).
- A cada 10 s o log mostra as estatísticas, por exemplo:
  `envio: enviados=200 ok=200 falhas=0 erros=0 pulados=0 destravados=0 | telem: invalidas=0 fila_cheia=0 | adc_falhas=0`

### 9.3 Telemetria e LED

A tarefa de UI valida cada telemetria (origem `CAR_MAC`, tamanho, magic, tipo, CRC e estado) e loga:

```
estado=OPERANDO dist=42cm bat=7430mV rej=0
```

Com o sensor inválido, aparece `dist=invalido`. Sem telemetria por mais de 1 s, loga `SEM TELEMETRIA` uma vez por segundo.

| LED (GPIO 2) | Estado do carrinho |
| --- | --- |
| Aceso fixo | PARADO |
| Pisca a 1 Hz | OPERANDO |
| Pisca a 4 Hz | OBSTACULO |
| Duas piscadas curtas a cada 1 s | FALHA_COM |
| Apagado | Sem telemetria (carrinho desligado, fora de alcance ou MAC errado) |

### 9.4 Teste T4 (`CONFIG_FW_TEST_T4`)

O build de teste é ligado por `sdkconfig.defaults.t4` (seção 4.2). No build normal, o código do T4 não existe: o `controller.map` do build normal não tem nenhum símbolo `t4_*`.

10 s depois do boot, o controle envia **uma vez**:

| Passo | Pacote | Esperado no carrinho |
| --- | --- | --- |
| 1 | `seq` 10 abaixo do atual | Rejeitado (sequência antiga), `rej` +1 |
| 2 | `x = 120` | Rejeitado (fora de faixa), `rej` +1 |
| 3 | CRC errado | Rejeitado, `rej` +1 |
| 4 | 3 s de `y = +100` | **Válido**: o carrinho anda. A subida segue a rampa (0 a 100% em 0,5 s) |
| 5 | 1 s de `y = −100`, logo depois do passo 4 | **Válido**: a saída vai a zero, espera 50 ms (`REVERSE_DWELL_MS`) e só então sobe em ré |

Depois disso, o controle volta ao joystick. Cada passo aparece no log do controle como `T4: 1/5 ...` até `T4: concluido`.

Os passos 1 a 3 ocupam 3 ciclos de 50 ms sem pacote válido (150 ms). Isso é menos que 300 ms, então o enlace **não** cai durante o teste.

Os passos 4 e 5 são comandos legítimos e **não** contam em `rejected_cnt`. Eles verificam a proteção, não a rejeição: a rampa na subida e o intertravamento na inversão.

---

## 10. Testes unitários (seção 9.1)

São 22 testes em `test_apps/logic_host/main/test_main.c`:
- normalização e zona morta;
- mistura;
- rampa;
- CRC-8;
- sequência e validação de pacote;
- todas as transições da tabela 8.3;
- controle reiniciado depois de perder o enlace (T1);
- boot sem controle e Wi-Fi lento;
- histerese 15/20 cm;
- sensor inválido e recuperação;
- validação da telemetria no controle;
- bit da buzina no pacote de comando;
- padrão do LED do controle.

### 10.1 Rodando no PC (foi o que rodou de fato)

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

Resultado em 16/11/2026: **`21 Tests 0 Failures 0 Ignored OK`**, com gcc 16.1.0 (MinGW-W64). Na Parte A, em 15/11, eram 19 de 19.

Também conferi que os testes pegam o bug do item 1: com o `link.c` antigo, `test_link_controller_restart` e `test_sm_controller_restart` falham.

### 10.2 No ESP32 (plano B)

```powershell
cd C:\Unisinos\embarcados\trabalhoGB\test_apps\logic_host
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

O build para `esp32` **compila sem warnings**, mas **não foi gravado nem executado** numa placa.

---

## 11. Aceitação em bancada (seção 9.2): procedimento

Nenhum destes testes foi executado ainda: dependem do hardware montado. Em todos, deixe o monitor serial aberto no carrinho (`idf.py -p COMx monitor`) e anote os timestamps dos eventos.

| Teste | Requisito | Procedimento | Critério |
| --- | --- | --- | --- |
| **T1** | RF05, RF07, RNF01 | Carrinho andando para a frente. Desligue o controle (tire a alimentação). Meça de `EVT link_lost` até `EVT motors_zero` (ou o DEBUG_PIN). Religue o controle **com o joystick à frente**: o carrinho deve continuar parado (FALHA_COM). Solte o joystick no centro: vai a PARADO; daí em diante volta a andar. Repita 10 vezes. | Parada ≤ 500 ms do último pacote; só retoma depois do centro. |
| **T2** | RF04, RNF02 | Velocidade máxima em direção a um anteparo. Meça a distância final com régua. Repita 10 vezes. Se falhar, ajuste `MAX_PWM_PCT` ou `OBSTACLE_ENTER_CM`. | ≥ 8 cm em 10 de 10. |
| **T3** | RNF03 | 20 partidas simultâneas dos dois motores, do zero à frente máxima. Pode usar o modo de bancada. Procure `rst:` ou `Guru Meditation` no monitor. | Nenhuma reinicialização. |
| **T4** | RF06 | **Rodas fora do chão.** Grave o controle com o build T4 (seção 4.2) e deixe o joystick no centro. Anote o `rej=` da telemetria antes dos 10 s. Na sequência (seção 9.4): o `rej=` deve subir 3 (passos 1 a 3) e o carrinho não deve se mexer nesse trecho. Nos passos 4 e 5 o carrinho anda: confira no `DEBUG_PIN` ou no log que a subida é gradual e que a inversão passa por `EVT motors_zero` antes da ré. | Os 3 pacotes inválidos rejeitados e contados em `rejected_cnt`; o enlace não cai; frente máxima com rampa; inversão passa por zero. |
| **T5** | RF04 (sensor inválido) | Carrinho avançando. Desconecte o ECHO (ou o sensor). Observe `EVT state OPERANDO->OBSTACULO`. Reconecte, solte o joystick no centro e confira a volta a PARADO. | Avanço bloqueado em ≤ 500 ms (3 leituras × 60 ms + latência); recupera ao reconectar com o joystick no centro. |
| **T6** | RF03, RNF04 | Em movimento, solte o joystick. Meça de `EVT center_detected` até `EVT motors_zero`. Repita 10 vezes. | ≤ 300 ms em 10 de 10. |

> **Sem eco = caminho livre.** Se o ECHO sobe mas o eco não volta (nada a menos de uns 4 m), a leitura vale `dist=400cm` e o carrinho continua andando. Só bloqueia como **sensor inválido** quando o ECHO nem sobe (sensor desligado ou ECHO solto, como no T5) ou fica preso em nível alto por mais de 5 ciclos (300 ms).

---

## 12. Checklist (só o que foi realmente feito)

- [x] `idf.py build` do `carrinho` em modo normal, **sem warnings** de compilação (15/11/2026).
- [x] `idf.py build` do `carrinho` em modo de bancada, **sem warnings** de compilação (15/11/2026). O CMake mostra 5 avisos de dependência privada entre `esp_wifi` e `wpa_supplicant`, que são de dentro do próprio IDF, não do nosso código.
- [x] `idf.py build` do `controle` em modo normal e no build T4, **sem warnings** de compilação (16/11/2026). No build normal não há nenhum símbolo `t4_*` no `controller.map`.
- [x] `carrinho` recompilado nos dois modos depois das mudanças do controle (16/11/2026).
- [x] Testes da lógica compilados nativamente (gcc) e **executados: 21/21 passaram** (16/11/2026).
- [x] `test_apps/logic_host` compila para `esp32` sem warnings.
- [ ] `test_apps/logic_host` no alvo linux (não funciona no Windows, ver 10.1).
- [ ] `test_apps/logic_host` executado num ESP32.
- [ ] Gravação e teste do carrinho no hardware.
- [ ] Gravação e teste do controle no hardware (calibração, LED, telemetria).
- [ ] Testes de aceitação T1 a T6.
- [ ] MACs reais em `config_macs.h` (local, fora do git).

---

## 13. APIs validadas (RNF05)

**Fonte:** o MCP `espressif-documentation` (`search_espressif_sources`) **não estava disponível** na sessão do agente. A validação foi feita nos **headers e fontes do ESP-IDF v6.1 instalado** (`C:\esp\v6.1\esp-idf`, `version.txt` = v6.1), que são a origem da documentação oficial. A ordem de inicialização foi conferida no exemplo oficial `examples/wifi/espnow`.

**Data:** 15/11/2026 (carrinho) e 16/11/2026 (itens do controle).

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
| `adc_oneshot_unit_init_cfg_t` | Campos `unit_id`, `clk_src`, `ulp_mode` | `esp_adc/include/esp_adc/adc_oneshot.h` |
| `adc_oneshot_chan_cfg_t` | Campos `atten`, `bitwidth` | `esp_adc/include/esp_adc/adc_oneshot.h` |
| `ADC_ATTEN_DB_12`, `ADC_BITWIDTH_DEFAULT` | `ADC_ATTEN_DB_12 = 3`; `ADC_BITWIDTH_DEFAULT = 0` = maior largura suportada, 12 bits no ESP32 (`SOC_ADC_RTC_MAX_BITWIDTH = 12`) | `esp_hal_ana_conv/include/hal/adc_types.h`, `soc/esp32/include/soc/soc_caps.h` |
| `esp_timer_create_args_t` | Campos `callback`, `arg`, `dispatch_method` (`ESP_TIMER_TASK`), `name`, `skip_unhandled_events` | `esp_timer/include/esp_timer.h` |
| `gpio_config` | `esp_err_t gpio_config(const gpio_config_t *pGPIOConfig)` | `esp_driver_gpio/include/driver/gpio.h` |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240`, `CONFIG_FREERTOS_HZ` | Opções existem no v6.1 (`sdkconfig.defaults` dos dois projetos) | `esp_system/port/soc/esp32/Kconfig.cpu`, `freertos/Kconfig` |
| `Kconfig.projbuild`, `SDKCONFIG_DEFAULTS` com `;` | Funcionam: `CONFIG_CAR_BENCH_MODE=y` e `CONFIG_FW_TEST_T4=y` aparecem nos `sdkconfig` gerados | Build local, 16/11/2026 |
| Alvo linux | `idf.py --preview set-target linux` falha no Windows (`-ladvapi32`) | Teste local, 15/11/2026 |

---

## 14. Suposições e decisões

### 14.1 Valores [CONFIRMAR] usados (padrões das seções 5 e 10)

| Item | Valor usado | Onde |
| --- | --- | --- |
| Pinos do carrinho | Tabela 3.1 | `config.h` |
| Pinos do controle | X = 34, Y = 35, LED = 2 | `config.h` |
| Alimentação do joystick | 3V3 do ESP32 | Seção 3.2 |
| `DEBUG_PIN` | GPIO 23 | `CAR_PIN_DEBUG` |
| `PWM_MIN_PCT` | 60 | `config.h` (ajustar, seção 7) |
| `MAX_PWM_PCT` | 100 | `config.h` (ajustar no T2) |
| `BATT_DIV_RATIO_X1000` | 3000 (divisor 3:1) | `config.h` (medir o divisor real) |
| `OBSTACLE_ENTER_CM` / `OBSTACLE_EXIT_CM` | 15 / 20 | `config.h` |
| `CONTROLLER_MAC` / `CAR_MAC` | MACs das placas usadas | `config_macs.h`, local (seção 5) |
| `INVERT_*` | Todos `false` | `config.h` |
| Canal Wi-Fi | 1 | `CHANNEL` |
| Sem eco do HC-SR04 | ECHO sobe e o eco não volta: **livre** (`dist=400cm`). ECHO não sobe ou fica preso: **sensor inválido** (bloqueia o avanço) | `sonar.c` |
| Segundo motor "extra" | Não usado | — |
| IDs RF/RNF | Seguem a proposta v2 de 08/11 | Seção 2 da especificação |

### 14.2 Decisões tomadas na revisão (Parte A)

- **Limite de 300 ms:** a especificação se contradiz. A seção 8.3 diz `link_ok` = `now − last_rx ≤ 300 ms`, mas o teste obrigatório da 9.1 pede "FALHA_COM aos 300 ms e **não** aos 299 ms". Foi seguida a 9.1, que é o lado mais seguro: o enlace cai ao **completar** 300 ms sem pacote válido. Para voltar à leitura da 8.3, troque `<` por `<=` em `link_is_alive()` e ajuste os testes.
- **Controle reiniciado:** quando o enlace cai, o próximo pacote íntegro (origem, tamanho, magic, tipo e CRC corretos) é aceito como "primeiro pacote", qualquer que seja o `seq`. Sair de FALHA_COM continua exigindo o joystick no centro (RF07).
- **Boot sem controle:** `boot_time_ms` é gravado antes de o Wi-Fi subir. Se nenhum pacote chegar em 300 ms, contando o tempo de subida do Wi-Fi, o carrinho entra em FALHA_COM. É o comportamento da seção 8.3 ("boot sem controle vai a FALHA_COM") e tem teste (`test_sm_slow_wifi_boot`).
- **Telemetria:** não é enviada antes do callback do envio anterior (seção 6). Se o callback não tiver chegado, aquele período é pulado. `rejected_cnt` = rejeições do enlace + rejeições por estado + pacotes perdidos por fila cheia, este último com contador próprio, escrito só pelo callback.
- **Leitura da bateria:** fica no laço de controle (a cada 200 ms, 8 amostras), porque `adc_oneshot_read` não bloqueia (seção 13).
- **Sensor:** período fixo de 60 ms com `xTaskDelayUntil`. Antes eram 60 ms mais o tempo de espera do eco.
- **Tabela da bancada:** os valores de `x` e `y` dos 9 passos ficam na tabela de `bench.c`, porque são os dados do próprio ensaio da seção 15.4. As durações estão em `config.h`.

### 14.3 Decisões do controle (Parte B)

- **T4, passos 4 e 5:** frente máxima e inversão direta são pacotes **válidos** e não são rejeitados. O T4 verifica a rejeição dos passos 1 a 3 e o comportamento seguro nos passos 4 e 5 (rampa e passagem por zero). O carrinho não foi alterado para "rejeitar" a inversão (opção A, aprovada).
- **T4, durações:** 3 s de frente máxima (da especificação) e 1 s de ré depois da inversão (escolhido aqui; `T4_REV_MS`). A "sequência antiga" usa `seq` 10 abaixo do atual (`T4_OLD_SEQ_BACK`).
- **LED em INIT:** a especificação não define o padrão. Fica aceso fixo, como PARADO. Na prática o carrinho só manda telemetria depois de sair de INIT.
- **Falha do ADC em operação:** o pacote sai com x = y = 0 (neutro), e o carrinho para. A falha é contada em `adc_falhas=`.
- **Sem callback de envio:** depois de 4 ciclos seguidos pulados (200 ms), a trava é liberada para o controle voltar a enviar. Sem isso, um callback perdido deixaria o carrinho em FALHA_COM para sempre.
- **Calibração sem leitura:** se nenhuma leitura der certo, o centro fica em 2048 e o erro aparece no log. Se o centro calibrado cair em 0 ou 4095 (joystick desligado), `joystick_normalize()` devolve 0 e o controle manda neutro.
- **Commits:** o firmware do controle foi para um único commit, porque `main.c`, `radio.c`, `ui.c` e `t4.c` dependem uns dos outros para compilar.
