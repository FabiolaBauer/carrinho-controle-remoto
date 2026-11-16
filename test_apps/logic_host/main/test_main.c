/**
 * @file test_main.c
 * @brief Suíte completa de testes unitários para a lógica do carrinho (Seção 9.1)
 *
 * Testa componentes puros: protocol (CRC-8, pacotes) e logic (joystick, mixer, rampa, link, máquina de estados).
 * Roda tanto nativamente no host (via gcc/Unity) quanto no ESP32 via ESP-IDF Unity.
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"

#include "protocol.h"
#include "crc8.h"
#include "config.h"
#include "joystick.h"
#include "mixer.h"
#include "ramp.h"
#include "link.h"
#include "state_machine.h"
#include "telem.h"
#include "led_pattern.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* =========================================================================
 * 1. Testes de Joystick (Normalização e Zona Morta) - Seção 9.1
 * ========================================================================= */
void test_joystick_normalize(void)
{
    // Raw no centro (2048 em 0..4095) deve normalizar para 0
    TEST_ASSERT_EQUAL_INT8(0, joystick_normalize(2048, 2048, 0, 4095, false));

    // Raw no máximo (4095) deve normalizar para 100
    TEST_ASSERT_EQUAL_INT8(100, joystick_normalize(4095, 2048, 0, 4095, false));

    // Raw no mínimo (0) deve normalizar para -100
    TEST_ASSERT_EQUAL_INT8(-100, joystick_normalize(0, 2048, 0, 4095, false));

    // Inversão ativa deve trocar o sinal
    TEST_ASSERT_EQUAL_INT8(-100, joystick_normalize(4095, 2048, 0, 4095, true));
    TEST_ASSERT_EQUAL_INT8(100, joystick_normalize(0, 2048, 0, 4095, true));
}

void test_joystick_deadzone(void)
{
    // Centro -> 0
    TEST_ASSERT_EQUAL_INT8(0, joystick_deadzone(0, 10));

    // +-9% -> 0
    TEST_ASSERT_EQUAL_INT8(0, joystick_deadzone(9, 10));
    TEST_ASSERT_EQUAL_INT8(0, joystick_deadzone(-9, 10));

    // +-10% -> 0
    TEST_ASSERT_EQUAL_INT8(0, joystick_deadzone(10, 10));
    TEST_ASSERT_EQUAL_INT8(0, joystick_deadzone(-10, 10));

    // +-100% -> +-100
    TEST_ASSERT_EQUAL_INT8(100, joystick_deadzone(100, 10));
    TEST_ASSERT_EQUAL_INT8(-100, joystick_deadzone(-100, 10));

    // Reescala contínua na borda: (|v| - 10) * 100 / 90
    // v = 11 -> (11 - 10) * 100 / 90 = 1
    TEST_ASSERT_EQUAL_INT8(1, joystick_deadzone(11, 10));
    TEST_ASSERT_EQUAL_INT8(-1, joystick_deadzone(-11, 10));

    // v = 55 -> (55 - 10) * 100 / 90 = 4500 / 90 = 50
    TEST_ASSERT_EQUAL_INT8(50, joystick_deadzone(55, 10));
    TEST_ASSERT_EQUAL_INT8(-50, joystick_deadzone(-55, 10));
}

/* =========================================================================
 * 2. Testes do Misturador Diferencial (Mixer) - Seção 9.1
 * ========================================================================= */
void test_mixer(void)
{
    motor_cmd_t cmd;

    // (y=100, x=0) -> (100, 100)
    cmd = mixer_mix(0, 100);
    TEST_ASSERT_EQUAL_INT8(100, cmd.left);
    TEST_ASSERT_EQUAL_INT8(100, cmd.right);

    // (y=0, x=100) -> (100, -100)
    cmd = mixer_mix(100, 0);
    TEST_ASSERT_EQUAL_INT8(100, cmd.left);
    TEST_ASSERT_EQUAL_INT8(-100, cmd.right);

    // (y=100, x=100) -> clamp em (100, 0)
    cmd = mixer_mix(100, 100);
    TEST_ASSERT_EQUAL_INT8(100, cmd.left);
    TEST_ASSERT_EQUAL_INT8(0, cmd.right);

    // (y=-50, x=0) -> (-50, -50)
    cmd = mixer_mix(0, -50);
    TEST_ASSERT_EQUAL_INT8(-50, cmd.left);
    TEST_ASSERT_EQUAL_INT8(-50, cmd.right);

    // (y=0, x=-100) -> (-100, 100)
    cmd = mixer_mix(-100, 0);
    TEST_ASSERT_EQUAL_INT8(-100, cmd.left);
    TEST_ASSERT_EQUAL_INT8(100, cmd.right);

    // (y=-100, x=-100) -> clamp em (-100, 0)
    cmd = mixer_mix(-100, -100);
    TEST_ASSERT_EQUAL_INT8(-100, cmd.left);
    TEST_ASSERT_EQUAL_INT8(0, cmd.right);
}

/* =========================================================================
 * 3. Testes da Rampa de Aceleração (Ramp) - Seção 9.1
 * ========================================================================= */
