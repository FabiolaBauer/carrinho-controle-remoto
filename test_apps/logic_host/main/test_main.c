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

    // Timeout de enlace (RF05): aos 300 ms ainda está vivo; aos 301 ms está perdido
    rx.last_rx_ms = 1000;
    TEST_ASSERT_TRUE(link_is_alive(&rx, 1299)); // 299 ms -> link_ok = true
    TEST_ASSERT_TRUE(link_is_alive(&rx, 1300)); // 300 ms -> link_ok = true
    TEST_ASSERT_FALSE(link_is_alive(&rx, 1301)); // 301 ms -> link_ok = false (FALHA_COM)
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

#ifdef ESP_PLATFORM
void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_joystick_normalize);
    RUN_TEST(test_joystick_deadzone);
    RUN_TEST(test_mixer);
    RUN_TEST(test_ramp);
    RUN_TEST(test_crc8);
    RUN_TEST(test_link_validation);
    RUN_TEST(test_state_machine);
    UNITY_END();
}
#else
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_joystick_normalize);
    RUN_TEST(test_joystick_deadzone);
    RUN_TEST(test_mixer);
    RUN_TEST(test_ramp);
    RUN_TEST(test_crc8);
    RUN_TEST(test_link_validation);
    RUN_TEST(test_state_machine);
    return UNITY_END();
}
#endif