void test_ramp(void)
{
    ramp_t r;
    ramp_reset(&r, 0);

    // Subida limitada pelo tempo: RAMP_UP_PCT_PER_S = 200%/s
    // t = 0 ms -> 0
    TEST_ASSERT_EQUAL_INT8(0, ramp_step(&r, 100, 0));

    // t = 100 ms -> 20%
    TEST_ASSERT_EQUAL_INT8(20, ramp_step(&r, 100, 100));

    // t = 250 ms -> 50%
    TEST_ASSERT_EQUAL_INT8(50, ramp_step(&r, 100, 250));

    // t = 500 ms -> 100% (atingiu o alvo)
    TEST_ASSERT_EQUAL_INT8(100, ramp_step(&r, 100, 500));

    // Descida imediata: de 100 para 30 sem esperar rampa
    TEST_ASSERT_EQUAL_INT8(30, ramp_step(&r, 30, 510));

    // Parada imediata em zero
    TEST_ASSERT_EQUAL_INT8(0, ramp_step(&r, 0, 520));

    // Troca de sinal passa por 0 e espera REVERSE_DWELL_MS (50 ms)
    // Acelera para +40%
    ramp_reset(&r, 1000);
    ramp_step(&r, 40, 1000);
    ramp_step(&r, 40, 1200); // 200 ms -> 40%
    TEST_ASSERT_EQUAL_INT8(40, r.current);

    // Inversão para -40% no timestamp 1300 ms
    // No instante da inversão, a saída vai imediatamente a 0 e entra em dwell
    TEST_ASSERT_EQUAL_INT8(0, ramp_step(&r, -40, 1300));
    TEST_ASSERT_TRUE(r.in_dwell);

    // Durante o dwell (ex: 20 ms depois -> 1320 ms), continua em 0
    TEST_ASSERT_EQUAL_INT8(0, ramp_step(&r, -40, 1320));

    // 49 ms depois (1349 ms) ainda está em dwell (< 50 ms)
    TEST_ASSERT_EQUAL_INT8(0, ramp_step(&r, -40, 1349));

    // 50 ms depois (1350 ms), dwell termina e começa a rampa no novo sentido
    int8_t out_dwell_end = ramp_step(&r, -40, 1350);
    TEST_ASSERT_EQUAL_INT8(0, out_dwell_end);
    TEST_ASSERT_FALSE(r.in_dwell);

    // 100 ms após o fim do dwell (1450 ms): aumentou 20% no sentido negativo (-20%)
    TEST_ASSERT_EQUAL_INT8(-20, ramp_step(&r, -40, 1450));

    // 200 ms após o fim do dwell (1550 ms): atinge -40%
    TEST_ASSERT_EQUAL_INT8(-40, ramp_step(&r, -40, 1550));
}

/* =========================================================================
 * 4. Testes do CRC-8 - Seção 9.1
 * ========================================================================= */
void test_crc8(void)
{
    // Vetor padrão conhecido para poly 0x07, init 0x00: ASCII "123456789" -> 0xF4
    const uint8_t test_vec[] = "123456789";
    TEST_ASSERT_EQUAL_HEX8(0xF4, crc8_calc(test_vec, 9));

    // Teste com cmd_packet_t
    cmd_packet_t pkt;
    pkt.magic = PROTO_MAGIC;
    pkt.type = PKT_CMD;
    pkt.seq = 1;
    pkt.x = 25;
    pkt.y = 50;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);

    // Validação correta
    uint8_t check = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_EQUAL_HEX8(pkt.crc, check);

    // Pacote com 1 bit trocado deve ser detectado
    uint8_t corrupted[sizeof(cmd_packet_t)];
    memcpy(corrupted, &pkt, sizeof(cmd_packet_t));
    corrupted[3] ^= 0x01; // inverte 1 bit no seq
    uint8_t bad_check = crc8_calc(corrupted, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_NOT_EQUAL(corrupted[sizeof(cmd_packet_t) - 1], bad_check);
}

/* =========================================================================
 * 5. Testes de Link e Validação de Pacotes (RF06, RF05) - Seção 9.1
 * ========================================================================= */
void test_link_validation(void)
{
    link_rx_t rx;
    uint8_t mac_expected[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    uint8_t mac_wrong[6]    = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x99 };
    cmd_packet_t pkt, out;

    link_init(&rx, mac_expected, 0);

    // Primeiro pacote após boot deve ser aceito mesmo que seq != 1
    pkt.magic = PROTO_MAGIC;
    pkt.type = PKT_CMD;
    pkt.seq = 100;
    pkt.x = 20;
    pkt.y = 40;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);

    TEST_ASSERT_EQUAL(LINK_OK, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 10, &out));
    TEST_ASSERT_EQUAL_UINT16(100, rx.last_seq);
    TEST_ASSERT_EQUAL_UINT16(0, rx.rejected_cnt);

    // Sequência crescente aceita (101 > 100)
    pkt.seq = 101;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_EQUAL(LINK_OK, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 30, &out));

    // Sequência repetida (101) deve ser rejeitada
    TEST_ASSERT_EQUAL(LINK_ERR_SEQ, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 50, &out));
    TEST_ASSERT_EQUAL_UINT16(1, rx.rejected_cnt);

    // Sequência antiga (99 < 101) deve ser rejeitada
    pkt.seq = 99;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_EQUAL(LINK_ERR_SEQ, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 70, &out));
    TEST_ASSERT_EQUAL_UINT16(2, rx.rejected_cnt);

    // Wraparound de 65535 para 0 deve ser aceito
    rx.last_seq = 65535;
    pkt.seq = 0;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_EQUAL(LINK_OK, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 90, &out));
    TEST_ASSERT_EQUAL_UINT16(0, rx.last_seq);

    // Valores fora de faixa: x = 120 rejeitado
    pkt.seq = 1;
    pkt.x = 120;
    pkt.y = 0;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_EQUAL(LINK_ERR_RANGE, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 110, &out));

    // Valores fora de faixa: y = -120 rejeitado
    pkt.seq = 2;
    pkt.x = 0;
    pkt.y = -120;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_EQUAL(LINK_ERR_RANGE, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 130, &out));

    // Origem incorreta rejeitada
    pkt.seq = 3;
    pkt.x = 0;
    pkt.y = 50;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_EQUAL(LINK_ERR_SRC_MAC, link_check_cmd(&rx, mac_wrong, (const uint8_t *)&pkt, sizeof(pkt), 150, &out));

    // Magic incorreto rejeitado
    pkt.magic = 0x5A;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    TEST_ASSERT_EQUAL(LINK_ERR_MAGIC, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 170, &out));
    pkt.magic = PROTO_MAGIC;

    // CRC incorreto rejeitado
    pkt.seq = 4;
    pkt.crc = 0x00; // CRC errado
    TEST_ASSERT_EQUAL(LINK_ERR_CRC, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt), 190, &out));

    // CRC: pacote válido com 1 bit trocado é rejeitado pelo link e não renova o enlace
    pkt.seq = 5;
    pkt.x = 0;
    pkt.y = 30;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);
    uint8_t flipped[sizeof(cmd_packet_t)];
    memcpy(flipped, &pkt, sizeof(pkt));
    flipped[5] ^= 0x04; // 1 bit em y
    uint32_t last_rx_before = rx.last_rx_ms;
    TEST_ASSERT_EQUAL(LINK_ERR_CRC, link_check_cmd(&rx, mac_expected, flipped, sizeof(flipped), 200, &out));
    TEST_ASSERT_EQUAL_UINT32(last_rx_before, rx.last_rx_ms);

    // Tamanho errado rejeitado
    TEST_ASSERT_EQUAL(LINK_ERR_SIZE, link_check_cmd(&rx, mac_expected, (const uint8_t *)&pkt, sizeof(pkt) - 1, 210, &out));

    // Timeout de enlace (RF05, Seção 9.1): aos 299 ms vivo; aos 300 ms perdido (FALHA_COM)
    rx.last_rx_ms = 1000;
    TEST_ASSERT_TRUE(link_is_alive(&rx, 1299));  // 299 ms -> link_ok = true
    TEST_ASSERT_FALSE(link_is_alive(&rx, 1300)); // 300 ms -> link_ok = false
}

/* Helper: monta um cmd_packet_t válido */
static cmd_packet_t make_cmd(uint16_t seq, int8_t x, int8_t y)
{
    cmd_packet_t p;
    p.magic = PROTO_MAGIC;
    p.type = PKT_CMD;
    p.seq = seq;
    p.x = x;
    p.y = y;
    p.crc = crc8_calc((const uint8_t *)&p, sizeof(cmd_packet_t) - 1);
    return p;
}

/* Boot sem controle (A.1 item 8): enlace perdido aos 300 ms contados do boot */
void test_link_boot_without_controller(void)
{
    link_rx_t rx;
    uint8_t mac[6] = { 1, 2, 3, 4, 5, 6 };
    link_init(&rx, mac, 1000);   // boot_time_ms = 1000

    TEST_ASSERT_TRUE(link_is_alive(&rx, 1000));
    TEST_ASSERT_TRUE(link_is_alive(&rx, 1299));
    TEST_ASSERT_FALSE(link_is_alive(&rx, 1300));
    TEST_ASSERT_FALSE(link_is_alive(&rx, 5000));
}

/* Controle reiniciado (A.1 item 1, teste T1): depois da perda de enlace, o
 * controle volta a contar seq do início e deve ser aceito. */
void test_link_controller_restart(void)
{
    link_rx_t rx;
    uint8_t mac[6] = { 1, 2, 3, 4, 5, 6 };
    cmd_packet_t out;
    link_init(&rx, mac, 0);

    // Controle enviando há tempo: seq alto
    cmd_packet_t p = make_cmd(1000, 0, 50);
    TEST_ASSERT_EQUAL(LINK_OK, link_check_cmd(&rx, mac, (const uint8_t *)&p, sizeof(p), 100, &out));

    // Com enlace vivo, seq baixo continua sendo "sequência antiga"
    p = make_cmd(1, 0, 0);
    TEST_ASSERT_EQUAL(LINK_ERR_SEQ, link_check_cmd(&rx, mac, (const uint8_t *)&p, sizeof(p), 150, &out));
    TEST_ASSERT_EQUAL_UINT16(1, rx.rejected_cnt);

    // Controle desligado: enlace cai aos 300 ms sem pacote
    TEST_ASSERT_FALSE(link_is_alive(&rx, 400));

    // Controle religado: recomeça do seq 1 e é aceito como primeiro pacote
    p = make_cmd(1, 0, 0);
    TEST_ASSERT_EQUAL(LINK_OK, link_check_cmd(&rx, mac, (const uint8_t *)&p, sizeof(p), 2000, &out));
    TEST_ASSERT_EQUAL_UINT16(1, rx.last_seq);
    TEST_ASSERT_EQUAL_UINT16(1, rx.rejected_cnt);
    TEST_ASSERT_TRUE(link_is_alive(&rx, 2000));

    // A partir daí a sequência volta a ser verificada normalmente
    p = make_cmd(2, 0, 0);
    TEST_ASSERT_EQUAL(LINK_OK, link_check_cmd(&rx, mac, (const uint8_t *)&p, sizeof(p), 2050, &out));
    TEST_ASSERT_EQUAL(LINK_ERR_SEQ, link_check_cmd(&rx, mac, (const uint8_t *)&p, sizeof(p), 2100, &out));

    // Pacote corrompido depois da queda não reabre o enlace
    TEST_ASSERT_FALSE(link_is_alive(&rx, 3000));
    cmd_packet_t bad = make_cmd(1, 0, 0);
    bad.crc ^= 0xFF;
    TEST_ASSERT_EQUAL(LINK_ERR_CRC, link_check_cmd(&rx, mac, (const uint8_t *)&bad, sizeof(bad), 3010, &out));
    TEST_ASSERT_FALSE(link_is_alive(&rx, 3010));
}

/* =========================================================================
 * Integração link + máquina de estados, em ticks de CONTROL_PERIOD_MS
 * ========================================================================= */
typedef struct {
    link_rx_t     rx;
    car_sm_t      sm;
    car_outputs_t out;
    uint8_t       mac[6];
} sim_t;

static void sim_init(sim_t *s, uint32_t boot_ms)
{
    const uint8_t mac[6] = { 1, 2, 3, 4, 5, 6 };
    memcpy(s->mac, mac, sizeof(mac));
    link_init(&s->rx, s->mac, boot_ms);
    car_sm_init(&s->sm);
    car_sm_set_ready(&s->sm);
}

/* Um tick do laço de controle, opcionalmente com um pacote recebido */
static void sim_tick(sim_t *s, uint32_t now_ms, const cmd_packet_t *pkt)
{
    car_inputs_t in;
    memset(&in, 0, sizeof(in));
    cmd_packet_t cmd;
    if (pkt != NULL &&
        link_check_cmd(&s->rx, s->mac, (const uint8_t *)pkt, sizeof(*pkt), now_ms, &cmd) == LINK_OK) {
        in.has_cmd = true;
        in.cmd_x = cmd.x;
        in.cmd_y = cmd.y;
    }
    in.link_ok = link_is_alive(&s->rx, now_ms);
    in.now_ms = now_ms;
    car_sm_step(&s->sm, &in, &s->out);
}

/* Boot sem controle vai a FALHA_COM aos 300 ms e não aos 299 ms */
void test_sm_boot_without_controller(void)
{
    sim_t s;
    sim_init(&s, 0);
    for (uint32_t t = CONTROL_PERIOD_MS; t < LINK_TIMEOUT_MS; t += CONTROL_PERIOD_MS) {
        sim_tick(&s, t, NULL);
        TEST_ASSERT_EQUAL(ST_PARADO, s.sm.state);
    }
    sim_tick(&s, 299, NULL);
    TEST_ASSERT_EQUAL(ST_PARADO, s.sm.state);
    sim_tick(&s, 300, NULL);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, s.sm.state);
    TEST_ASSERT_TRUE(s.out.brake_immediate);
}

/* Wi-Fi lento (A.1 item 8): o primeiro tick só ocorre depois de 300 ms do
 * boot -> já entra em FALHA_COM e só sai com o joystick no centro. */
void test_sm_slow_wifi_boot(void)
{
    sim_t s;
    sim_init(&s, 0);
    sim_tick(&s, 350, NULL);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, s.sm.state);

    cmd_packet_t p = make_cmd(1, 0, 60);   // controle já com o joystick à frente
    sim_tick(&s, 360, &p);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, s.sm.state);
    TEST_ASSERT_TRUE(s.out.brake_immediate);

    p = make_cmd(2, 0, 0);
    sim_tick(&s, 410, &p);
    TEST_ASSERT_EQUAL(ST_PARADO, s.sm.state);
}

/* T1 completo: em movimento, controle some, volta reiniciado; só retoma com centro */
void test_sm_controller_restart(void)
{
    sim_t s;
    sim_init(&s, 0);
    uint16_t seq = 500;
    cmd_packet_t p;

    p = make_cmd(seq++, 0, 0);
    sim_tick(&s, 10, &p);                  // arma
    p = make_cmd(seq++, 0, 80);
    sim_tick(&s, 60, &p);
    TEST_ASSERT_EQUAL(ST_OPERANDO, s.sm.state);

    // Controle desligado: FALHA_COM aos 300 ms do último pacote
    sim_tick(&s, 350, NULL);
    TEST_ASSERT_EQUAL(ST_OPERANDO, s.sm.state);
    sim_tick(&s, 360, NULL);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, s.sm.state);
    TEST_ASSERT_TRUE(s.out.brake_immediate);

    // Controle religado com seq do início e joystick fora do centro: continua parado
    p = make_cmd(1, 0, 80);
    sim_tick(&s, 5000, &p);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, s.sm.state);
    TEST_ASSERT_TRUE(s.out.brake_immediate);
    TEST_ASSERT_TRUE(link_is_alive(&s.rx, 5000));   // enlace voltou (pacote aceito)

    // Joystick no centro: rearma
    p = make_cmd(2, 0, 0);
    sim_tick(&s, 5050, &p);
    TEST_ASSERT_EQUAL(ST_PARADO, s.sm.state);
    TEST_ASSERT_TRUE(s.sm.armed);

    p = make_cmd(3, 0, 50);
    sim_tick(&s, 5100, &p);
    TEST_ASSERT_EQUAL(ST_OPERANDO, s.sm.state);
}

/* =========================================================================
 * 6. Testes da Máquina de Estados (State Machine) - Seção 9.1
 * ========================================================================= */
void test_state_machine(void)
{
    car_sm_t sm;
    car_inputs_t in;
    car_outputs_t out;

    car_sm_init(&sm);
    TEST_ASSERT_EQUAL(ST_INIT, sm.state);
    TEST_ASSERT_FALSE(sm.armed);

    // Boot: passar autoteste -> vai para ST_PARADO desarmado
    car_sm_set_ready(&sm);
    TEST_ASSERT_EQUAL(ST_PARADO, sm.state);
    TEST_ASSERT_FALSE(sm.armed);

    // Boot sem controle por 301 ms vai para ST_FALHA_COM
    memset(&in, 0, sizeof(in));
    in.link_ok = false; // > 300 ms
    in.now_ms = 301;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, sm.state);
    TEST_ASSERT_TRUE(out.brake_immediate);

    // Saída de ST_FALHA_COM exige link_ok e comando neutro (x=0, y=0)
    // Se link_ok mas com joystick fora do centro (y=50), continua em FALHA_COM
    in.link_ok = true;
    in.has_cmd = true;
    in.cmd_x = 0;
    in.cmd_y = 50;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, sm.state);
    TEST_ASSERT_TRUE(sm.rejected_cmd_cnt > 0);

    // Quando envia neutro (x=0, y=0), rearma e vai para ST_PARADO
    in.cmd_x = 0;
    in.cmd_y = 0;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_PARADO, sm.state);
    TEST_ASSERT_TRUE(sm.armed);

    // Em ST_PARADO armado, comando válido leva a ST_OPERANDO
    in.cmd_x = 0;
    in.cmd_y = 50;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OPERANDO, sm.state);
    TEST_ASSERT_FALSE(out.brake_immediate);
    TEST_ASSERT_EQUAL_INT8(50, out.target_left);
    TEST_ASSERT_EQUAL_INT8(50, out.target_right);

    // Em ST_OPERANDO, retornar joystick para neutro leva a ST_PARADO com parada imediata
    in.cmd_x = 0;
    in.cmd_y = 0;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_PARADO, sm.state);
    TEST_ASSERT_TRUE(out.brake_immediate);

    // Histerese de Obstáculo (25 cm / 30 cm)
    // Coloca em movimento
    in.cmd_x = 0;
    in.cmd_y = 50;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OPERANDO, sm.state);

    // Leitura válida de 25 cm -> entra em ST_OBSTACULO
    in.has_sensor_data = true;
    in.echo_ok = true;
    in.distance_cm = 25;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    TEST_ASSERT_TRUE(sm.obstacle);
    TEST_ASSERT_TRUE(out.brake_immediate);

    // Em ST_OBSTACULO: avanço (y > 0) é bloqueado e rejeitado
    uint16_t rej_before = sm.rejected_cmd_cnt;
    in.has_sensor_data = false;
    in.has_cmd = true;
    in.cmd_x = 0;
    in.cmd_y = 60;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    TEST_ASSERT_EQUAL_INT8(0, out.target_left);
    TEST_ASSERT_EQUAL_INT8(0, out.target_right);
    TEST_ASSERT_EQUAL_UINT16(rej_before + 1, sm.rejected_cmd_cnt);

    // Ré (y < 0) e giro são permitidos em ST_OBSTACULO
    in.cmd_x = 0;
    in.cmd_y = -40;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    TEST_ASSERT_EQUAL_INT8(-40, out.target_left);
    TEST_ASSERT_EQUAL_INT8(-40, out.target_right);

    // Histerese: Leitura de 28 cm (> 25 cm mas <= 30 cm) -> permanece em obstáculo
    in.has_sensor_data = true;
    in.echo_ok = true;
    in.distance_cm = 28;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_TRUE(sm.obstacle);

    // Leitura de 35 cm (ciclo 1 > 30 cm) -> ainda não sai (precisa de 3 leituras válidas)
    in.distance_cm = 35;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_TRUE(sm.obstacle);

    // Ciclo 2 > 30 cm
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_TRUE(sm.obstacle);

    // Ciclo 3 > 30 cm -> obstáculo resolvido!
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_FALSE(sm.obstacle);

    // Mas para sair de ST_OBSTACULO para ST_PARADO exige joystick centrado
    in.has_sensor_data = false;
    in.cmd_x = 0;
    in.cmd_y = 0;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_PARADO, sm.state);

    // Sensor inválido após 3 ciclos sem eco (RF04)
    in.has_sensor_data = true;
    in.echo_ok = false; // sem eco
    in.distance_cm = 0xFFFF;
    car_sm_step(&sm, &in, &out); // Falha 1
    TEST_ASSERT_TRUE(sm.sensor_valid);
    car_sm_step(&sm, &in, &out); // Falha 2
    TEST_ASSERT_TRUE(sm.sensor_valid);
    car_sm_step(&sm, &in, &out); // Falha 3 -> sensor inválido!
    TEST_ASSERT_FALSE(sm.sensor_valid);
    TEST_ASSERT_TRUE(sm.obstacle); // Sensor inválido ativa bloqueio de obstáculo!

    // Recuperação após 3 leituras válidas
    in.echo_ok = true;
    in.distance_cm = 50;
    car_sm_step(&sm, &in, &out); // Válida 1
    TEST_ASSERT_FALSE(sm.sensor_valid);
    car_sm_step(&sm, &in, &out); // Válida 2
    TEST_ASSERT_FALSE(sm.sensor_valid);
    car_sm_step(&sm, &in, &out); // Válida 3 -> sensor recuperado!
    TEST_ASSERT_TRUE(sm.sensor_valid);
    TEST_ASSERT_FALSE(sm.obstacle);
}

/* =========================================================================
 * 7. Máquina de estados: transições restantes da tabela 8.3
 * ========================================================================= */

/* Helpers para entradas da máquina de estados */
static car_inputs_t in_cmd(int8_t x, int8_t y)
{
    car_inputs_t in;
    memset(&in, 0, sizeof(in));
    in.link_ok = true;
    in.has_cmd = true;
    in.cmd_x = x;
    in.cmd_y = y;
    return in;
}

static car_inputs_t in_sensor(uint16_t dist_cm, bool echo_ok)
{
    car_inputs_t in;
    memset(&in, 0, sizeof(in));
    in.link_ok = true;
    in.has_sensor_data = true;
    in.echo_ok = echo_ok;
    in.distance_cm = dist_cm;
    return in;
}

/* Leva a máquina até OPERANDO com (x, y) */
static void sm_to_operando(car_sm_t *sm, car_outputs_t *out, int8_t x, int8_t y)
{
    car_sm_init(sm);
    car_sm_set_ready(sm);
    car_inputs_t in = in_cmd(0, 0);
    car_sm_step(sm, &in, out);   // arma
    in = in_cmd(x, y);
    car_sm_step(sm, &in, out);
    TEST_ASSERT_EQUAL(ST_OPERANDO, sm->state);
}

/* INIT: motores parados e todos os comandos rejeitados */
void test_sm_init_rejects_all(void)
{
    car_sm_t sm;
    car_outputs_t out;
    car_sm_init(&sm);

    car_inputs_t in = in_cmd(0, 80);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_INIT, sm.state);
    TEST_ASSERT_TRUE(out.brake_immediate);
    TEST_ASSERT_EQUAL_UINT16(1, sm.rejected_cmd_cnt);

    // Mesmo sem enlace, INIT não vai a FALHA_COM (só "qualquer != INIT")
    in.link_ok = false;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_INIT, sm.state);
}

/* Boot com joystick fora do centro só arma depois do neutro */
void test_sm_boot_off_center_arms_after_neutral(void)
{
    car_sm_t sm;
    car_outputs_t out;
    car_sm_init(&sm);
    car_sm_set_ready(&sm);

    car_inputs_t in = in_cmd(0, 70);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_PARADO, sm.state);
    TEST_ASSERT_FALSE(sm.armed);
    TEST_ASSERT_TRUE(out.brake_immediate);
    TEST_ASSERT_EQUAL_UINT16(1, sm.rejected_cmd_cnt);

    car_sm_step(&sm, &in, &out);   // continua ignorado
    TEST_ASSERT_EQUAL(ST_PARADO, sm.state);
    TEST_ASSERT_EQUAL_UINT16(2, sm.rejected_cmd_cnt);

    in = in_cmd(0, 0);             // neutro arma, mas não anda
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_PARADO, sm.state);
    TEST_ASSERT_TRUE(sm.armed);

    in = in_cmd(0, 70);            // agora sim
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OPERANDO, sm.state);
}

/* OPERANDO -> FALHA_COM e OBSTACULO -> FALHA_COM */
void test_sm_link_lost_from_any_state(void)
{
    car_sm_t sm;
    car_outputs_t out;
    car_inputs_t in;

    sm_to_operando(&sm, &out, 0, 60);
    memset(&in, 0, sizeof(in));
    in.link_ok = false;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, sm.state);
    TEST_ASSERT_TRUE(out.brake_immediate);
    TEST_ASSERT_EQUAL_INT8(0, out.target_left);
    TEST_ASSERT_EQUAL_INT8(0, out.target_right);

    sm_to_operando(&sm, &out, 0, 60);
    in = in_sensor(10, true);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    memset(&in, 0, sizeof(in));
    in.link_ok = false;
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, sm.state);
    TEST_ASSERT_TRUE(out.brake_immediate);

    // FALHA_COM: comando fora do centro rejeitado, mesmo com enlace de volta
    uint16_t rej = sm.rejected_cmd_cnt;
    in = in_cmd(30, 0);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_FALHA_COM, sm.state);
    TEST_ASSERT_EQUAL_UINT16(rej + 1, sm.rejected_cmd_cnt);
}

/* PARADO -> OBSTACULO e saída só com centered */
void test_sm_obstacle_from_parado_and_exit(void)
{
    car_sm_t sm;
    car_outputs_t out;
    car_sm_init(&sm);
    car_sm_set_ready(&sm);

    car_inputs_t in = in_sensor(20, true);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    TEST_ASSERT_TRUE(out.brake_immediate);

    // Joystick em ré: liberado
    in = in_cmd(0, -50);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    TEST_ASSERT_EQUAL_INT8(-50, out.target_left);

    // Obstáculo some (3 leituras > 30 cm) com o joystick ainda em ré: continua em OBSTACULO
    for (int i = 0; i < OBSTACLE_EXIT_CYCLES; i++) {
        in = in_sensor(80, true);
        car_sm_step(&sm, &in, &out);
    }
    TEST_ASSERT_FALSE(sm.obstacle);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);

    // Só sai com o joystick no centro
    in = in_cmd(0, 0);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_PARADO, sm.state);
    TEST_ASSERT_TRUE(out.brake_immediate);
}

/* OBSTACULO: avanço bloqueado (y = min(y, 0)), giro liberado */
void test_sm_obstacle_turn_allowed(void)
{
    car_sm_t sm;
    car_outputs_t out;
    sm_to_operando(&sm, &out, 0, 60);
    car_inputs_t in = in_sensor(OBSTACLE_ENTER_CM, true);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    TEST_ASSERT_TRUE(out.brake_immediate);   // entra parando imediatamente

    // Frente + direita: só o giro passa -> mix(60, 0) = (60, -60)
    uint16_t rej = sm.rejected_cmd_cnt;
    in = in_cmd(60, 50);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL_INT8(60, out.target_left);
    TEST_ASSERT_EQUAL_INT8(-60, out.target_right);
    TEST_ASSERT_EQUAL_UINT16(rej + 1, sm.rejected_cmd_cnt);

    // Giro puro no lugar: liberado, sem rejeição
    in = in_cmd(-40, 0);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL_INT8(-40, out.target_left);
    TEST_ASSERT_EQUAL_INT8(40, out.target_right);
    TEST_ASSERT_EQUAL_UINT16(rej + 1, sm.rejected_cmd_cnt);
}

/* Histerese 25/30 cm: 26 cm não entra; 25 entra; 30 não sai; > 30 x3 sai */
void test_sm_obstacle_hysteresis_edges(void)
{
    car_sm_t sm;
    car_outputs_t out;
    sm_to_operando(&sm, &out, 0, 40);

    car_inputs_t in = in_sensor(OBSTACLE_ENTER_CM + 1, true);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OPERANDO, sm.state);
    TEST_ASSERT_FALSE(sm.obstacle);

    in = in_sensor(OBSTACLE_ENTER_CM, true);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);

    for (int i = 0; i < 5; i++) {
        in = in_sensor(OBSTACLE_EXIT_CM, true);   // exatamente 30: não conta como livre
        car_sm_step(&sm, &in, &out);
    }
    TEST_ASSERT_TRUE(sm.obstacle);

    // Duas leituras livres e uma de 30 zeram a contagem
    in = in_sensor(OBSTACLE_EXIT_CM + 1, true);
    car_sm_step(&sm, &in, &out);
    car_sm_step(&sm, &in, &out);
    in = in_sensor(OBSTACLE_EXIT_CM, true);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_TRUE(sm.obstacle);

    in = in_sensor(OBSTACLE_EXIT_CM + 1, true);
    for (int i = 0; i < OBSTACLE_EXIT_CYCLES - 1; i++) {
        car_sm_step(&sm, &in, &out);
        TEST_ASSERT_TRUE(sm.obstacle);
    }
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_FALSE(sm.obstacle);
}

/* T5: sensor desconectado em avanço -> 3 leituras sem eco bloqueiam (OPERANDO -> OBSTACULO);
 * leituras fora de faixa (< 2 cm, > 400 cm) também contam como inválidas */
void test_sm_sensor_invalid_blocks_forward(void)
{
    car_sm_t sm;
    car_outputs_t out;
    sm_to_operando(&sm, &out, 0, 60);

    car_inputs_t in = in_sensor(PROTO_DIST_INVALID, false);
    car_sm_step(&sm, &in, &out);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OPERANDO, sm.state);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    TEST_ASSERT_FALSE(out.sensor_valid);

    // Fora de faixa também é inválido
    sm_to_operando(&sm, &out, 0, 60);
    in = in_sensor(SENSOR_VALID_MAX_CM + 1, true);
    car_sm_step(&sm, &in, &out);
    in = in_sensor(SENSOR_VALID_MIN_CM - 1, true);
    car_sm_step(&sm, &in, &out);
    car_sm_step(&sm, &in, &out);
    TEST_ASSERT_EQUAL(ST_OBSTACULO, sm.state);
    TEST_ASSERT_FALSE(out.sensor_valid);
}

/* =========================================================================
 * 8. Controle: validação de telemetria e LED (Seção 7)
 * ========================================================================= */
static telem_packet_t make_telem(uint8_t state)
{
    telem_packet_t t;
    t.magic = PROTO_MAGIC;
    t.type = PKT_TELEM;
    t.seq = 7;
    t.state = state;
    t.reserved = 0;
    t.distance_cm = 42;
    t.battery_mv = 7430;
    t.rejected_cnt = 3;
    t.crc = crc8_calc((const uint8_t *)&t, sizeof(t) - 1);
    return t;
}

void test_telem_check(void)
{
    const uint8_t car[6]   = { 9, 8, 7, 6, 5, 4 };
    const uint8_t other[6] = { 9, 8, 7, 6, 5, 3 };
    telem_packet_t t = make_telem(ST_OPERANDO), out;

    TEST_ASSERT_EQUAL(TELEM_OK, telem_check(car, car, (const uint8_t *)&t, sizeof(t), &out));
    TEST_ASSERT_EQUAL_UINT8(ST_OPERANDO, out.state);
    TEST_ASSERT_EQUAL_UINT16(42, out.distance_cm);
    TEST_ASSERT_EQUAL_UINT16(7430, out.battery_mv);
    TEST_ASSERT_EQUAL_UINT16(3, out.rejected_cnt);

    TEST_ASSERT_EQUAL(TELEM_ERR_SRC_MAC, telem_check(car, other, (const uint8_t *)&t, sizeof(t), &out));
    TEST_ASSERT_EQUAL(TELEM_ERR_SIZE, telem_check(car, car, (const uint8_t *)&t, sizeof(t) - 1, &out));

    // Um comando (PKT_CMD) não é telemetria
    cmd_packet_t c = make_cmd(1, 0, 0);
    TEST_ASSERT_EQUAL(TELEM_ERR_SIZE, telem_check(car, car, (const uint8_t *)&c, sizeof(c), &out));

    telem_packet_t bad = t;
    bad.magic = 0x5A;
    bad.crc = crc8_calc((const uint8_t *)&bad, sizeof(bad) - 1);
    TEST_ASSERT_EQUAL(TELEM_ERR_MAGIC, telem_check(car, car, (const uint8_t *)&bad, sizeof(bad), &out));

    bad = t;
    bad.type = PKT_CMD;
    bad.crc = crc8_calc((const uint8_t *)&bad, sizeof(bad) - 1);
    TEST_ASSERT_EQUAL(TELEM_ERR_TYPE, telem_check(car, car, (const uint8_t *)&bad, sizeof(bad), &out));

    bad = t;
    ((uint8_t *)&bad)[6] ^= 0x01;   // 1 bit trocado em distance_cm
    TEST_ASSERT_EQUAL(TELEM_ERR_CRC, telem_check(car, car, (const uint8_t *)&bad, sizeof(bad), &out));

    bad = make_telem(ST_FALHA_COM + 1);
    TEST_ASSERT_EQUAL(TELEM_ERR_STATE, telem_check(car, car, (const uint8_t *)&bad, sizeof(bad), &out));

    TEST_ASSERT_EQUAL_STRING("OPERANDO", telem_state_name(ST_OPERANDO));
    TEST_ASSERT_EQUAL_STRING("FALHA_COM", telem_state_name(ST_FALHA_COM));
}

/* Conta as bordas de subida do LED em 1 s, amostrando a cada 1 ms */
static int led_rising_edges(bool has_telem, uint8_t state, uint32_t start_ms)
{
    int edges = 0;
    bool prev = led_pattern_level(has_telem, state, start_ms);
    for (uint32_t t = start_ms + 1; t <= start_ms + 1000; t++) {
        bool cur = led_pattern_level(has_telem, state, t);
        if (cur && !prev) {
            edges++;
        }
        prev = cur;
    }
    return edges;
}

void test_led_pattern(void)
{
    // Sem telemetria: sempre apagado
    for (uint32_t t = 0; t < 2000; t += 7) {
        TEST_ASSERT_FALSE(led_pattern_level(false, ST_PARADO, t));
        TEST_ASSERT_FALSE(led_pattern_level(false, ST_OPERANDO, t));
    }
    // PARADO: aceso fixo
    for (uint32_t t = 0; t < 2000; t += 7) {
        TEST_ASSERT_TRUE(led_pattern_level(true, ST_PARADO, t));
    }
    // OPERANDO: 1 Hz (1 subida por segundo); OBSTACULO: 4 Hz; FALHA_COM: 2 piscadas por segundo
    TEST_ASSERT_EQUAL_INT(1, led_rising_edges(true, ST_OPERANDO, 0));
    TEST_ASSERT_EQUAL_INT(4, led_rising_edges(true, ST_OBSTACULO, 0));
    TEST_ASSERT_EQUAL_INT(2, led_rising_edges(true, ST_FALHA_COM, 0));
    TEST_ASSERT_EQUAL_INT(0, led_rising_edges(true, ST_PARADO, 0));

    // OPERANDO: metade do período aceso
    TEST_ASSERT_TRUE(led_pattern_level(true, ST_OPERANDO, 0));
    TEST_ASSERT_TRUE(led_pattern_level(true, ST_OPERANDO, 499));
    TEST_ASSERT_FALSE(led_pattern_level(true, ST_OPERANDO, 500));

    // FALHA_COM: aceso 0-99, apagado 100-199, aceso 200-299, apagado até 999
    TEST_ASSERT_TRUE(led_pattern_level(true, ST_FALHA_COM, 50));
    TEST_ASSERT_FALSE(led_pattern_level(true, ST_FALHA_COM, 150));
    TEST_ASSERT_TRUE(led_pattern_level(true, ST_FALHA_COM, 250));
    TEST_ASSERT_FALSE(led_pattern_level(true, ST_FALHA_COM, 600));
}

static void run_all_tests(void)
{
    RUN_TEST(test_joystick_normalize);
    RUN_TEST(test_joystick_deadzone);
    RUN_TEST(test_mixer);
    RUN_TEST(test_ramp);
    RUN_TEST(test_crc8);
    RUN_TEST(test_link_validation);
    RUN_TEST(test_link_boot_without_controller);
    RUN_TEST(test_link_controller_restart);
    RUN_TEST(test_sm_boot_without_controller);
    RUN_TEST(test_sm_slow_wifi_boot);
    RUN_TEST(test_sm_controller_restart);
    RUN_TEST(test_state_machine);
    RUN_TEST(test_sm_init_rejects_all);
    RUN_TEST(test_sm_boot_off_center_arms_after_neutral);
    RUN_TEST(test_sm_link_lost_from_any_state);
    RUN_TEST(test_sm_obstacle_from_parado_and_exit);
    RUN_TEST(test_sm_obstacle_turn_allowed);
    RUN_TEST(test_sm_obstacle_hysteresis_edges);
    RUN_TEST(test_sm_sensor_invalid_blocks_forward);
    RUN_TEST(test_telem_check);
    RUN_TEST(test_led_pattern);
}

#ifdef ESP_PLATFORM
void app_main(void)
{
    UNITY_BEGIN();
    run_all_tests();
    UNITY_END();
}
#else
int main(void)
{
    UNITY_BEGIN();
    run_all_tests();
    return UNITY_END();
}
#endif
